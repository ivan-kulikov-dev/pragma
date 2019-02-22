#ifndef __LTIMER_H__
#define __LTIMER_H__
#include "pragma/networkdefinitions.h"
#include <sharedutils/functioncallback.h>

namespace Lua
{
	namespace time
	{
		int create_timer(lua_State *l);
		int create_simple_timer(lua_State *l);
	};
};

#include "pragma/util/timertypes.h"

class Game;
class TimerHandle;
class DLLNETWORK Timer
{
private:
	TimerType m_timeType;
	float m_delay;
	unsigned int m_reps;
	int m_function;
	CallbackHandle m_callback;
	double m_start;
	bool m_bRemove;
	bool m_bRunning;
	bool m_bIsValid;
	std::vector<std::shared_ptr<TimerHandle>> m_handles;

	double GetCurTime(Game *game);
	double GetDeltaTime(Game *game);
protected:
	float m_next;
	virtual void Reset();
	Timer();
public:
	Timer(float delay,unsigned int reps,int function,TimerType timetype=TimerType::CurTime);
	Timer(float delay,unsigned int reps,const CallbackHandle &hCallback,TimerType timetype=TimerType::CurTime);
	~Timer();
	void Update(Game *game);
	void Start(Game *game);
	void Pause();
	void Stop();
	void Remove(Game *game);
	bool IsValid();
	bool IsRunning();
	bool IsPaused();
	void InvalidateHandle(TimerHandle *hTimer);
	float GetTimeLeft();
	void SetTimeInterval(float time);
	float GetTimeInterval();
	unsigned int GetRepetitionsLeft();
	void SetRepetitions(unsigned int rep);
	std::shared_ptr<TimerHandle> CreateHandle();
	void SetCall(Game *game,int function);
	void SetCall(Game *game,const CallbackHandle &hCallback);

	void Call(Game *game);
};

#include "pragma/util/timer_handle.h"

DLLNETWORK void Lua_Timer_Start(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_Stop(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_Pause(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_Remove(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_IsValid(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_GetTimeLeft(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_GetTimeInterval(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_SetTimeInterval(lua_State *l,std::shared_ptr<TimerHandle> pTimer,float time);
DLLNETWORK void Lua_Timer_GetRepetitionsLeft(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_SetRepetitions(lua_State *l,std::shared_ptr<TimerHandle> pTimer,unsigned int reps);
DLLNETWORK void Lua_Timer_IsRunning(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_IsPaused(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_Call(lua_State *l,std::shared_ptr<TimerHandle> pTimer);
DLLNETWORK void Lua_Timer_SetCall(lua_State *l,std::shared_ptr<TimerHandle> pTimer,luabind::object o);

#endif