#ifndef __LIMPORT_HPP__
#define __LIMPORT_HPP__

#include "pragma/networkdefinitions.h"
#include "pragma/lua/ldefinitions.h"

struct aiScene;
class Model;
struct aiScene;
namespace pragma {class SceneSnapshot;};
namespace Lua
{
	namespace import
	{
		DLLNETWORK int import_wrci(lua_State *l);
		DLLNETWORK int import_wad(lua_State *l);
		DLLNETWORK int import_wrmi(lua_State *l);
		DLLNETWORK int import_smd(lua_State *l);
		DLLNETWORK int import_obj(lua_State *l);
		DLLNETWORK int import_model_asset(lua_State *l);

		DLLNETWORK std::shared_ptr<aiScene> snapshot_to_assimp_scene(const pragma::SceneSnapshot &snapshot);
		DLLNETWORK int export_model_asset(lua_State *l);
	};
};

#endif
