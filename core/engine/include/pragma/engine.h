#ifndef __ENGINE_H__
#define __ENGINE_H__
#include "pragma/definitions.h"
#include "pragma/lua/luaapi.h"
#include "pragma/console/cvar_handler.h"
#include "pragma/console/debugconsole.h"
#include <sharedutils/chronotime.h>
#include <fsys/vfileptr.h>
#include "pragma/debug/mdump.h"
#include "pragma/input/key_state.hpp"
#include "pragma/engine_info.hpp"
#include <sharedutils/util_cpu_profiler.hpp>
#include <materialmanager.h>
#include <thread>
#include <sharedutils/callback_handler.h>
#include <sharedutils/scope_guard.h>

#ifdef _DEBUG
#define ENGINE_DEFAULT_TICK_RATE 33
#else
#define ENGINE_DEFAULT_TICK_RATE 60
#endif

class NetworkState;
class ServerState;
class VFilePtrInternalReal;
class PtrConVar;
class NetPacket;
class ZIPFile;
namespace upad {class PackageManager;};
class DLLENGINE Engine
	: public CVarHandler,public util::CPUProfiler,
	public CallbackHandler
{
public:
	static const uint32_t DEFAULT_TICK_RATE;
// For internal use only! Not to be used directly!
public:
	virtual std::unordered_map<std::string,std::shared_ptr<PtrConVar>> &GetConVarPtrs();
	static ConVarHandle GetConVarHandle(std::string scvar);
//
	class DLLENGINE StateInstance
	{
	public:
		~StateInstance();
		StateInstance(const std::shared_ptr<MaterialManager> &matManager,Material *matErr);
		std::shared_ptr<MaterialManager> materialManager;
		std::unique_ptr<NetworkState> state;
	};
protected:
	bool ExecConfig(const std::string &cfg,const std::function<void(std::string&,std::vector<std::string>&)> &callback);
	std::unique_ptr<StateInstance> m_svInstance;
	bool m_bMountExternalGameResources = true;
public:
	Engine(int argc,char* argv[]);
	virtual ~Engine();
protected:
	bool RunEngineConsoleCommand(std::string cmd,std::vector<std::string> &argv,KeyState pressState=KeyState::Press,float magnitude=1.f,const std::function<bool(ConConf*,float&)> &callback=nullptr);
	void WriteServerConfig(VFilePtrReal f);
	void WriteEngineConfig(VFilePtrReal f);
	struct DLLENGINE LaunchCommand
	{
		LaunchCommand(const std::string &cmd,const std::vector<std::string> &args);
		std::string command;
		std::vector<std::string> args;
	};
	std::vector<LaunchCommand> m_launchCommands;
	void RunLaunchCommands();
	virtual void InitializeExternalArchiveManager();

	// Console
	std::queue<std::string> m_consoleInput;
	DebugConsole *m_console;
	std::thread *m_consoleThread;
	void ProcessConsoleInput(KeyState pressState=KeyState::Press);

	unsigned int m_tickRate;
	ChronoTime m_ctTick;
	long long m_lastTick;
	std::shared_ptr<VFilePtrInternalReal> m_logFile;

	bool m_bVerbose = false;

	mutable upad::PackageManager *m_padPackageManager = nullptr;

	// Init
	bool m_bRunning;
	bool m_bInitialized;

	std::unordered_map<std::string,std::function<void(int,char*[])>> m_launchOptions;

	void InitLaunchOptions(int argc,char *argv[]);
	virtual void Think();
	virtual void Tick();

#ifdef PHYS_ENGINE_PHYSX
	// PhysX
	physx::PxPhysics *m_physics;
	physx::PxFoundation *m_pxFoundation;
	physx::PxCooking *m_pxCooking;

#ifdef _DEBUG
	physx::debugger::comm::PvdConnection *m_pxPvdConnection;
#endif
#endif
public:
	DEBUGCONSOLE;
	bool Initialize(int argc,char *argv[],bool bRunLaunchCommands);
	virtual bool Initialize(int argc,char *argv[]);
	virtual void Start();
	void AddLaunchConVar(std::string cvar,std::string val);
	bool IsProfilingEnabled() const;
	virtual void DumpDebugInformation(ZIPFile &zip) const;
	virtual void Close();
	virtual void Release();
	void ClearCache();

	upad::PackageManager *GetPADPackageManager() const;

	void SetVerbose(bool bVerbose);
	bool IsVerbose() const;

	// Console
	void ConsoleInput(const std::string &line);
	void ProcessConsoleInput(const std::string &line,KeyState pressState=KeyState::Press,float magnitude=1.f);
	// Lua
	virtual NetworkState *GetNetworkState(lua_State *l);
	virtual Lua::Interface *GetLuaInterface(lua_State *l);
#ifdef PHYS_ENGINE_PHYSX
	// PhysX
	physx::PxPhysics *GetPhysics();
	physx::PxCooking *GetCookingLibrary();
#ifdef _DEBUG
	void OpenPVDConnection();
	void OpenPVDConnection(const char *host,int port,unsigned int timeout);
	void ClosePVDConnection();
#endif
#endif
	// Log
	void StartLogging();
	void EndLogging();
	void WriteToLog(const std::string &str);
	// Config
	virtual void LoadConfig();
	void LoadServerConfig();
	void SaveServerConfig();
	void SaveEngineConfig();
	// Util
	bool IsRunning();
	std::string GetDate(const std::string &format="%Y-%m-%d %X");
	const long long GetTickCount();
	double GetTickTime();
	const long long &GetLastTick();
	long long GetDeltaTick();
	UInt32 GetTickRate() const;
	void SetTickRate(UInt32 tickRate);
	bool IsGameActive();
	virtual bool IsServerOnly();
	virtual bool IsClientConnected();
	virtual void EndGame();
	// Convars
	virtual ConVarMap *GetConVarMap() override;
	virtual std::string GetConVarString(const std::string &cv);
	virtual int GetConVarInt(const std::string &cv);
	virtual float GetConVarFloat(const std::string &cv);
	virtual bool GetConVarBool(const std::string &cv);
	virtual ConConf *GetConVar(const std::string &cv);
	template<class T>
		T *GetConVar(const std::string &cv);
	virtual bool RunConsoleCommand(std::string cmd,std::vector<std::string> &argv,KeyState pressState=KeyState::Press,float magnitude=1.f,const std::function<bool(ConConf*,float&)> &callback=nullptr);
	// NetState
	virtual NetworkState *GetActiveState();

	StateInstance &GetServerStateInstance();

	bool IsMultiPlayer() const;
	bool IsSinglePlayer() const;
	bool IsActiveState(NetworkState *state);
	bool IsServerRunning();
	// ServerState
	ServerState *GetServerState() const;
	// Same as GetServerState, but returns base pointer
	NetworkState *GetServerNetworkState() const;
	ServerState *OpenServerState();
	virtual NetworkState *GetClientState() const;
	void CloseServerState();
	void StartServer();
	void CloseServer();
	void HandleLocalPlayerServerPacket(NetPacket &p);
	virtual void HandleLocalPlayerClientPacket(NetPacket &p);
	virtual void LoadMap(const char *map);
	// Config
	bool ExecConfig(const std::string &cfg);

	void SetMountExternalGameResources(bool b);
	bool ShouldMountExternalGameResources() const;

	int32_t GetRemoteDebugging() const;

	void ShutDown();
};

template<class T>
	T *Engine::GetConVar(const std::string &scvar)
{
	ConConf *cv = GetConVar(scvar);
	if(cv == NULL)
		return NULL;
	return static_cast<T*>(cv);
}

template<class T>
	std::shared_ptr<T> InitializeEngine(int argc,char *argv[])
{
#ifdef _WIN32
	auto exe = engine_info::get_executable_name();
	MiniDumper dmp(exe.c_str());
#endif
	auto en = std::make_shared<T>(argc,argv);
	ScopeGuard sgEngine([en]() {
		en->Release(); // Required in case of stack unwinding
	});
#ifdef __linux__
	en->OpenConsole();
#endif
	if(en->Initialize(argc,argv) == false)
		return nullptr;
	en->Start();
	return en;
}

inline DLLENGINE std::shared_ptr<Engine> InitializeServer(int argc,char *argv[])
{
#ifdef _WIN32
	auto exe = engine_info::get_executable_name();
	MiniDumper dmp(exe.c_str());
#endif
	auto en = std::make_shared<Engine>(argc,argv);
	ScopeGuard sgEngine([en]() {
		en->Release(); // Required in case of stack unwinding
	});
	en->OpenConsole();
	if(en->Initialize(argc,argv) == false)
		return nullptr;
	en->StartServer();
	en->Start();
	return en;
}

#endif