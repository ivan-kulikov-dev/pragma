#ifndef __C_CVAR_GLOBAL_H__
#define __C_CVAR_GLOBAL_H__
#include "pragma/c_enginedefinitions.h"
#include "pragma/clientdefinitions.h"
#include "pragma/networkdefinitions.h"
#include <pragma/console/convars.h>

#define LUA_ENABLE_RUN_GUI 1

#if LUA_ENABLE_RUN_GUI == 1
	DLLCENGINE void CMD_lua_run_gui(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
	REGISTER_CONCOMMAND_CL(lua_run_gui,CMD_lua_run_gui,ConVarFlags::None,"Runs a lua command on the GUI lua state.");

	DLLCENGINE void CMD_lua_exec_gui(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
	REGISTER_CONCOMMAND_CL(lua_exec_gui,CMD_lua_exec_gui,ConVarFlags::None,"Opens and executes a lua-file on the GUI state.");
#endif

DLLCENGINE void CMD_lua_run_cl(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
REGISTER_CONCOMMAND_CL(lua_run_cl,CMD_lua_run_cl,ConVarFlags::None,"Runs a lua command on the client lua state.");

DLLCENGINE void CMD_lua_exec_cl(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
REGISTER_CONCOMMAND_CL(lua_exec_cl,CMD_lua_exec_cl,ConVarFlags::None,"Opens and executes a lua-file on the client.");

DLLCENGINE void CMD_lua_reload_entity(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
REGISTER_CONCOMMAND_CL(lua_reload_entity,CMD_lua_reload_entity,ConVarFlags::None,"Reloads the scripts for the given entity class. Usage: lua_reload_entity <className>");

DLLCENGINE void CMD_lua_reload_weapon(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
REGISTER_CONCOMMAND_CL(lua_reload_weapon,CMD_lua_reload_weapon,ConVarFlags::None,"Reloads the scripts for the given weapon class. Usage: lua_reload_weapon <className>");

DLLCENGINE void CMD_lua_reload_entities(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
REGISTER_CONCOMMAND_CL(lua_reload_entities,CMD_lua_reload_entities,ConVarFlags::None,"Reloads the scripts for all registered lua entity classes.");

DLLCENGINE void CMD_lua_reload_weapons(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv);
REGISTER_CONCOMMAND_CL(lua_reload_weapons,CMD_lua_reload_weapons,ConVarFlags::None,"Reloads the scripts for all registered lua weapon classes.");
#endif