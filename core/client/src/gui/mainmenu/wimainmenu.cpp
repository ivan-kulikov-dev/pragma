#include "stdafx_client.h"
#include "pragma/c_engine.h"
#include "pragma/gui/mainmenu/wimainmenu.h"
#include "pragma/gui/mainmenu/wimainmenu_newgame.h"
#include "pragma/gui/mainmenu/wimainmenu_loadgame.h"
#include "pragma/gui/mainmenu/wimainmenu_options.h"
#include "pragma/gui/mainmenu/wimainmenu_mods.hpp"
#include "pragma/gui/mainmenu/wimainmenu_credits.hpp"
#include "pragma/gui/wiloadscreen.h"
#include <wgui/types/witext.h>
#include "pragma/gui/wiconsole.h"
#include "pragma/gui/wiimageslideshow.h"
#include "pragma/gui/wiserverbrowser.h"
#include "pragma/clientstate/clientstate.h"
#include "pragma/game/c_game.h"
#include "pragma/localization.h"
#include <pragma/engine_version.h>
#include <pragma/audio/alsound_type.h>
#if WIMENU_ENABLE_PATREON_LOGO != 0
#include <shellapi.h>
#endif
#include <pragma/engine_info.hpp>

#define DLLSPEC_ISTEAMWORKS DLLNETWORK
#include <wv_steamworks.hpp>

extern DLLCENGINE CEngine *c_engine;
extern ClientState *client;
extern CGame *c_game;

WIMainMenu::WIMainMenu()
	: WIBase(),WIBaseBlur(),m_menuType(0),m_tOpen(0.0)
{
	SetKeyboardInputEnabled(true);
	//auto &context = c_engine->GetRenderContext(); // prosper TODO
	//InitializeBlur(context.GetWidth(),context.GetHeight()); // prosper TODO
	SetZPos(1000);
}

WIMainMenu::~WIMainMenu()
{
	if(m_cbBlur.IsValid())
		m_cbBlur.Remove();
	if(m_cbOnGameStart.IsValid())
		m_cbOnGameStart.Remove();
	if(m_cbOnGameEnd.IsValid())
		m_cbOnGameEnd.Remove();
	if(m_hServerBrowser.IsValid())
		m_hServerBrowser->Remove();
	if(m_cbMenuTrack.IsValid())
		m_cbMenuTrack.Remove();
	if(m_menuSound != nullptr)
		m_menuSound->Stop();

	if(m_cbOnSteamworksInit.IsValid())
		m_cbOnSteamworksInit.Remove();
	if(m_cbOnSteamworksShutdown.IsValid())
		m_cbOnSteamworksShutdown.Remove();
}

void WIMainMenu::KeyboardCallback(GLFW::Key key,int scanCode,GLFW::KeyState state,GLFW::Modifier mods)
{
	if(!m_hActive.IsValid())
		return;
	m_hActive->KeyboardCallback(key,scanCode,state,mods);
}

void WIMainMenu::OnVisibilityChanged(bool bVisible)
{
	WIBase::OnVisibilityChanged(bVisible);
	if(c_game == NULL)
		return;
	if(bVisible == true)
	{
		double tCur = c_game->RealTime();
		m_tOpen = tCur;
		// Obsolete?
		/*m_cbBlur = c_game->AddCallback("RenderPostProcessing",FunctionCallback<void,unsigned int,unsigned int>::Create([this](unsigned int ppFBO,unsigned int) {
			double t = c_game->RealTime();
			double tDelta = t -m_tOpen;
			float blurSize = (CFloat(tDelta) /0.1f) *3.f;
			if(blurSize > 2.f)
				blurSize = 2.f;
			RenderBlur(ppFBO,blurSize,GetWidth(),GetHeight());
		}));*/
		return;
	}
	if(m_cbBlur.IsValid())
		m_cbBlur.Remove();
}

void WIMainMenu::PlayNextMenuTrack(bool newRound)
{
	if(m_menuSound != nullptr)
	{
		m_menuSound->Stop();
		m_menuSound = nullptr;
	}
	if(m_cbMenuTrack.IsValid())
		m_cbMenuTrack.Remove();
	if(m_menuTracks.empty())
	{
		FileManager::FindFiles("sounds/ui/gamestartup_*.*",&m_menuTracks,nullptr);
		newRound = true;
	}
	if(m_menuTracks.empty())
		return;
	auto next = umath::random(0,CUInt32(m_menuTracks.size() -1));
	auto it = m_menuTracks.begin() +next;
	auto sound = *it;
	m_menuTracks.erase(it);
	if(client->PrecacheSound(std::string("ui/") +sound) == false || (m_menuSound = client->PlaySound(std::string("ui/") +sound,ALSoundType::GUI,ALCreateFlags::None)) == nullptr)
	{
		if(newRound == false)
			PlayNextMenuTrack(newRound);
	}
	else
	{
		m_menuSound->SetType(ALSoundType::Music | ALSoundType::GUI);
		m_cbMenuTrack = FunctionCallback<void,ALState,ALState>::Create([this](ALState,ALState newstate) {
			if(newstate != ALState::Playing)
				this->PlayNextMenuTrack();
		});
		m_menuSound->AddCallback("OnStateChanged",m_cbMenuTrack);
	}
}

void WIMainMenu::Initialize()
{
	WIBase::Initialize();
	
	m_hBg = CreateChild<WIRect>();
	WIRect *bg = m_hBg.get<WIRect>();
	bg->SetColor(0,0,0,0.5f);
	bg->SetAutoAlignToParent(true);

	bg->SetMouseInputEnabled(true);
	bg->SetMouseMovementCheckEnabled(true);
	//bg->AddCallback("OnCursorEntered",FunctionCallback<>::Create([]() {
	//	std::cout<<"ENTERED!"<<std::endl;
	//}));

	m_hBgSlideShow = CreateChild<WIImageSlideShow>();
	auto *pImageSlideShow = m_hBgSlideShow.get<WIImageSlideShow>();
	pImageSlideShow->SetAutoAlignToParent(true);
	pImageSlideShow->SetColor(0.75f,0.75f,0.75f,1.f);
	std::vector<std::string> imgFiles;
	FileManager::FindFiles("screenshots/*.tga",&imgFiles,nullptr);
	for(auto it=imgFiles.begin();it!=imgFiles.end();it++)
		*it = "screenshots/" +*it;
	pImageSlideShow->SetImages(imgFiles);

	//std::shared_ptr<ALSound> PlaySound(std::string snd,int mode=AL_CHANNEL_AUTO,unsigned char priority=0);
	m_hMain = CreateChild<WIMainMenuBase>();
	WIMainMenuBase *menu = m_hMain.get<WIMainMenuBase>();
	menu->SetVisible(false);
	menu->SetAutoAlignToParent(true);
	menu->AddMenuItem(Locale::GetText("menu_newgame"),FunctionCallback<>::Create([this]() {
		SetActiveMenu(m_hNewGame);
	}));
	menu->AddMenuItem(Locale::GetText("menu_find_servers"),FunctionCallback<>::Create([this]() {
		if(m_hServerBrowser.IsValid())
			m_hServerBrowser->Remove();
		m_hServerBrowser = CreateChild<WIServerBrowser>();
		auto *sb = m_hServerBrowser.get<WIServerBrowser>();
		sb->SetKeyboardInputEnabled(true);
		sb->SetMouseInputEnabled(true);
		sb->SetPos(200,200);
		sb->RequestFocus();
	}));
#ifdef _DEBUG
	menu->AddMenuItem(Locale::GetText("menu_loadgame"),FunctionCallback<>::Create([this]() {
		SetActiveMenu(m_hLoad);
	}));
#endif
	menu->AddMenuItem(Locale::GetText("menu_options"),FunctionCallback<>::Create([this]() {
		SetActiveMenu(m_hOptions);
	}));
	menu->AddMenuItem(Locale::GetText("menu_credits"),FunctionCallback<>::Create([this]() {
		SetActiveMenu(m_hCredits);
	}));
	menu->AddMenuItem(Locale::GetText("menu_addons"),FunctionCallback<>::Create([this]() {
		SetActiveMenu(m_hMods);
		//ShellExecute(0,0,engine_info::get_modding_hub_url().c_str(),0,0,SW_SHOW);
	}));
#ifdef _DEBUG
	menu->AddMenuItem("Loadscreen",FunctionCallback<>::Create([this]() {
		SetActiveMenu(m_hLoadScreen);
	}));
#endif
	menu->AddMenuItem(Locale::GetText("menu_quit"),FunctionCallback<>::Create([]() {
		c_engine->ShutDown();
	}));
	menu->SetKeyboardInputEnabled(true);

	m_hNewGame = CreateChild<WIMainMenuNewGame>();
	WIMainMenuNewGame *newGame = m_hNewGame.get<WIMainMenuNewGame>();
	newGame->SetVisible(false);
	newGame->SetAutoAlignToParent(true);
	newGame->SetKeyboardInputEnabled(true);

	m_hOptions = CreateChild<WIMainMenuOptions>();
	WIMainMenuOptions *options = m_hOptions.get<WIMainMenuOptions>();
	options->SetVisible(false);
	options->SetAutoAlignToParent(true);
	options->SetKeyboardInputEnabled(true);

	m_hMods = CreateChild<WIMainMenuMods>();
	auto *pMods = m_hMods.get<WIMainMenuMods>();
	pMods->SetVisible(false);
	pMods->SetAutoAlignToParent(true);
	pMods->SetKeyboardInputEnabled(true);

	m_hCredits = CreateChild<WIMainMenuCredits>();
	auto *pCredits = m_hCredits.get<WIMainMenuCredits>();
	pCredits->SetVisible(false);
	pCredits->SetAutoAlignToParent(true);
	pCredits->SetKeyboardInputEnabled(true);

	m_hLoad = CreateChild<WIMainMenuLoadGame>();
	WIMainMenuLoadGame *loadGame = m_hLoad.get<WIMainMenuLoadGame>();
	loadGame->SetVisible(false);
	loadGame->SetAutoAlignToParent(true);
	loadGame->SetKeyboardInputEnabled(true);

	m_hLoadScreen = CreateChild<WILoadScreen>();
	auto *pLoadScreen = m_hLoadScreen.get<WILoadScreen>();
	pLoadScreen->SetVisible(false);
	pLoadScreen->SetAutoAlignToParent(true);
	pLoadScreen->SetKeyboardInputEnabled(true);

	m_hVersion = CreateChild<WIText>();
	auto *pVersion = m_hVersion.get<WIText>();
	pVersion->AddStyleClass("game_version");
	pVersion->SetColor(1.f,1.f,1.f,1.f);
	pVersion->SetText(get_pretty_engine_version());
	pVersion->SizeToContents();
	pVersion->SetName("engine_version");

	m_cbOnSteamworksInit = client->AddCallback("OnSteamworksInitialized",FunctionCallback<void,std::reference_wrapper<struct ISteamworks>>::Create([this](std::reference_wrapper<struct ISteamworks> isteamworks) {
		if(m_hVersion.IsValid() == false || isteamworks.get().get_build_id == nullptr)
			return;
		m_hBuild = CreateChild<WIText>();
		auto *pBuildId = m_hBuild.get<WIText>();
		pBuildId->AddStyleClass("game_version");
		pBuildId->SetColor(1.f,1.f,1.f,1.f);
		pBuildId->SetText("Build: " +std::to_string(isteamworks.get().get_build_id()));
		pBuildId->SizeToContents();
		if(m_hVersion.IsValid())
		{
			auto pos = m_hVersion->GetPos();
			pBuildId->SetPos(pos.x,pos.y -m_hVersion->GetHeight());
		}
	}));
	m_cbOnSteamworksShutdown = client->AddCallback("OnSteamworksShutdown",FunctionCallback<void>::Create([this]() {
		if(m_hBuild.IsValid())
			m_hBuild->Remove();
	}));

	if(Lua::get_extended_lua_modules_enabled() == true)
	{
		m_hVersionAttributes = CreateChild<WIText>();
		auto *pAttributes = m_hVersionAttributes.get<WIText>();
		pAttributes->AddStyleClass("game_version");
		pAttributes->SetColor(Color::Red);
		pAttributes->SetText("[D]");
		pAttributes->SizeToContents();
	}

#if WIMENU_ENABLE_PATREON_LOGO != 0
	m_hPatreonIcon = CreateChild<WITexturedRect>();
	auto *pIcon = m_hPatreonIcon.get<WITexturedRect>();
	pIcon->SetMaterial("wgui/patreon_logo");
	pIcon->SetSize(64,64);
	pIcon->SetMouseInputEnabled(true);
	pIcon->AddCallback("OnMousePressed",FunctionCallback<void>::Create([]() {
		ShellExecute(0,0,engine_info::get_patreon_url().c_str(),0,0,SW_SHOW);
	}));
#endif

	/*WIHandle hConsole = CreateChild<WIConsole>();
	WIConsole *console = hConsole.get<WIConsole>();
	console->SetSize(256,512);
	console->SetPos(600,200);*/

	TrapFocus(true);
	RequestFocus();
	OpenMainMenu();

	m_cbOnGameStart = client->AddCallback("OnGameStart",FunctionCallback<void,CGame*>::Create([this](CGame*) {
		if(m_menuSound != nullptr)
		{
			m_menuSound->FadeOut(5.f);
			m_menuSound = nullptr;
			if(m_cbMenuTrack.IsValid())
				m_cbMenuTrack.Remove();
		}
		if(!m_hBgSlideShow.IsValid())
			return;
		m_hBgSlideShow->SetVisible(false);
	}));
	m_cbOnGameEnd = client->AddCallback("EndGame",FunctionCallback<void,CGame*>::Create([this](CGame*) {
		PlayNextMenuTrack();
		if(!m_hBgSlideShow.IsValid())
			return;
		m_hBgSlideShow->SetVisible(true);
	}));
	PlayNextMenuTrack();

	SetAutoAlignToParent(true);
}

void WIMainMenu::SetActiveMenu(WIHandle &hMenu)
{
	if(hMenu.get() == m_hActive.get() && hMenu.IsValid())
		return;
	if(m_hActive.IsValid())
		m_hActive->SetVisible(false);
	if(!hMenu.IsValid())
		return;
	hMenu->SetVisible(true);
	//hMenu->TrapFocus(true);
	hMenu->RequestFocus();
	m_hActive = hMenu;
}
void WIMainMenu::OpenMainMenu() {SetActiveMenu(m_hMain);}
void WIMainMenu::OpenNewGameMenu() {SetActiveMenu(m_hNewGame);}
void WIMainMenu::OpenLoadGameMenu() {SetActiveMenu(m_hLoad);}
void WIMainMenu::OpenOptionsMenu() {SetActiveMenu(m_hOptions);}
void WIMainMenu::OpenModsMenu() {SetActiveMenu(m_hMods);}
void WIMainMenu::OpenCreditsMenu() {SetActiveMenu(m_hCredits);}
void WIMainMenu::OpenLoadScreen() {SetActiveMenu(m_hLoadScreen);}

void WIMainMenu::OnFocusGained()
{
	if(!m_hActive.IsValid())
		return;
	WIMainMenuBase *menu = m_hActive.get<WIMainMenuBase>();
	menu->RequestFocus();
}

void WIMainMenu::OnFocusKilled()
{
	if(!m_hActive.IsValid())
		return;
	WIMainMenuBase *menu = m_hActive.get<WIMainMenuBase>();
	menu->KillFocus(true);
}

void WIMainMenu::SetContinueMenu()
{
	if(m_menuType == 1 || !m_hMain.IsValid())
		return;
	m_menuType = 1;
	WIMainMenuBase *menu = m_hMain.get<WIMainMenuBase>();
	menu->AddMenuItem(0,Locale::GetText("menu_resumegame"),FunctionCallback<>::Create([]() {
		client->CloseMainMenu();
	}));
	menu->AddMenuItem(1,Locale::GetText("menu_disconnect"),FunctionCallback<>::Create([]() {
		c_engine->EndGame();
	}));
}

void WIMainMenu::SetNewGameMenu()
{
	if(m_menuType == 0 || !m_hMain.IsValid())
		return;
	m_menuType = 0;
	WIMainMenuBase *menu = m_hMain.get<WIMainMenuBase>();
	menu->RemoveMenuItem(1);
	menu->RemoveMenuItem(0);
}

void WIMainMenu::SetSize(int x,int y)
{
	WIBase::SetSize(x,y);
	if(m_hVersion.IsValid())
	{
		auto *pVersion = m_hVersion.get();
		pVersion->SetPos(x -pVersion->GetWidth() -20,y -pVersion->GetHeight() -20);
		if(m_hVersionAttributes.IsValid())
		{
			pVersion->SetX(pVersion->GetX() -(m_hVersionAttributes->GetWidth() +4));
			m_hVersionAttributes->SetPos(pVersion->GetRight() +4,pVersion->GetY());
		}
	}
#if WIMENU_ENABLE_PATREON_LOGO != 0
	if(m_hPatreonIcon.IsValid())
	{
		auto *pIcon = m_hPatreonIcon.get();
		pIcon->SetPos(x -pIcon->GetWidth() -20,y -pIcon->GetHeight() -60);
	}
#endif
}