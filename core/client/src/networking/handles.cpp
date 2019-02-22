#include "stdafx_client.h"
#include "pragma/clientstate/clientutil.h"
#include <fsys/filesystem.h>
#include "pragma/c_engine.h"
#include "pragma/networking/netmessages.h"
#include <pragma/networking/nwm_util.h>
#include "pragma/networking/wvclient.h"
#include <sharedutils/util_string.h>
#include "pragma/entities/components/c_entity_component.hpp"
#include "pragma/entities/c_world.h"
#include <pragma/networking/resources.h>
#include <pragma/lua/lua_script_watcher.h>
#include <pragma/util/resource_watcher.h>
#include <pragma/game/gamemode/gamemodemanager.h>
#include <pragma/entities/components/map_component.hpp>

extern "C" {
	#include "bzlib.h"
}

extern DLLCENGINE CEngine *c_engine;
extern DLLCLIENT CGame *c_game;

void ClientState::HandlePacket(NetPacket &packet)
{
	packet->SetClient(true);
	CallCallbacks<void,std::reference_wrapper<NetPacket>>("OnReceivePacket",packet);
	unsigned int ID = packet.GetMessageID();
	CLNetMessage *msg = GetNetMessage(ID);
	if(msg == NULL)
	{
		Con::cwar<<"WARNING: (CLIENT) Unhandled net message: "<<ID<<Con::endl;
		return;
	}
	// packet->SetClient(true); // WVTODO
	msg->handler(packet);
}

void ClientState::HandleConnect()
{
	RequestServerInfo();
}

void ClientState::RequestServerInfo()
{
	Con::ccl<<"[CLIENT] Sending serverinfo request..."<<Con::endl;
	NetPacket packet;
	packet->WriteString(GetConVarString("password"));
	SendPacketTCP("serverinfo_request",packet);
}

void ClientState::HandleClientReceiveServerInfo(NetPacket &packet)
{
	m_svInfo = std::make_unique<ServerInfo>();
	if(IsConnected())
		m_svInfo->address = m_client->GetIP();
	else
		m_svInfo->address = "[::1]";
	if(packet->Read<unsigned char>() == 1)
		m_svInfo->portUDP = packet->Read<unsigned short>();
	else m_svInfo->portUDP = 0;

	auto svPath = m_svInfo->address;
	ustring::replace(svPath,":","_");
	ustring::replace(svPath,"[","");
	ustring::replace(svPath,"]","");
	m_svInfo->SetDownloadPath("downloads\\" +svPath +'\\');

	auto luaPath = m_svInfo->GetDownloadPath() +"lua";
	FileManager::CreatePath(luaPath.c_str());

	unsigned int numResources = packet->Read<unsigned int>();
	Con::ccl<<"Downloading "<<numResources<<" files from server..."<<Con::endl;
	
	StartResourceTransfer();
}

void ClientState::LoadLuaCache(std::string cache,unsigned int cacheSize)
{
	std::string path = "cache\\" +cache +".cache";
	auto f = FileManager::OpenFile(path.c_str(),"rb");
	if(f == NULL)
	{
		Con::cout<<"WARNING: Unable to open lua-cache file!"<<Con::endl;
		return;
	}
	unsigned int sourceLength = CUInt32(f->GetSize());
	int verbosity = 0;
	int small = 0;
	char *dest = new char[cacheSize];
	char *source = new char[sourceLength];
	f->Read(source,sourceLength);
	int err = BZ2_bzBuffToBuffDecompress(dest,&cacheSize,source,sourceLength,small,verbosity);
	if(err == BZ_OK)
	{
		unsigned int offset = 0;
		while(offset < cacheSize)
		{
			std::string path = ustring::read_until_etx(offset +dest);
			offset += CUInt32(path.size()) +1;
			std::string content = ustring::read_until_etx(dest +offset);
			offset += CUInt32(content.length()) +1;
			auto data = std::make_shared<std::vector<uint8_t>>();
			data->resize(content.length() +1);
			memcpy(data->data(),content.c_str(),content.length() +1);
			FileManager::AddVirtualFile(("lua\\" +path).c_str(),data);
			//int s = luaL_dostring(m_lua,content.c_str());
			//lua_err(m_lua,s);
		}
	}
	else
		Con::cwar<<"WARNING: Unable to decompress lua-cache ("<<err<<")!"<<Con::endl;
	delete[] dest;
	delete[] source;
}

extern CBaseEntity *NET_cl_ent_create(NetPacket &packet,bool bSpawn,bool bIgnoreMapInit=false);
extern CBaseEntity *NET_cl_ent_create_lua(NetPacket &packet,bool bSpawn,bool bIgnoreMapInit=false);

void ClientState::ReadEntityData(NetPacket &packet)
{
	unsigned int numEnts = packet->Read<unsigned int>();
	std::vector<EntityHandle> ents;
	ents.reserve(numEnts);
	for(unsigned int i=0;i<numEnts;i++)
	{
		auto bScripted = packet->Read<Bool>();
		CBaseEntity *ent = nullptr;
		if(bScripted == false)
			ent = NET_cl_ent_create(packet,false,true);
		else
		{
			auto offset = packet->GetOffset();
			auto entSize = packet->Read<UInt32>(); // Insurance, in case entity couldn't be created, or data hasn't been received properly
			ent = NET_cl_ent_create_lua(packet,false,true);
			packet->SetOffset(offset +entSize);
		}
		if(ent != nullptr)
		{
			auto pMapComponent = ent->GetComponent<pragma::MapComponent>();
			if(pMapComponent.expired() || pMapComponent->GetMapIndex() == 0u)
				ents.push_back(ent->GetHandle());
		}
	} // Don't spawn them right away, in case they need to access each other
	for(unsigned int i=0;i<ents.size();i++)
	{
		EntityHandle &h = ents[i];
		if(h.IsValid())
			h->Spawn();
	}
}

void ClientState::HandleReceiveGameInfo(NetPacket &packet)
{
	if(IsGameActive())
		EndGame();
#ifdef DEBUG_SOCKET
	Con::ccl<<"[CLIENT] Received Game Information!"<<Con::endl;
#endif
	// Read replicated ConVars
	auto numReplicated = packet->Read<uint32_t>();
	for(auto i=decltype(numReplicated){0};i<numReplicated;++i)
	{
		auto id = packet->Read<uint32_t>();
		std::string cvar;
		auto bIdentifier = false;
		if(id == 0)
		{
			auto name = packet->ReadString();
			bIdentifier = true;
		}
		else
			bIdentifier = GetServerConVarIdentifier(id,cvar);

		auto value = packet->ReadString();
		if(bIdentifier == true)
		{
			auto *cf = GetConVar(cvar);
			if(cf != nullptr)
			{
				if(cf->GetType() == ConType::Var)
				{
					auto *cv = static_cast<ConVar*>(cf);
					if((cv->GetFlags() &ConVarFlags::Replicated) != ConVarFlags::None)
						SetConVar(cvar,value);
				}
			}
			else
				Con::cwar<<"WARNING: Replicated ConVar "<<cvar<<" doesn't exist"<<Con::endl;
		}
	}
	//
	GameModeManager::Initialize();
	//if(IsConnected())
	if(IsGameActive() == false)
		StartGame(GetConVarString("sv_gamemode"));
	auto *game = static_cast<CGame*>(GetGameState());
	game->InitializeGame();

	auto luaPath = m_svInfo->GetDownloadPath() +"lua";
	auto &scriptWatcher = m_game->GetLuaScriptWatcher();
	auto &resourceWatcher = m_game->GetResourceWatcher();

	scriptWatcher.MountDirectory(luaPath);
	resourceWatcher.MountDirectory("downloads\\");

	//if(game == NULL)
	//	return;
	std::string map = packet->ReadString();
	auto svGameFlags = packet->Read<Game::GameFlags>();
	if((svGameFlags &Game::GameFlags::LevelTransition) != Game::GameFlags::None)
		game->SetGameFlags(game->GetGameFlags() | Game::GameFlags::LevelTransition);
	double tServer = packet->Read<double>();
	tServer -= game->CurTime();
	game->SetServerTime(tServer);

	std::vector<std::string> *msgs = game->GetLuaNetMessageIndices();
	msgs->clear();
	msgs->push_back("invalid");
	unsigned int numMessages = packet->Read<unsigned int>();
	for(unsigned int i=0;i<numMessages;i++)
		msgs->push_back(packet->ReadString());

	auto &netEventIds = c_game->GetNetEventIds();
	netEventIds.clear();
	auto numEventIds = packet->Read<uint32_t>();
	netEventIds.reserve(numEventIds);
	for(auto i=decltype(numEventIds){0u};i<numEventIds;++i)
		netEventIds.push_back(packet->ReadString());

	unsigned int numConCommands = packet->Read<unsigned int>();
	for(unsigned int i=0;i<numConCommands;i++)
	{
		std::string scmd = packet->ReadString();
		unsigned int id = packet->Read<unsigned int>();
		RegisterServerConVar(scmd,id);
	}

	// Read component manager table
	auto &componentManager = static_cast<pragma::CEntityComponentManager&>(c_game->GetEntityComponentManager());
	auto &componentTypes = componentManager.GetRegisteredComponentTypes();
	auto &svComponentToClComponentTable = componentManager.GetServerComponentIdToClientComponentIdTable();
	auto numTotalSvComponents = packet->Read<uint32_t>();
	auto numComponents = packet->Read<uint32_t>();
	svComponentToClComponentTable.resize(numTotalSvComponents,pragma::CEntityComponentManager::INVALID_COMPONENT);
	for(auto i=decltype(numComponents){0u};i<numComponents;++i)
	{
		auto name = packet->ReadString();
		auto svId = packet->Read<pragma::ComponentId>();
		auto clComponentId = componentManager.PreRegisterComponentType(name);
		if(clComponentId >= svComponentToClComponentTable.size())
			svComponentToClComponentTable.resize(clComponentId +1u,pragma::CEntityComponentManager::INVALID_COMPONENT);
		svComponentToClComponentTable.at(svId) = clComponentId;
	}
	//

	unsigned int cacheSize = packet->Read<unsigned int>();
	if(cacheSize > 0)
	{
		std::string cache = packet->ReadString();
		LoadLuaCache(cache,cacheSize);
	}
	game->SetUp();

	// Note: These have to be called BEFORE the map entities are created
	m_mapInfo = std::make_unique<MapInfo>();
	m_mapInfo->name = map;
	game->CallCallbacks("OnPreLoadMap");
	game->CallLuaCallbacks("OnPreLoadMap");

	ReadEntityData(packet);

	BaseEntity *wrld = nwm::read_entity(packet);
	if(wrld != NULL)
	{
		auto pWorldComponent = wrld->GetComponent<pragma::CWorldComponent>();
		game->SetWorld(pWorldComponent.get());
	}
	LoadMap(map.c_str(),true);
	game->ReloadSoundCache();

	SendPacketTCP("game_ready");
	game->OnGameReady();

	// WEAVETODO
/*
	unsigned int ID = g_SvEntityNetworkMap->GetFactoryID(typeid(*ent));
	if(ID == 0)
		return;
	SBaseEntity *sent = static_cast<SBaseEntity*>(ent);
	NetPacket p;
	p.Write<unsigned int>(ID);
	p.Write<unsigned int>(ent->GetIndex());
	sent->SendData(&p);
	server->SendTCPMessage("ent_create",&p);
*/
	// TODO: Send all existing entities to client
	/*unsigned char numPlayers = packet->Read<unsigned char>();
	for(unsigned char i=0;i<numPlayers;i++)
	{
		unsigned int idx = packet->Read<unsigned int>();
		std::string name = packet->ReadString();
		double tm = packet->Read<double>();
		CPlayer *
	}*/

	/*RestartGame(); // CURRENT TODO
	ClientState *state = GetClientState();
	if(state == NULL)
		return;
	std::string map = packet->ReadString();
	m_tServer = packet->Read<double>();
	m_tServer -= UnPredictedCurTime();
	unsigned char lpIdx = packet->Read<unsigned char>();
	unsigned char numPlayers = packet->Read<unsigned char>();
	for(int i=0;i<numPlayers;i++)
	{
		unsigned int idx = packet->Read<unsigned int>();
		std::string name = packet->ReadString();
		double tm = packet->Read<double>();
		Player *pl = CreateEntity<Player>(idx);
		pl->Spawn();
		if(idx == lpIdx)
		{
			pl->m_bLocalPlayer = true;
			m_plLocal = pl;
		}
		else
			pl->CreateTestSprite();
		pl->m_userName = name;
		pl->m_timeConnected = tm;
	}
	m_luaClientNetMessageIndex.clear();
	m_luaClientNetMessageIndex.push_back("invalid");
	unsigned int numMessages = packet->Read<unsigned int>();
	for(int i=0;i<numMessages;i++)
		m_luaClientNetMessageIndex.push_back(packet->ReadString());

	unsigned int luaCmds = packet->Read<unsigned int>();
	for(unsigned int i=0;i<luaCmds;i++)
	{
		std::string name = packet->ReadString();
		unsigned int ID = packet->Read<unsigned int>();
		state->RegisterServerConCommand(name,ID);
	}

	unsigned int numReplicated = packet->Read<unsigned int>();
	for(int i=0;i<numReplicated;i++)
	{
		unsigned int ID = packet->Read<unsigned int>();
		std::string cvar;
		bool bIdentifier = false;
		if(ID == 0)
		{
			cvar = packet->ReadString();
			bIdentifier = true;
		}
		else
			bIdentifier = state->GetServerConVarIdentifier(ID,&cvar);
		std::string value = packet->ReadString();
		if(bIdentifier)
		{
			ConConf *cv = state->GetConVar(cvar);
			if(cv != NULL)
			{
				if(cv->GetType() == 0)
				{
					ConVar *conv = static_cast<ConVar*>(cv);
					if((conv->GetFlags() &ConVarFlags::Replicated) == ConVarFlags::Replicated)
						state->SetConVar(cvar,value);
				}
			}
			else
				Con::cwar<<"WARNING: Replicated ConVar "<<cvar<<" doesn't exist"<<Con::endl;
		}
	}
	unsigned int cacheSize = packet->Read<unsigned int>();
	if(cacheSize > 0)
	{
		std::string cache = packet->ReadString();
		state->LoadLuaCache(cache,cacheSize);
	}
	state->InitializeLua();
	BuildVMF(map.c_str());*/
}
