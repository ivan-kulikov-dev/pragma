#ifndef __LDEFINITIONS_H__
#define __LDEFINITIONS_H__
#include <pragma/lua/luaapi.h>
#include "pragma/networkdefinitions.h"
#define luaL_checkboolean(L,n)    (luaL_checktype(L, (n), LUA_TBOOLEAN))
#define luaL_checkfunction(L,n)    (luaL_checktype(L, (n), LUA_TFUNCTION))
#define luaL_checklightuserdata(L,n)    (luaL_checktype(L, (n), LUA_TLIGHTUSERDATA))
#define luaL_checknil(L,n)    (luaL_checktype(L, (n), LUA_TNIL))
#define luaL_checktable(L,n)    (luaL_checktype(L, (n), LUA_TTABLE))
#define luaL_checkthread(L,n)    (luaL_checktype(L, (n), LUA_TTHREAD))
#define luaL_checkuserdata(L,n)    (luaL_checktype(L, (n), LUA_TUSERDATA))

class BaseLuaObj;
namespace Lua
{
	DLLNETWORK void PushObject(lua_State *l,BaseLuaObj *o);
};

inline int lua_createreference(lua_State *l,int index)
{
	lua_pushvalue(l,index);
	return luaL_ref(l,LUA_REGISTRYINDEX);
}

inline void lua_removereference(lua_State *l,int index)
{
	luaL_unref(l,LUA_REGISTRYINDEX,index);
}

namespace Lua
{
	enum class StatusCode : decltype(LUA_OK);
	enum class DLLNETWORK ErrorColorMode : uint32_t
	{
		White = 0,
		Cyan = 1,
		Magenta = 2
	};
	DLLNETWORK Lua::StatusCode Execute(lua_State *l,const std::function<Lua::StatusCode(int(*traceback)(lua_State*))> &target,ErrorColorMode colorMode);
	DLLNETWORK void Execute(lua_State *l,const std::function<void(int(*traceback)(lua_State*),void(*syntaxHandle)(lua_State*,Lua::StatusCode))> &target,ErrorColorMode colorMode);
	DLLNETWORK Lua::StatusCode Execute(lua_State *l,const std::function<Lua::StatusCode(int(*traceback)(lua_State*))> &target);
	DLLNETWORK void Execute(lua_State *l,const std::function<void(int(*traceback)(lua_State*),void(*syntaxHandle)(lua_State*,Lua::StatusCode))> &target);
	DLLNETWORK void HandleLuaError(lua_State *l);
	DLLNETWORK void HandleLuaError(lua_State *l,Lua::StatusCode s);
	DLLNETWORK ErrorColorMode GetErrorColorMode(lua_State *l);
};

#define lua_registerglobalint(global) \
	lua_pushinteger(m_lua,global); \
	lua_setglobal(m_lua,#global);

#define STR(arg) #arg
#define lua_registerglobalenum(en,name) \
	lua_pushinteger(m_lua,int(en::name)); \
	lua_setglobal(m_lua,STR(en##_##name));

#define lua_checkentityret(l,hEnt,ret) \
	if(!hEnt->IsValid()) \
	{ \
		lua_pushstring(l,"Attempted to use a NULL entity"); \
		lua_error(l); \
		return ret; \
	}

#define lua_checkentity(l,hEnt) \
	if(!hEnt->IsValid()) \
	{ \
		lua_pushstring(l,"Attempted to use a NULL entity"); \
		lua_error(l); \
		return; \
	}

#define lua_checktimer(l,pTimer) \
	{ \
		TimerHandle *hTimer = pTimer.get(); \
		if(hTimer == NULL || !hTimer->IsValid()) \
		{ \
			lua_pushstring(l,"Attempted to use a NULL timer"); \
			lua_error(l); \
			return ; \
		} \
	}

#define lua_pushentity(luastate,ent) \
	{ \
		ent->GetLuaObject()->push(luastate); \
	}

#define lua_pushtablecfunction(l,tablename,funcname,cfuncref) \
	lua_getglobal(l,tablename); \
	if(lua_istable(l,-1)) \
	{ \
		int top = lua_gettop(l); \
		lua_pushstring(l,funcname); \
		lua_pushcfunction(l,cfuncref); \
		lua_settable(l,top); \
	} \
	lua_pop(l,1);

#define lua_gettablefunction(lstate,tablename,funcname,oncall) \
	lua_getglobal(lstate,tablename); \
	if(lua_istable(lstate,-1)) \
	{ \
		lua_getfield(lstate,-1,funcname); \
		if(lua_isfunction(lstate,-1)) \
		{ \
			oncall \
		} \
	} \
	lua_pop(l,1);

#define lua_callfunction(nwstate,func) \
	{ \
		lua_State *l = nwstate->GetLuaState(); \
		lua_rawgeti(l,LUA_REGISTRYINDEX,func); \
		luaerror(nwstate,lua_pcall(l,0,0,0)); \
	}

#define lua_calltablefunction(lstate,tablename,funcname,numreturn,oncall) \
	lua_getglobal(lstate,tablename); \
	if(lua_istable(lstate,-1)) \
	{ \
		lua_getfield(lstate,-1,funcname); \
		if(lua_isfunction(lstate,-1)) \
		{ \
			if(lua_pcall((lstate),(0),(numreturn),(0)) == 0) \
			{ \
				oncall \
			} \
		} \
	} \
	lua_pop(l,1);

#define COMMA ,
#define lua_bind(data) \
	luabind::module(m_lua) \
	[ \
		data \
	];

inline const char *lua_gettype(lua_State *l,int n)
{
	const char *arg = lua_tostring(l,n);
	if(arg == NULL)
	{
		int type = lua_type(l,n);
		switch(type)
		{
		case LUA_TNIL:
			{
				arg = "nil";
				break;
			}
		case LUA_TBOOLEAN:
			{
				arg = "Boolean";
				break;
			}
			
		case LUA_TLIGHTUSERDATA:
			{
				arg = "LightUserData";
				break;
			}
		case LUA_TNUMBER:
			{
				arg = "Number";
				break;
			}
		case LUA_TSTRING:
			{
				arg = "String";
				break;
			}
		case LUA_TTABLE:
			{
				arg = "Table";
				break;
			}
		case LUA_TFUNCTION:
			{
				arg = "Function";
				break;
			}
		case LUA_TUSERDATA:
			{
				arg = "UserData";
				break;
			}
		case LUA_TTHREAD:
			{
				arg = "Thread";
				break;
			}
		default:
			arg = "Unknown";
		}
	}
	return arg;
}

namespace Lua
{
	template<typename T,typename=std::enable_if_t<std::is_arithmetic<T>::value>>
		T Check(lua_State *l,int32_t n)
	{
		return Lua::CheckNumber(l,n);
	}

	template<typename T,typename=std::enable_if_t<!std::is_arithmetic<T>::value>>
		T &Check(lua_State *l,int32_t n)
	{
		Lua::CheckUserData(l,n);
		luabind::object o(luabind::from_stack(l,n));
		boost::optional<T*> pV = luabind::object_cast_nothrow<T*>(o);
		if(pV == boost::none)
		{
			std::string err = std::string(typeid(T).name()) +" expected, got ";
			err += lua_gettype(l,n);
			luaL_argerror(l,n,err.c_str());
		}
		T *v = pV.get();
		return *v;
	}

	template<typename T>
		bool IsType(lua_State *l,int32_t n)
	{
		if(!lua_isuserdata(l,n))
			return false;
		luabind::object o(luabind::from_stack(l,n));
		boost::optional<T*> pV = luabind::object_cast_nothrow<T*>(o);
		return (pV != boost::none && pV.get() != NULL) ? true : false;
	}
};

#define lua_registercheck(type,cls) \
	inline cls *_lua_##type##_check(lua_State *l,int n) \
	{ \
		luaL_checkuserdata(l,n); \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<cls*> pV = luabind::object_cast_nothrow<cls*>(o); \
		if(pV == boost::none) \
		{ \
			std::string err = #type " expected, got "; \
			err += lua_gettype(l,n); \
			luaL_argerror(l,n,err.c_str()); \
		} \
		cls *v = pV.get(); \
		return v; \
	} \
	inline bool _lua_is##type(lua_State *l,int n) \
	{ \
		if(!lua_isuserdata(l,n)) \
			return false; \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<cls*> pV = luabind::object_cast_nothrow<cls*>(o); \
		return (pV != boost::none) ? true : false; \
	} \
	inline cls *_lua_##type##_get(lua_State *l,int n) \
	{ \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<cls*> pV = luabind::object_cast_nothrow<cls*>(o); \
		cls *v = pV.get(); \
		return v; \
	} \
	namespace Lua \
	{ \
		static cls *(&Check##type)(lua_State*,int) = _lua_##type##_check; \
		static bool (&Is##type)(lua_State*,int) = _lua_is##type; \
		static cls *(&To##type)(lua_State*,int) = _lua_##type##_get; \
	};

#define lua_registercheck_shared_ptr(type,cls) \
	inline cls *_lua_##type##_check(lua_State *l,int n) \
	{ \
		luaL_checkuserdata(l,n); \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<std::shared_ptr<cls>*> pV = luabind::object_cast_nothrow<std::shared_ptr<cls>*>(o); \
		if(pV == boost::none || pV.get() == NULL) \
		{ \
			std::string err = #type " expected, got "; \
			err += lua_gettype(l,n); \
			luaL_argerror(l,n,err.c_str()); \
		} \
		std::shared_ptr<cls> *v = pV.get(); \
		return v->get(); \
	} \
	inline bool _lua_is##type(lua_State *l,int n) \
	{ \
		if(!lua_isuserdata(l,n)) \
			return false; \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<std::shared_ptr<cls>*> pV = luabind::object_cast_nothrow<std::shared_ptr<cls>*>(o); \
		return (pV != boost::none && pV.get() != NULL) ? true : false; \
	} \
	inline cls *_lua_##type##_get(lua_State *l,int n) \
	{ \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<std::shared_ptr<cls>*> pV = luabind::object_cast_nothrow<std::shared_ptr<cls>*>(o); \
		std::shared_ptr<cls> *v = pV.get(); \
		return v->get(); \
	} \
	namespace Lua \
	{ \
		static cls *(&Check##type)(lua_State*,int) = _lua_##type##_check; \
		static bool (&Is##type)(lua_State*,int) = _lua_is##type; \
		static cls *(&To##type)(lua_State*,int) = _lua_##type##_get; \
	};

#define lua_registercheck_inherited_ptr(type,cls,pcls) \
	inline cls *_lua_##type##_check(lua_State *l,int n) \
	{ \
		luaL_checkuserdata(l,n); \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<pcls*> pV = luabind::object_cast_nothrow<pcls*>(o); \
		if(pV == boost::none || pV.get() == NULL) \
		{ \
			std::string err = #type " expected, got "; \
			err += lua_gettype(l,n); \
			luaL_argerror(l,n,err.c_str()); \
		} \
		cls *v = pV.get()->get(); \
		return v; \
	} \
	inline bool _lua_is##type(lua_State *l,int n) \
	{ \
		if(!lua_isuserdata(l,n)) \
			return false; \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<pcls*> pV = luabind::object_cast_nothrow<pcls*>(o); \
		return (pV != boost::none && pV.get() != NULL) ? true : false; \
	} \
	inline cls *_lua_##type##_get(lua_State *l,int n) \
	{ \
		luabind::object o(luabind::from_stack(l,n)); \
		boost::optional<pcls*> pV = luabind::object_cast_nothrow<pcls*>(o); \
		cls *v = pV.get()->get(); \
		return v; \
	} \
	namespace Lua \
	{ \
		static cls *(&Check##type)(lua_State*,int) = _lua_##type##_check; \
		static bool (&Is##type)(lua_State*,int) = _lua_is##type; \
		static cls *(&To##type)(lua_State*,int) = _lua_##type##_get; \
	};

#define LUA_SETUP_HANDLE_CHECK(localname,classname,handlename) \
	namespace Lua \
	{ \
		static inline classname *Check##localname(lua_State *l,int n) \
		{ \
			luaL_checkuserdata(l,n); \
			luabind::object o(luabind::from_stack(l,n)); \
			boost::optional<handlename*> pV = luabind::object_cast_nothrow<handlename*>(o); \
			if(pV == boost::none) \
			{ \
				std::string err = #classname " expected, got "; \
				err += lua_gettype(l,n); \
				luaL_argerror(l,n,err.c_str()); \
			} \
			handlename *v = pV.get(); \
			if(!v->IsValid()) \
			{ \
				std::string err = "Attempted to use a NULL "; \
				err += #localname; \
				std::transform(err.begin(),err.end(),err.begin(),::tolower); \
				lua_pushstring(l,err.c_str()); \
				lua_error(l); \
			} \
			return v->get(); \
		} \
		static inline handlename *Check##localname##Handle(lua_State *l,int n) \
		{ \
			luaL_checkuserdata(l,n); \
			luabind::object o(luabind::from_stack(l,n)); \
			boost::optional<handlename*> pV = luabind::object_cast_nothrow<handlename*>(o); \
			if(pV == boost::none) \
			{ \
				std::string err = #classname " expected, got "; \
				err += lua_gettype(l,n); \
				luaL_argerror(l,n,err.c_str()); \
			} \
			return pV.get(); \
		} \
		static inline bool Is##localname(lua_State *l,int n) \
		{ \
			if(!lua_isuserdata(l,n)) \
				return false; \
			luabind::object o(luabind::from_stack(l,n)); \
			boost::optional<handlename*> pV = luabind::object_cast_nothrow<handlename*>(o); \
			return (pV != boost::none) ? true : false; \
		} \
		static inline classname *Get##localname(lua_State *l,int n) \
		{ \
			luabind::object o(luabind::from_stack(l,n)); \
			boost::optional<handlename*> pV = luabind::object_cast_nothrow<handlename*>(o); \
			handlename *v = pV.get(); \
			if(v == nullptr) \
				return nullptr; \
			return v->get(); \
		} \
	};

#endif