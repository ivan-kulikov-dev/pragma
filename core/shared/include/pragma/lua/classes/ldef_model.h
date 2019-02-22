#ifndef __LDEF_MODEL_H__
#define __LDEF_MODEL_H__
#include "pragma/lua/ldefinitions.h"
#include "pragma/model/model.h"

#define LUA_CHECK_MODEL(l,hModel) \
	if(hModel == nullptr) \
	{ \
		lua_pushstring(l,"Attempted to use a NULL model"); \
		lua_error(l); \
		return; \
	}

lua_registercheck(Model,std::shared_ptr<::Model>);
lua_registercheck(ModelSubMesh,std::shared_ptr<::ModelSubMesh>);
lua_registercheck(ModelMesh,std::shared_ptr<::ModelMesh>);

//LUA_SETUP_HANDLE_CHECK(Model,::Model,ModelHandle);

#endif