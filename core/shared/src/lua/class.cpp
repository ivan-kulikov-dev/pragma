#include "stdafx_shared.h"
#include <pragma/game/game.h>
#include "pragma/lua/classes/lvector.h"
#include "pragma/lua/classes/langle.h"
#include "pragma/lua/libraries/lmatrix.h"
#include "pragma/lua/libraries/lfile.h"
#include "pragma/lua/classes/lconvar.h"
#include "pragma/lua/classes/lquaternion.h"
#include "pragma/lua/libraries/ltimer.h"
#include <pragma/util/matrices.h>
#include "pragma/physics/physobj.h"
#include "pragma/lua/classes/lphysobj.h"
#ifdef PHYS_ENGINE_BULLET
#include "pragma/lua/classes/lphysics.h"
#elif PHYS_ENGINE_PHYSX
#include "pragma/lua/classes/lphysx.h"
#endif
#include "pragma/lua/classes/ldamageinfo.h"
#include "pragma/game/damageinfo.h"
#include "pragma/lua/classes/lplane.h"
#include "pragma/model/modelmesh.h"
#include "pragma/lua/classes/lmodelmesh.h"
#include "pragma/lua/classes/lmodel.h"
#include "pragma/model/model.h"
#include "luasystem.h"
#include "pragma/game/gamemode/gamemode.h"
#include "pragma/physics/physshape.h"
#include "pragma/physics/physcollisionobject.h"
#include "pragma/lua/libraries/lnoise.h"
#include "pragma/lua/classes/lsurfacematerial.h"
#include "pragma/audio/alsound_type.h"
#include "pragma/lua/libraries/lregex.h"
#include "pragma/lua/classes/ldata.hpp"
#include "pragma/entities/basenpc.h"
#include "pragma/lua/classes/lproperty.hpp"
#include "pragma/lua/libraries/lstring.hpp"
#include "pragma/util/giblet_create_info.hpp"
#include "pragma/util/bulletinfo.h"
#include "pragma/math/util_pid_controller.hpp"
#include "pragma/entities/entity_iterator.hpp"
#include "pragma/lua/lua_entity_iterator.hpp"
#include "pragma/lua/libraries/lents.h"
#include "pragma/util/util_splash_damage_info.hpp"
#include "pragma/lua/lua_call.hpp"
#include "pragma/entities/entity_property.hpp"
#include "pragma/lua/classes/lproperty.hpp"
#include "pragma/lua/classes/lproperty_generic.hpp"
#include "pragma/lua/classes/lproperty_entity.hpp"
#include "pragma/game/game_coordinate_system.hpp"
#include <pragma/util/transform.h>
#include <sharedutils/datastream.h>
#include <luainterface.hpp>
#include <luabind/iterator_policy.hpp>

extern DLLENGINE Engine *engine;

static std::ostream &operator<<(std::ostream &out,const std::shared_ptr<ALSound> &snd)
{
	auto state = snd->GetState();
	out<<"ALSound["<<snd->GetIndex()<<"][";
	switch(state)
	{
		case ALState::Initial:
			out<<"Initial";
			break;
		case ALState::Playing:
			out<<"Playing";
			break;
		case ALState::Paused:
			out<<"Paused";
			break;
		case ALState::Stopped:
			out<<"Stopped";
			break;
	}
	out<<"][";
	auto type = snd->GetType();
	auto values = umath::get_power_of_2_values(static_cast<uint64_t>(type));
	auto bStart = true;
	for(auto v : values)
	{
		if(bStart == false)
			out<<" | ";
		else
			bStart = true;
		if(v == static_cast<uint64_t>(ALSoundType::Effect))
			out<<"Effect";
		else if(v == static_cast<uint64_t>(ALSoundType::Music))
			out<<"Music";
		else if(v == static_cast<uint64_t>(ALSoundType::Voice))
			out<<"Voice";
		else if(v == static_cast<uint64_t>(ALSoundType::Weapon))
			out<<"Weapon";
		else if(v == static_cast<uint64_t>(ALSoundType::NPC))
			out<<"NPC";
		else if(v == static_cast<uint64_t>(ALSoundType::Player))
			out<<"Player";
		else if(v == static_cast<uint64_t>(ALSoundType::Vehicle))
			out<<"Vehicle";
		else if(v == static_cast<uint64_t>(ALSoundType::Physics))
			out<<"Physics";
		else if(v == static_cast<uint64_t>(ALSoundType::Environment))
			out<<"Environment";
	}
	out<<"]";
	return out;
}

static void RegisterLuaMatrices(Lua::Interface &lua);

static Quat QuaternionConstruct() {return uquat::identity();}
static Quat QuaternionConstruct(float w,float x,float y,float z) {return Quat(w,x,y,z);}
static Quat QuaternionConstruct(const Vector3 &v,float f) {return uquat::create(v,f);}
static Quat QuaternionConstruct(const Vector3 &a,const Vector3 &b,const Vector3 &c)
{
	auto m = umat::create_from_axes(a,b,c);
	return Quat(m);
}
static Quat QuaternionConstruct(const Quat &q) {return Quat(q);}
static Quat QuaternionConstruct(const Vector3 &forward,const Vector3 &up) {return uquat::create_look_rotation(forward,up);}

void NetworkState::RegisterSharedLuaClasses(Lua::Interface &lua)
{
	lua_pushtablecfunction(lua.GetState(),"string","calc_levenshtein_distance",Lua::string::calc_levenshtein_distance);
	lua_pushtablecfunction(lua.GetState(),"string","calc_levenshtein_similarity",Lua::string::calc_levenshtein_similarity);
	lua_pushtablecfunction(lua.GetState(),"string","find_longest_common_substring",Lua::string::find_longest_common_substring);
	lua_pushtablecfunction(lua.GetState(),"string","split",Lua::string::split);
	lua_pushtablecfunction(lua.GetState(),"string","join",Lua::string::join);
	lua_pushtablecfunction(lua.GetState(),"string","remove_whitespace",Lua::string::remove_whitespace);
	lua_pushtablecfunction(lua.GetState(),"string","remove_quotes",Lua::string::remove_quotes);

	auto &modUtil = lua.RegisterLibrary("util");

	auto defDataBlock = luabind::class_<std::shared_ptr<ds::Block>>("DataBlock");
	defDataBlock.def("GetInt",&Lua::DataBlock::GetInt);
	defDataBlock.def("GetFloat",&Lua::DataBlock::GetFloat);
	defDataBlock.def("GetBool",&Lua::DataBlock::GetBool);
	defDataBlock.def("GetString",&Lua::DataBlock::GetString);
	defDataBlock.def("GetData",&Lua::DataBlock::GetData);
	defDataBlock.def("SetValue",&Lua::DataBlock::SetValue);
	modUtil[defDataBlock];

	// Properties
	Lua::Property::register_classes(lua);

	auto &modMath = lua.RegisterLibrary("math");

	// Transform
	auto classDefTransform = luabind::class_<Transform>("Transform");
	classDefTransform.def(luabind::constructor<>());
	classDefTransform.def(luabind::constructor<const Vector3&>());
	classDefTransform.def(luabind::constructor<const Quat&>());
	classDefTransform.def(luabind::constructor<const Vector3&,const Quat&>());
	classDefTransform.def("GetScale",static_cast<void(*)(lua_State*,Transform&)>([](lua_State *l,Transform &t) {
		auto scale = t.GetScale();
		Lua::Push<Vector3>(l,scale);
	}));
	classDefTransform.def("GetPosition",static_cast<void(*)(lua_State*,Transform&)>([](lua_State *l,Transform &t) {
		auto pos = t.GetPosition();
		Lua::Push<Vector3>(l,pos);
	}));
	classDefTransform.def("GetRotation",static_cast<void(*)(lua_State*,Transform&)>([](lua_State *l,Transform &t) {
		auto rot = t.GetOrientation();
		Lua::Push<Quat>(l,rot);
	}));
	classDefTransform.def("GetTransformationMatrix",static_cast<void(*)(lua_State*,Transform&)>([](lua_State *l,Transform &t) {
		auto m = t.GetTransformationMatrix();
		Lua::Push<Mat4>(l,m);
	}));
	classDefTransform.def("SetScale",static_cast<void(*)(lua_State*,Transform&,const Vector3&)>([](lua_State *l,Transform &t,const Vector3 &scale) {
		t.SetScale(scale);
	}));
	classDefTransform.def("SetPosition",static_cast<void(*)(lua_State*,Transform&,const Vector3&)>([](lua_State *l,Transform &t,const Vector3 &pos) {
		t.SetPosition(pos);
	}));
	classDefTransform.def("SetRotation",static_cast<void(*)(lua_State*,Transform&,const Quat&)>([](lua_State *l,Transform &t,const Quat &rot) {
		t.SetOrientation(rot);
	}));
	classDefTransform.def("UpdateMatrix",static_cast<void(*)(lua_State*,Transform&)>([](lua_State *l,Transform &t) {
		t.UpdateMatrix();
	}));
	modMath[classDefTransform];

	// PID Controller
	auto defPIDController = luabind::class_<util::PIDController>("PIDController");
	defPIDController.def(luabind::constructor<>());
	defPIDController.def(luabind::constructor<float,float,float>());
	defPIDController.def(luabind::constructor<float,float,float,float,float>());
	defPIDController.def("SetProportionalTerm",&util::PIDController::SetProportionalTerm);
	defPIDController.def("SetIntegralTerm",&util::PIDController::SetIntegralTerm);
	defPIDController.def("SetDerivativeTerm",&util::PIDController::SetDerivativeTerm);
	defPIDController.def("SetTerms",&util::PIDController::SetTerms);
	defPIDController.def("GetProportionalTerm",&util::PIDController::GetProportionalTerm);
	defPIDController.def("GetIntegralTerm",&util::PIDController::GetIntegralTerm);
	defPIDController.def("GetDerivativeTerm",&util::PIDController::GetDerivativeTerm);
	defPIDController.def("GetTerms",static_cast<void(*)(lua_State*,const util::PIDController&)>([](lua_State *l,const util::PIDController &pidController) {
		auto p = 0.f;
		auto i = 0.f;
		auto d = 0.f;
		pidController.GetTerms(p,i,d);
		Lua::PushNumber(l,p);
		Lua::PushNumber(l,i);
		Lua::PushNumber(l,d);
	}));
	defPIDController.def("SetRange",&util::PIDController::SetRange);
	defPIDController.def("GetRange",static_cast<void(*)(lua_State*,const util::PIDController&)>([](lua_State *l,const util::PIDController &pidController) {
		auto range = pidController.GetRange();
		Lua::PushNumber(l,range.first);
		Lua::PushNumber(l,range.second);
	}));
	defPIDController.def("Calculate",&util::PIDController::Calculate);
	defPIDController.def("Reset",&util::PIDController::Reset);
	defPIDController.def("ClearRange",&util::PIDController::ClearRange);
	defPIDController.def("SetMin",&util::PIDController::SetMin);
	defPIDController.def("SetMax",&util::PIDController::SetMax);
	modMath[defPIDController];

	// Noise
	auto defNoiseModule = luabind::class_<NoiseBaseModule>("NoiseModule");
	defNoiseModule.def("GetValue",&Lua_NoiseModule_GetValue);
	defNoiseModule.def("SetScale",&Lua_NoiseModule_SetScale);
	
	auto defNoiseAbs = luabind::class_<NoiseAbs COMMA NoiseBaseModule>("Abs");
	defNoiseModule.scope[defNoiseAbs];

	auto defNoiseBillow = luabind::class_<NoiseBillow COMMA NoiseBaseModule>("Billow");
	defNoiseBillow.def("GetFrequency",&Lua_BillowNoise_GetFrequency);
	defNoiseBillow.def("GetLacunarity",&Lua_BillowNoise_GetLacunarity);
	defNoiseBillow.def("GetNoiseQuality",&Lua_BillowNoise_GetNoiseQuality);
	defNoiseBillow.def("GetOctaveCount",&Lua_BillowNoise_GetOctaveCount);
	defNoiseBillow.def("GetPersistence",&Lua_BillowNoise_GetPersistence);
	defNoiseBillow.def("GetSeed",&Lua_BillowNoise_GetSeed);
	defNoiseBillow.def("SetFrequency",&Lua_BillowNoise_SetFrequency);
	defNoiseBillow.def("SetLacunarity",&Lua_BillowNoise_SetLacunarity);
	defNoiseBillow.def("SetNoiseQuality",&Lua_BillowNoise_SetNoiseQuality);
	defNoiseBillow.def("SetOctaveCount",&Lua_BillowNoise_SetOctaveCount);
	defNoiseBillow.def("SetPersistence",&Lua_BillowNoise_SetPersistence);
	defNoiseBillow.def("SetSeed",&Lua_BillowNoise_SetSeed);
	defNoiseModule.scope[defNoiseBillow];

	auto defNoiseBlend = luabind::class_<NoiseBlend COMMA NoiseBaseModule>("Blend");
	defNoiseModule.scope[defNoiseBlend];

	auto defNoiseCache = luabind::class_<NoiseCache COMMA NoiseBaseModule>("Cache");
	defNoiseModule.scope[defNoiseCache];

	auto defNoiseCheckerboard = luabind::class_<NoiseCheckerboard COMMA NoiseBaseModule>("Checkerboard");
	defNoiseModule.scope[defNoiseCheckerboard];

	auto defNoiseClamp = luabind::class_<NoiseClamp COMMA NoiseBaseModule>("Clamp");
	defNoiseModule.scope[defNoiseClamp];

	auto defNoiseConst = luabind::class_<NoiseConst COMMA NoiseBaseModule>("Const");
	defNoiseModule.scope[defNoiseConst];

	auto defNoiseCurve = luabind::class_<NoiseCurve COMMA NoiseBaseModule>("Curve");
	defNoiseModule.scope[defNoiseCurve];

	auto defNoiseCylinders = luabind::class_<NoiseCylinders COMMA NoiseBaseModule>("Cylinders");
	defNoiseModule.scope[defNoiseCylinders];

	auto defNoiseDisplace = luabind::class_<NoiseDisplace COMMA NoiseBaseModule>("Displace");
	defNoiseModule.scope[defNoiseDisplace];

	auto defNoiseExponent = luabind::class_<NoiseExponent COMMA NoiseBaseModule>("Exponent");
	defNoiseModule.scope[defNoiseExponent];

	auto defNoiseInvert = luabind::class_<NoiseInvert COMMA NoiseBaseModule>("Invert");
	defNoiseModule.scope[defNoiseInvert];

	auto defNoiseMax = luabind::class_<NoiseMax COMMA NoiseBaseModule>("Max");
	defNoiseModule.scope[defNoiseMax];

	auto defNoiseMin = luabind::class_<NoiseMin COMMA NoiseBaseModule>("Min");
	defNoiseModule.scope[defNoiseMin];

	auto defNoiseMultiply = luabind::class_<NoiseMultiply COMMA NoiseBaseModule>("Multiply");
	defNoiseModule.scope[defNoiseMultiply];

	auto defNoisePerlin = luabind::class_<NoisePerlin COMMA NoiseBaseModule>("Perlin");
	defNoisePerlin.def("GetFrequency",&Lua_PerlinNoise_GetFrequency);
	defNoisePerlin.def("GetLacunarity",&Lua_PerlinNoise_GetLacunarity);
	defNoisePerlin.def("GetNoiseQuality",&Lua_PerlinNoise_GetNoiseQuality);
	defNoisePerlin.def("GetOctaveCount",&Lua_PerlinNoise_GetOctaveCount);
	defNoisePerlin.def("GetPersistence",&Lua_PerlinNoise_GetPersistence);
	defNoisePerlin.def("GetSeed",&Lua_PerlinNoise_GetSeed);
	defNoisePerlin.def("SetFrequency",&Lua_PerlinNoise_SetFrequency);
	defNoisePerlin.def("SetLacunarity",&Lua_PerlinNoise_SetLacunarity);
	defNoisePerlin.def("SetNoiseQuality",&Lua_PerlinNoise_SetNoiseQuality);
	defNoisePerlin.def("SetOctaveCount",&Lua_PerlinNoise_SetOctaveCount);
	defNoisePerlin.def("SetPersistence",&Lua_PerlinNoise_SetPersistence);
	defNoisePerlin.def("SetSeed",&Lua_PerlinNoise_SetSeed);
	defNoiseModule.scope[defNoisePerlin];

	auto defNoisePower = luabind::class_<NoisePower COMMA NoiseBaseModule>("Power");
	defNoiseModule.scope[defNoisePower];

	auto defNoiseRidgedMulti = luabind::class_<NoiseRidgedMulti COMMA NoiseBaseModule>("RidgedMulti");
	defNoiseRidgedMulti.def("GetFrequency",&Lua_RidgedMultiNoise_GetFrequency);
	defNoiseRidgedMulti.def("GetLacunarity",&Lua_RidgedMultiNoise_GetLacunarity);
	defNoiseRidgedMulti.def("GetNoiseQuality",&Lua_RidgedMultiNoise_GetNoiseQuality);
	defNoiseRidgedMulti.def("GetOctaveCount",&Lua_RidgedMultiNoise_GetOctaveCount);
	defNoiseRidgedMulti.def("GetSeed",&Lua_RidgedMultiNoise_GetSeed);
	defNoiseRidgedMulti.def("SetFrequency",&Lua_RidgedMultiNoise_SetFrequency);
	defNoiseRidgedMulti.def("SetLacunarity",&Lua_RidgedMultiNoise_SetLacunarity);
	defNoiseRidgedMulti.def("SetNoiseQuality",&Lua_RidgedMultiNoise_SetNoiseQuality);
	defNoiseRidgedMulti.def("SetOctaveCount",&Lua_RidgedMultiNoise_SetOctaveCount);
	defNoiseRidgedMulti.def("SetSeed",&Lua_RidgedMultiNoise_SetSeed);
	defNoiseModule.scope[defNoiseRidgedMulti];

	auto defNoiseRotatePoint = luabind::class_<NoiseRotatePoint COMMA NoiseBaseModule>("RotatePoint");
	defNoiseModule.scope[defNoiseRotatePoint];

	auto defNoiseScaleBias = luabind::class_<NoiseScaleBias COMMA NoiseBaseModule>("ScaleBias");
	defNoiseModule.scope[defNoiseScaleBias];

	auto noiseScalePoint = luabind::class_<NoiseScalePoint COMMA NoiseBaseModule>("ScalePoint");
	defNoiseModule.scope[noiseScalePoint];

	auto noiseSelect = luabind::class_<NoiseSelect COMMA NoiseBaseModule>("Select");
	defNoiseModule.scope[noiseSelect];

	auto noiseSpheres = luabind::class_<NoiseSpheres COMMA NoiseBaseModule>("Spheres");
	defNoiseModule.scope[noiseSpheres];

	auto noiseTerrace = luabind::class_<NoiseTerrace COMMA NoiseBaseModule>("Terrace");
	defNoiseModule.scope[noiseTerrace];

	auto noiseTransplatePoint = luabind::class_<NoiseTranslatePoint COMMA NoiseBaseModule>("TranslatePoint");
	defNoiseModule.scope[noiseTransplatePoint];

	auto noiseTurbulance = luabind::class_<NoiseTurbulance COMMA NoiseBaseModule>("Turbulance");
	defNoiseModule.scope[noiseTurbulance];

	auto noiseVoroni = luabind::class_<NoiseVoronoi COMMA NoiseBaseModule>("Voronoi");
	noiseVoroni.def("GetDisplacement",&Lua_VoronoiNoise_GetDisplacement);
	noiseVoroni.def("GetFrequency",&Lua_VoronoiNoise_GetFrequency);
	noiseVoroni.def("GetSeed",&Lua_VoronoiNoise_GetSeed);
	noiseVoroni.def("SetDisplacement",&Lua_VoronoiNoise_SetDisplacement);
	noiseVoroni.def("SetFrequency",&Lua_VoronoiNoise_SetFrequency);
	noiseVoroni.def("SetSeed",&Lua_VoronoiNoise_SetSeed);
	defNoiseModule.scope[noiseVoroni];

	modMath[defNoiseModule];

	auto noiseMap = luabind::class_<std::shared_ptr<noise::utils::NoiseMap>>("NoiseMap");
	noiseMap.def("GetValue",&Lua_NoiseMap_GetValue);
	noiseMap.def("GetHeight",&Lua_NoiseMap_GetHeight);
	noiseMap.def("GetWidth",&Lua_NoiseMap_GetWidth);
	modMath[noiseMap];
	//

	auto defVectori = luabind::class_<Vector3i>("Vectori");
	defVectori.def(luabind::constructor<>());
	defVectori.def(luabind::constructor<int32_t,int32_t,int32_t>());
	defVectori.def(luabind::tostring(luabind::self));
	defVectori.def(-luabind::const_self);
	defVectori.def_readwrite("x",&Vector3i::x);
	defVectori.def_readwrite("y",&Vector3i::y);
	defVectori.def_readwrite("z",&Vector3i::z);
	defVectori.def(luabind::const_self /int32_t());
	defVectori.def(luabind::const_self *int32_t());
	defVectori.def(luabind::const_self +Vector3i());
	defVectori.def(luabind::const_self -Vector3i());
	defVectori.def(luabind::const_self ==Vector3i());
	defVectori.def(int32_t() /luabind::const_self);
	defVectori.def(int32_t() *luabind::const_self);
	defVectori.def("Copy",&Lua::Vectori::Copy);
	modMath[defVectori];
	
	auto defVector2i = luabind::class_<Vector2i>("Vector2i");
	defVector2i.def(luabind::constructor<>());
	defVector2i.def(luabind::constructor<int32_t,int32_t>());
	defVector2i.def(luabind::tostring(luabind::self));
	defVector2i.def(-luabind::const_self);
	defVector2i.def_readwrite("x",&Vector2i::x);
	defVector2i.def_readwrite("y",&Vector2i::y);
	defVector2i.def(luabind::const_self /int32_t());
	defVector2i.def(luabind::const_self *int32_t());
	defVector2i.def(luabind::const_self +Vector2i());
	defVector2i.def(luabind::const_self -Vector2i());
	defVector2i.def(luabind::const_self ==Vector2i());
	defVector2i.def(int32_t() /luabind::const_self);
	defVector2i.def(int32_t() *luabind::const_self);
	defVector2i.def("Copy",&Lua::Vector2i::Copy);
	modMath[defVector2i];

	auto defVector4i = luabind::class_<Vector4i>("Vector4i");
	defVector4i.def(luabind::constructor<>());
	defVector4i.def(luabind::constructor<int32_t,int32_t,int32_t,int32_t>());
	defVector4i.def(luabind::tostring(luabind::self));
	defVector4i.def(-luabind::const_self);
	defVector4i.def_readwrite("w",&Vector4i::w);
	defVector4i.def_readwrite("x",&Vector4i::x);
	defVector4i.def_readwrite("y",&Vector4i::y);
	defVector4i.def_readwrite("z",&Vector4i::z);
	defVector4i.def(luabind::const_self /int32_t());
	defVector4i.def(luabind::const_self *int32_t());
	defVector4i.def(luabind::const_self +Vector4i());
	defVector4i.def(luabind::const_self -Vector4i());
	defVector4i.def(luabind::const_self ==Vector4i());
	defVector4i.def(luabind::const_self *Vector4i());
	defVector4i.def(int32_t() /luabind::const_self);
	defVector4i.def(int32_t() *luabind::const_self);
	defVector4i.def("Copy",&Lua::Vector4i::Copy);
	modMath[defVector4i];

	auto defVector = luabind::class_<Vector3>("Vector");
	defVector.def(luabind::constructor<>());
	defVector.def(luabind::constructor<float,float,float>());
	defVector.def(luabind::tostring(luabind::self));
	defVector.def(-luabind::const_self);
	defVector.def_readwrite("x",&Vector3::x);
	defVector.def_readwrite("y",&Vector3::y);
	defVector.def_readwrite("z",&Vector3::z);
	defVector.def(luabind::const_self /float());
	defVector.def(luabind::const_self *float());
	defVector.def(luabind::const_self +Vector3());
	defVector.def(luabind::const_self -Vector3());
	defVector.def(luabind::const_self ==Vector3());
	defVector.def(luabind::const_self *Quat());
	defVector.def(float() /luabind::const_self);
	defVector.def(float() *luabind::const_self);
	defVector.def(Quat() *luabind::const_self);
	defVector.def("GetNormal",&Lua::Vector::GetNormal);
	defVector.def("Normalize",&Lua::Vector::Normalize);
	defVector.def("ToEulerAngles",&Lua::Vector::Angle);
	defVector.def("Length",&Lua::Vector::Length);
	defVector.def("LengthSqr",&Lua::Vector::LengthSqr);
	defVector.def("Distance",&Lua::Vector::Distance);
	defVector.def("DistanceSqr",&Lua::Vector::DistanceSqr);
	defVector.def("PlanarDistance",&Lua::Vector::PlanarDistance);
	defVector.def("PlanarDistanceSqr",&Lua::Vector::PlanarDistanceSqr);
	defVector.def("Cross",&Lua::Vector::Cross);
	defVector.def("DotProduct",&Lua::Vector::DotProduct);
	defVector.def("GetRotation",&Lua::Vector::GetRotation);
	defVector.def("Rotate",static_cast<void(*)(lua_State*,Vector3&,const EulerAngles&)>(&Lua::Vector::Rotate));
	defVector.def("Rotate",static_cast<void(*)(lua_State*,Vector3&,const Vector3&,float)>(&Lua::Vector::Rotate));
	defVector.def("Rotate",static_cast<void(*)(lua_State*,Vector3&,const Quat&)>(&Lua::Vector::Rotate));
	defVector.def("RotateAround",&Lua::Vector::RotateAround);
	defVector.def("Lerp",&Lua::Vector::Lerp);
	defVector.def("Slerp",static_cast<void(*)(lua_State*,const Vector3&,const Vector3&,float)>([](lua_State *l,const Vector3 &a,const Vector3 &b,float factor) {
		auto result = glm::slerp(a,b,factor);
		Lua::Push<Vector3>(l,result);
	}));
	defVector.def("Copy",&Lua::Vector::Copy);
	defVector.def("Set",static_cast<void(*)(lua_State*,Vector3&,const Vector3&)>(&Lua::Vector::Set));
	defVector.def("Set",static_cast<void(*)(lua_State*,Vector3&,float,float,float)>(&Lua::Vector::Set));
	defVector.def("Get",static_cast<void(*)(lua_State*,const Vector3&,uint32_t)>([](lua_State *l,const Vector3 &v,uint32_t idx) {
		Lua::PushNumber(l,v[idx]);
	}));
	defVector.def("ToMatrix",&Lua::Vector::ToMatrix);
	defVector.def("SnapToGrid",static_cast<void(*)(lua_State*,Vector3&)>(&Lua::Vector::SnapToGrid));
	defVector.def("SnapToGrid",static_cast<void(*)(lua_State*,Vector3&,UInt32)>(&Lua::Vector::SnapToGrid));
	defVector.def("Project",&Lua::Vector::Project);
	defVector.def("ProjectToPlane",static_cast<void(*)(lua_State*,const Vector3&,const Vector3&,float)>(&Lua::Vector::ProjectToPlane));
	defVector.def("GetPerpendicular",&Lua::Vector::GetPerpendicular);
	defVector.def("OuterProduct",&Lua::Vector::OuterProduct);
	modMath[defVector];

	auto defVector2 = luabind::class_<Vector2>("Vector2");
	defVector2.def(luabind::constructor<>());
	defVector2.def(luabind::constructor<float,float>());
	defVector2.def(luabind::tostring(luabind::self));
	defVector2.def(-luabind::const_self);
	defVector2.def_readwrite("x",&Vector2::x);
	defVector2.def_readwrite("y",&Vector2::y);
	defVector2.def(luabind::const_self /float());
	defVector2.def(luabind::const_self *float());
	defVector2.def(luabind::const_self +Vector2());
	defVector2.def(luabind::const_self -Vector2());
	defVector2.def(luabind::const_self ==Vector2());
	defVector2.def(float() /luabind::const_self);
	defVector2.def(float() *luabind::const_self);
	defVector2.def("GetNormal",&Lua::Vector2::GetNormal);
	defVector2.def("Normalize",&Lua::Vector2::Normalize);
	defVector2.def("Length",&Lua::Vector2::Length);
	defVector2.def("LengthSqr",&Lua::Vector2::LengthSqr);
	defVector2.def("Distance",&Lua::Vector2::Distance);
	defVector2.def("DistanceSqr",&Lua::Vector2::DistanceSqr);
	defVector2.def("Cross",&Lua::Vector2::Cross);
	defVector2.def("DotProduct",&Lua::Vector2::DotProduct);
	defVector2.def("Rotate",&Lua::Vector2::Rotate);
	defVector2.def("RotateAround",&Lua::Vector2::RotateAround);
	defVector2.def("Lerp",&Lua::Vector2::Lerp);
	defVector2.def("Copy",&Lua::Vector2::Copy);
	defVector2.def("Set",static_cast<void(*)(lua_State*,Vector2&,const Vector2&)>(&Lua::Vector2::Set));
	defVector2.def("Set",static_cast<void(*)(lua_State*,Vector2&,float,float)>(&Lua::Vector2::Set));
	defVector2.def("Get",static_cast<void(*)(lua_State*,const Vector2&,uint32_t)>([](lua_State *l,const Vector2 &v,uint32_t idx) {
		Lua::PushNumber(l,v[idx]);
	}));
	defVector2.def("Project",&Lua::Vector2::Project);
	modMath[defVector2];

	auto defVector4 = luabind::class_<Vector4>("Vector4");
	defVector4.def(luabind::constructor<>());
	defVector4.def(luabind::constructor<float,float,float,float>());
	defVector4.def(luabind::tostring(luabind::self));
	defVector4.def(-luabind::const_self);
	defVector4.def_readwrite("w",&Vector4::w);
	defVector4.def_readwrite("x",&Vector4::x);
	defVector4.def_readwrite("y",&Vector4::y);
	defVector4.def_readwrite("z",&Vector4::z);
	defVector4.def(luabind::const_self /float());
	defVector4.def(luabind::const_self *float());
	defVector4.def(luabind::const_self +Vector4());
	defVector4.def(luabind::const_self -Vector4());
	defVector4.def(luabind::const_self ==Vector4());
	defVector4.def(luabind::const_self *Mat4());
	defVector4.def(float() /luabind::const_self);
	defVector4.def(float() *luabind::const_self);
	defVector4.def("GetNormal",&Lua::Vector4::GetNormal);
	defVector4.def("Normalize",&Lua::Vector4::Normalize);
	defVector4.def("Length",&Lua::Vector4::Length);
	defVector4.def("LengthSqr",&Lua::Vector4::LengthSqr);
	defVector4.def("Distance",&Lua::Vector4::Distance);
	defVector4.def("DistanceSqr",&Lua::Vector4::DistanceSqr);
	defVector4.def("DotProduct",&Lua::Vector4::DotProduct);
	defVector4.def("Lerp",&Lua::Vector4::Lerp);
	defVector4.def("Copy",&Lua::Vector4::Copy);
	defVector4.def("Set",static_cast<void(*)(lua_State*,Vector4&,const Vector4&)>(&Lua::Vector4::Set));
	defVector4.def("Set",static_cast<void(*)(lua_State*,Vector4&,float,float,float,float)>(&Lua::Vector4::Set));
	defVector4.def("Get",static_cast<void(*)(lua_State*,const Vector4&,uint32_t)>([](lua_State *l,const Vector4 &v,uint32_t idx) {
		Lua::PushNumber(l,v[idx]);
	}));
	defVector4.def("Project",&Lua::Vector4::Project);
	modMath[defVector4];

	auto defEulerAngles = luabind::class_<EulerAngles>("EulerAngles");
	defEulerAngles.def(luabind::constructor<>());
	defEulerAngles.def(luabind::constructor<float,float,float>());
	defEulerAngles.def(luabind::constructor<const EulerAngles&>());
	defEulerAngles.def(luabind::constructor<const Mat4&>());
	defEulerAngles.def(luabind::constructor<const Vector3&>());
	defEulerAngles.def(luabind::constructor<const Vector3&,const Vector3&>());
	defEulerAngles.def(luabind::constructor<const Quat&>());
	defEulerAngles.def(luabind::tostring(luabind::self));
	defEulerAngles.def(-luabind::const_self);
	defEulerAngles.def_readwrite("p",&EulerAngles::p);
	defEulerAngles.def_readwrite("y",&EulerAngles::y);
	defEulerAngles.def_readwrite("r",&EulerAngles::r);
	defEulerAngles.def(luabind::const_self /float());
	defEulerAngles.def(luabind::const_self *float());
	defEulerAngles.def(luabind::const_self +EulerAngles());
	defEulerAngles.def(luabind::const_self -EulerAngles());
	defEulerAngles.def(luabind::const_self ==EulerAngles());
	defEulerAngles.def(float() *luabind::const_self);
	defEulerAngles.def("GetForward",&EulerAngles::Forward);
	defEulerAngles.def("GetRight",&EulerAngles::Right);
	defEulerAngles.def("GetUp",&Lua::Angle::Up);
	defEulerAngles.def("GetOrientation",&Lua::Angle::Orientation);
	defEulerAngles.def("Normalize",static_cast<void(EulerAngles::*)()>(&EulerAngles::Normalize));
	defEulerAngles.def("Normalize",static_cast<void(EulerAngles::*)(float)>(&EulerAngles::Normalize));
	defEulerAngles.def("ToMatrix",&Lua::Angle::ToMatrix);
	defEulerAngles.def("Copy",&Lua::Angle::Copy);
	defEulerAngles.def("ToQuaternion",Lua::Angle::ToQuaternion);
	defEulerAngles.def("ToQuaternion",static_cast<void(*)(lua_State*,EulerAngles*)>([](lua_State *l,EulerAngles *ang) {
		Lua::Angle::ToQuaternion(l,ang,umath::to_integral(pragma::RotationOrder::YXZ));
	}));
	defEulerAngles.def("Set",static_cast<void(EulerAngles::*)(const EulerAngles&)>(&EulerAngles::Set));
	defEulerAngles.def("Set",&Lua::Angle::Set);
	defEulerAngles.def("Get",static_cast<void(*)(lua_State*,const EulerAngles&,uint32_t)>([](lua_State *l,const EulerAngles &ang,uint32_t idx) {
		Lua::PushNumber(l,ang[idx]);
	}));
	modMath[defEulerAngles];

	modMath[
		luabind::def("Quaternion",static_cast<Quat(*)()>(&QuaternionConstruct)) COMMA
		luabind::def("Quaternion",static_cast<Quat(*)(float,float,float,float)>(&QuaternionConstruct)) COMMA
		luabind::def("Quaternion",static_cast<Quat(*)(const Vector3&,float)>(&QuaternionConstruct)) COMMA
		luabind::def("Quaternion",static_cast<Quat(*)(const Vector3&,const Vector3&,const Vector3&)>(&QuaternionConstruct)) COMMA
		luabind::def("Quaternion",static_cast<Quat(*)(const Quat&)>(&QuaternionConstruct)) COMMA
		luabind::def("Quaternion",static_cast<Quat(*)(const Vector3&,const Vector3&)>(&QuaternionConstruct))
	];
	auto defQuat = luabind::class_<Quat>("QuaternionInternal");
	defQuat.def(luabind::tostring(luabind::self));
	defQuat.def_readwrite("w",&Quat::w);
	defQuat.def_readwrite("x",&Quat::x);
	defQuat.def_readwrite("y",&Quat::y);
	defQuat.def_readwrite("z",&Quat::z);
	defQuat.def(-luabind::const_self);
	defQuat.def(luabind::const_self /float());
	defQuat.def(luabind::const_self *float());
	defQuat.def(luabind::const_self *Quat());
	defQuat.def(luabind::const_self ==Quat());
	defQuat.def(float() *luabind::const_self);
	defQuat.def("GetForward",&Lua::Quaternion::GetForward);
	defQuat.def("GetRight",&Lua::Quaternion::GetRight);
	defQuat.def("GetUp",&Lua::Quaternion::GetUp);
	defQuat.def("GetOrientation",&Lua::Quaternion::GetOrientation);
	defQuat.def("DotProduct",&Lua::Quaternion::DotProduct);
	defQuat.def("Inverse",&Lua::Quaternion::Inverse);
	defQuat.def("GetInverse",&Lua::Quaternion::GetInverse);
	defQuat.def("Length",&Lua::Quaternion::Length);
	defQuat.def("Normalize",&Lua::Quaternion::Normalize);
	defQuat.def("GetNormal",&Lua::Quaternion::GetNormal);
	defQuat.def("Copy",&Lua::Quaternion::Copy);
	defQuat.def("ToMatrix",&Lua::Quaternion::ToMatrix);
	defQuat.def("Lerp",&Lua::Quaternion::Lerp);
	defQuat.def("Slerp",&Lua::Quaternion::Slerp);
	defQuat.def("ToEulerAngles",Lua::Quaternion::ToEulerAngles);
	defQuat.def("ToEulerAngles",static_cast<void(*)(lua_State*,Quat&)>([](lua_State *l,Quat &rot) {
		Lua::Quaternion::ToEulerAngles(l,rot,umath::to_integral(pragma::RotationOrder::YXZ));
	}));
	defQuat.def("ToAxisAngle",&Lua::Quaternion::ToAxisAngle);
	defQuat.def("Set",&Lua::Quaternion::Set);
	defQuat.def("RotateX",static_cast<void(*)(Quat&,float)>(&uquat::rotate_x));
	defQuat.def("RotateY",static_cast<void(*)(Quat&,float)>(&uquat::rotate_y));
	defQuat.def("RotateZ",static_cast<void(*)(Quat&,float)>(&uquat::rotate_z));
	defQuat.def("Rotate",static_cast<void(*)(Quat&,const Vector3&,float)>(&uquat::rotate));
	defQuat.def("Rotate",static_cast<void(*)(Quat&,const EulerAngles&)>(&uquat::rotate));
	defQuat.def("ApproachDirection",static_cast<void(*)(lua_State*,const Quat&,const Vector3&,const Vector3&,const ::Vector2&,const ::Vector2*,const ::Vector2*,const Quat*,const EulerAngles*)>(&Lua::Quaternion::ApproachDirection));
	defQuat.def("ApproachDirection",static_cast<void(*)(lua_State*,const Quat&,const Vector3&,const Vector3&,const ::Vector2&,const ::Vector2*,const ::Vector2*,const Quat*)>(&Lua::Quaternion::ApproachDirection));
	defQuat.def("ApproachDirection",static_cast<void(*)(lua_State*,const Quat&,const Vector3&,const Vector3&,const ::Vector2&,const ::Vector2*,const ::Vector2*)>(&Lua::Quaternion::ApproachDirection));
	defQuat.def("ApproachDirection",static_cast<void(*)(lua_State*,const Quat&,const Vector3&,const Vector3&,const ::Vector2&,const ::Vector2*)>(&Lua::Quaternion::ApproachDirection));
	defQuat.def("ApproachDirection",static_cast<void(*)(lua_State*,const Quat&,const Vector3&,const Vector3&,const ::Vector2&)>(&Lua::Quaternion::ApproachDirection));
	defQuat.def("GetConjugate",&Lua::Quaternion::GetConjugate);
	modMath[defQuat];
	auto _G = luabind::globals(lua.GetState());
	_G["Vector2i"] = _G["math"]["Vector2i"];
	_G["Vector"] = _G["math"]["Vector"];
	_G["Vector2"] = _G["math"]["Vector2"];
	_G["Vector4"] = _G["math"]["Vector4"];
	_G["EulerAngles"] = _G["math"]["EulerAngles"];
	_G["Quaternion"] = _G["math"]["Quaternion"];

	RegisterLuaMatrices(lua);
	//modelMeshClassDef.scope[luabind::def("Create",&Lua::ModelMesh::Client::Create)];
}

static bool operator==(const EntityHandle &v,const LEntityProperty &prop) {return **prop == v;}
static std::ostream& operator<<(std::ostream &str,const LEntityProperty &v)
{
	if((*v)->IsValid())
		(*v)->get()->print(str);
	else
		str<<"NULL";
	return str;
}
void Lua::Property::push(lua_State *l,pragma::EntityProperty &prop) {Lua::Property::push_property<LEntityPropertyWrapper>(l,prop);}

void Game::RegisterLuaClasses()
{
	NetworkState::RegisterSharedLuaClasses(GetLuaInterface());

	// Entity
	auto &modUtil = GetLuaInterface().RegisterLibrary("util");
	auto entDef = luabind::class_<LEntityProperty,LBasePropertyWrapper>("EntityProperty");
	Lua::Property::add_generic_methods<LEntityProperty,EntityHandle,luabind::class_<LEntityProperty,LBasePropertyWrapper>>(entDef);
	entDef.def(luabind::constructor<>());
	entDef.def(luabind::constructor<EntityHandle>());
	entDef.def(luabind::tostring(luabind::const_self));
	entDef.def("Link",static_cast<void(*)(lua_State*,LEntityProperty&,LEntityProperty&)>(Lua::Property::link<LEntityProperty,EntityHandle>));
	modUtil[entDef];

	auto defSplashDamageInfo = luabind::class_<util::SplashDamageInfo>("SplashDamageInfo");
	defSplashDamageInfo.def(luabind::constructor<>());
	defSplashDamageInfo.def_readwrite("origin",&util::SplashDamageInfo::origin);
	defSplashDamageInfo.def_readwrite("radius",&util::SplashDamageInfo::radius);
	defSplashDamageInfo.def_readwrite("damageInfo",&util::SplashDamageInfo::damageInfo);
	defSplashDamageInfo.def("SetCone",static_cast<void(*)(lua_State*,util::SplashDamageInfo&,const Vector3&,float)>([](lua_State *l,util::SplashDamageInfo &splashDamageInfo,const Vector3 &coneDirection,float coneAngle) {
		splashDamageInfo.cone = {{coneDirection,coneAngle}};
	}));
	defSplashDamageInfo.def("SetCallback",static_cast<void(*)(lua_State*,util::SplashDamageInfo&,luabind::object)>([](lua_State *l,util::SplashDamageInfo &splashDamageInfo,luabind::object oCallback) {
		Lua::CheckFunction(l,2);
		splashDamageInfo.callback = [l,oCallback](BaseEntity *ent,DamageInfo &dmgInfo) -> bool {
			auto r = Lua::CallFunction(l,[ent,&dmgInfo,&oCallback](lua_State *l) -> Lua::StatusCode {
				oCallback.push(l);
				if(ent != nullptr)
					ent->GetLuaObject()->push(l);
				else
					Lua::PushNil(l);
				Lua::Push<DamageInfo*>(l,&dmgInfo);
				return Lua::StatusCode::Ok;
			},1);
			if(r == Lua::StatusCode::Ok && Lua::IsSet(l,-1))
				return Lua::CheckBool(l,-1);
			return false;
		};
	}));
	modUtil[defSplashDamageInfo];

	auto &modGame = GetLuaInterface().RegisterLibrary("game");
	auto defGmBase = luabind::class_<GameMode COMMA GameModeWrapper>("Base");
	defGmBase.def(luabind::constructor<>());
	defGmBase.def("GetName",&Lua::GameMode::GetName);
	defGmBase.def("GetIdentifier",&Lua::GameMode::GetIdentifier);
	defGmBase.def("GetClassName",&Lua::GameMode::GetClassName);
	defGmBase.def("GetAuthor",&Lua::GameMode::GetAuthor);
	defGmBase.def("GetVersion",&Lua::GameMode::GetVersion);
	defGmBase.def("Think",&GameModeWrapper::LThink,&GameModeWrapper::default_Think);
	defGmBase.def("Tick",&GameModeWrapper::LTick,&GameModeWrapper::default_Tick);
	defGmBase.def("OnEntityTakeDamage",&GameModeWrapper::LOnEntityTakeDamage,&GameModeWrapper::default_OnEntityTakeDamage);
	defGmBase.def("OnEntityTakenDamage",&GameModeWrapper::LOnEntityTakenDamage,&GameModeWrapper::default_OnEntityTakenDamage);
	defGmBase.def("OnEntityHealthChanged",&GameModeWrapper::LOnEntityHealthChanged,&GameModeWrapper::default_OnEntityHealthChanged);
	defGmBase.def("OnPlayerDeath",&GameModeWrapper::LOnPlayerDeath,&GameModeWrapper::default_OnPlayerDeath);
	defGmBase.def("OnPlayerSpawned",&GameModeWrapper::LOnPlayerSpawned,&GameModeWrapper::default_OnPlayerSpawned);
	defGmBase.def("OnActionInput",&GameModeWrapper::LOnActionInput,&GameModeWrapper::default_OnActionInput);
	defGmBase.def("OnPlayerDropped",&GameModeWrapper::LOnPlayerDropped,&GameModeWrapper::default_OnPlayerDropped);
	defGmBase.def("OnPlayerReady",&GameModeWrapper::LOnPlayerReady,&GameModeWrapper::default_OnPlayerReady);
	defGmBase.def("OnPlayerJoined",&GameModeWrapper::LOnPlayerJoined,&GameModeWrapper::default_OnPlayerJoined);
	defGmBase.def("OnGameReady",&GameModeWrapper::LOnGameReady,&GameModeWrapper::default_OnGameReady);
	defGmBase.def("OnGameInitialized",&GameModeWrapper::LOnGameInitialized,&GameModeWrapper::default_OnGameInitialized);
	defGmBase.def("OnMapInitialized",&GameModeWrapper::LOnMapInitialized,&GameModeWrapper::default_OnMapInitialized);
	modGame[defGmBase];
	auto _G = luabind::globals(GetLuaState());
	_G["GMBase"] = _G["game"]["Base"];

#ifdef PHYS_ENGINE_BULLET

#elif PHYS_ENGINE_PHYSX
	lua_bind(luabind::class_<PhysObjHandle>("PhysObj")
		.def("IsValid",&Lua_PhysObj_IsValid)
		.def("SetLinearVelocity",&Lua_PhysObj_SetLinearVelocity)
		.def("GetLinearVelocity",&Lua_PhysObj_GetLinearVelocity)
		.def("AddLinearVelocity",&Lua_PhysObj_AddLinearVelocity)
		.def("SetAngularVelocity",&Lua_PhysObj_SetAngularVelocity)
		.def("GetAngularVelocity",&Lua_PhysObj_GetAngularVelocity)
		.def("AddAngularVelocity",&Lua_PhysObj_AddAngularVelocity)
		.def("PutToSleep",&Lua_PhysObj_PutToSleep)
		.def("WakeUp",&Lua_PhysObj_WakeUp)
		.def("SetGravityScale",&Lua_PhysObj_SetGravityScale)
		.def("SetGravityOverride",static_cast<void(*)(lua_State*,PhysObjHandle&,Vector3&,float)>(&Lua_PhysObj_SetGravityOverride))
		.def("SetGravityOverride",static_cast<void(*)(lua_State*,PhysObjHandle&,float)>(&Lua_PhysObj_SetGravityOverride))
		.def("SetGravityOverride",static_cast<void(*)(lua_State*,PhysObjHandle&)>(&Lua_PhysObj_SetGravityOverride))
		.def("GetGravityScale",&Lua_PhysObj_GetGravityScale)
		.def("GetGravityDirection",&Lua_PhysObj_GetGravityDirection)
		.def("GetGravity",&Lua_PhysObj_GetGravity)
		.def("GetActorBone",&Lua_PhysObj_GetActorBone)

		.def("GetActor",&Lua_PhysObj_GetActor)
		.def("GetActors",&Lua_PhysObj_GetActors)
	);
#endif
	auto &modMath = m_lua->RegisterLibrary("math");
	auto defPlane = luabind::class_<Plane>("Plane");
	defPlane.def(luabind::constructor<Vector3,Vector3,Vector3>());
	defPlane.def(luabind::constructor<Vector3,Vector3>());
	defPlane.def(luabind::constructor<Vector3,double>());
	defPlane.def("Copy",static_cast<void(*)(lua_State*,Plane&)>([](lua_State *l,Plane &plane) {
		Lua::Push<Plane>(l,Plane{plane});
	}));
	defPlane.def("GetNormal",&Lua_Plane_GetNormal);
	defPlane.def("GetPos",&Lua_Plane_GetPos);
	defPlane.def("GetDistance",static_cast<void(*)(lua_State*,Plane&)>(&Lua_Plane_GetDistance));
	defPlane.def("GetDistance",static_cast<void(*)(lua_State*,Plane&,const Vector3&)>(&Lua_Plane_GetDistance));
	defPlane.def("MoveToPos",&Lua_Plane_MoveToPos);
	defPlane.def("Rotate",&Lua_Plane_Rotate);
	defPlane.def("GetCenterPos",&Lua_Plane_GetCenterPos);
	defPlane.def("Transform",static_cast<void(*)(lua_State*,Plane&,const Mat4&)>([](lua_State *l,Plane &plane,const Mat4 &transform) {
		const auto &n = plane.GetNormal();
		auto p = n *static_cast<float>(plane.GetDistance());
		auto n0 = uvec::get_perpendicular(n);
		uvec::normalize(&n0);
		auto n1 = uvec::cross(n,n0);
		uvec::normalize(&n1);
		auto p04 = transform *Vector4{p.x,p.y,p.z,1.f};
		auto p1 = p +n0 *10.f;
		auto p14 = transform *Vector4{p1.x,p1.y,p1.z,1.f};
		auto p2 = p +n1 *10.f;
		auto p24 = transform *Vector4{p2.x,p2.y,p2.z,1.f};
		plane = Plane{Vector3{p04.x,p04.y,p04.z},Vector3{p14.x,p14.y,p14.z},Vector3{p24.x,p24.y,p24.z}};
	}));
	modMath[defPlane];
#ifdef PHYS_ENGINE_BULLET
	/*lua_bind(luabind::class_<btTransform>("Transform")
		.def(luabind::constructor<>())
		.def("SetOrigin",&Lua_btTransform_SetOrigin)
		.def("GetOrigin",&Lua_btTransform_GetOrigin)
		.def("SetRotation",&Lua_btTransform_SetRotation)
		.def("GetRotation",&Lua_btTransform_GetRotation)
		.def("SetIdentity",&Lua_btTransform_SetIdentity)
	);*/
#elif PHYS_ENGINE_PHYSX
	//lua_bind(luabind::class_<PtrPhysObjHandle>("PhysObj")
	lua_bind(luabind::class_<PhysXMaterial>("PhysXMaterial")
		.def("Release",&Lua_PhysXMaterial_Release)
	);
	lua_bind(luabind::class_<PhysXActor>("PhysXActor")
		.def("Release",&Lua_PhysXActor_Release)
		.def("GetActorFlags",&Lua_PhysXActor_GetActorFlags)
		.def("SetActorFlags",&Lua_PhysXActor_SetActorFlags)
		.def("GetType",&Lua_PhysXActor_GetType)
	);
	lua_bind(luabind::class_<PhysXJoint>("PhysXJoint")
		.def("Release",&Lua_PhysXJoint_Release)
	);
	lua_bind(luabind::class_<PhysXFixedJoint COMMA PhysXJoint>("PhysXFixedJoint")

	);
	lua_bind(luabind::class_<PhysXSphericalJoint COMMA PhysXJoint>("PhysXSphericalJoint")
		.def("SetLimitCone",static_cast<void(*)(lua_State*,PhysXSphericalJoint&,float,float,float)>(&Lua_PhysXSpericalJoint_SetLimitCone))
		.def("SetLimitCone",static_cast<void(*)(lua_State*,PhysXSphericalJoint&,float,float)>(&Lua_PhysXSpericalJoint_SetLimitCone))
		.def("EnableLimit",&Lua_PhysXSpericalJoint_EnableLimit)
	);
	lua_bind(luabind::class_<PhysXRevoluteJoint COMMA PhysXJoint>("PhysXRevoluteJoint")

	);
	lua_bind(luabind::class_<PhysXPrismaticJoint COMMA PhysXJoint>("PhysXPrismaticJoint")

	);
	lua_bind(luabind::class_<PhysXDistanceJoint COMMA PhysXJoint>("PhysXDistanceJoint")

	);
	lua_bind(luabind::class_<PhysXShape>("PhysXShape")

	);
	lua_bind(luabind::class_<PhysXRigidActor COMMA PhysXActor>("PhysXRigidActor")
		.def("GetPosition",&Lua_PhysXRigidActor_GetPosition)
		.def("GetOrientation",&Lua_PhysXRigidActor_GetOrientation)
		.def("SetPosition",&Lua_PhysXRigidActor_SetPosition)
		.def("SetOrientation",&Lua_PhysXRigidActor_SetOrientation)
	);
	lua_bind(luabind::class_<PhysXRigidDynamic COMMA PhysXRigidActor COMMA PhysXActor>("PhysXRigidDynamic")
		.def("AddForce",static_cast<void(*)(lua_State*,PhysXRigidDynamic&,Vector3*,int,bool)>(&Lua_PhysXRigidDynamic_AddForce))
		.def("AddForce",static_cast<void(*)(lua_State*,PhysXRigidDynamic&,Vector3*,int)>(&Lua_PhysXRigidDynamic_AddForce))
		.def("AddTorque",static_cast<void(*)(lua_State*,PhysXRigidDynamic&,Vector3*,int,bool)>(&Lua_PhysXRigidDynamic_AddTorque))
		.def("AddTorque",static_cast<void(*)(lua_State*,PhysXRigidDynamic&,Vector3*,int)>(&Lua_PhysXRigidDynamic_AddTorque))
		.def("ClearForce",&Lua_PhysXRigidDynamic_ClearForce)
		.def("ClearTorque",&Lua_PhysXRigidDynamic_ClearTorque)
		.def("GetAngularDamping",&Lua_PhysXRigidDynamic_GetAngularDamping)
		.def("GetAngularVelocity",&Lua_PhysXRigidDynamic_GetAngularVelocity)
		.def("GetLinearDamping",&Lua_PhysXRigidDynamic_GetLinearDamping)
		.def("GetLinearVelocity",&Lua_PhysXRigidDynamic_GetLinearVelocity)
		.def("GetMass",&Lua_PhysXRigidDynamic_GetMass)
		.def("GetMassSpaceInertiaTensor",&Lua_PhysXRigidDynamic_GetMassSpaceInertiaTensor)
		.def("GetMaxAngularVelocity",&Lua_PhysXRigidDynamic_GetMaxAngularVelocity)
		.def("SetAngularDamping",&Lua_PhysXRigidDynamic_SetAngularDamping)
		.def("SetAngularVelocity",&Lua_PhysXRigidDynamic_SetAngularVelocity)
		.def("SetLinearDamping",&Lua_PhysXRigidDynamic_SetLinearDamping)
		.def("SetLinearVelocity",&Lua_PhysXRigidDynamic_SetLinearVelocity)
		.def("SetMass",&Lua_PhysXRigidDynamic_SetMass)
		.def("SetMassSpaceInertiaTensor",&Lua_PhysXRigidDynamic_SetMassSpaceInertiaTensor)
		.def("SetMaxAngularVelocity",&Lua_PhysXRigidDynamic_SetMaxAngularVelocity)
		.def("SetMassAndUpdateInertia",&Lua_PhysXRigidDynamic_SetMassAndUpdateInertia)
	);
	lua_bind(luabind::class_<PhysXController>("PhysXController")
		.def("Release",&Lua_PhysXController_Release)
		.def("Move",&Lua_PhysXController_Move)
		.def("GetPosition",&Lua_PhysXController_GetPosition)
		.def("GetFootPosition",&Lua_PhysXController_GetFootPosition)
		.def("GetContactOffset",&Lua_PhysXController_GetContactOffset)
		.def("GetNonWalkableMode",&Lua_PhysXController_GetNonWalkableMode)
		.def("GetSlopeLimit",&Lua_PhysXController_GetSlopeLimit)
		.def("GetStepOffset",&Lua_PhysXController_GetStepOffset)
		.def("GetUpDirection",&Lua_PhysXController_GetUpDirection)
		.def("GetActor",&Lua_PhysXController_GetActor)
		.def("SetPosition",&Lua_PhysXController_SetPosition)
		.def("SetFootPosition",&Lua_PhysXController_SetFootPosition)
		.def("SetContactOffset",&Lua_PhysXController_SetContactOffset)
		.def("SetNonWalkableMode",&Lua_PhysXController_SetNonWalkableMode)
		.def("SetSlopeLimit",&Lua_PhysXController_SetSlopeLimit)
		.def("SetStepOffset",&Lua_PhysXController_SetStepOffset)
		.def("SetUpDirection",&Lua_PhysXController_SetUpDirection)
	);
	luabind::module(m_lua)
	[
		luabind::class_<PhysXScene>("PhysXScene")
		.def("ShiftOrigin",&Lua_PhysXScene_ShiftOrigin)
		.def("Release",&Lua_PhysXScene_Release)
		.def("SetFlag",&Lua_PhysXScene_SetFlag)
		.def("SetGravity",&Lua_PhysXScene_SetGravity)
		.def("GetGravity",Lua_PhysXScene_GetGravity)
		.def("Simulate",&Lua_PhysXScene_Simulate)
		.def("FetchResults",static_cast<void(*)(lua_State*,PhysXScene&,bool)>(&Lua_PhysXScene_FetchResults))
		.def("FetchResults",static_cast<void(*)(lua_State*,PhysXScene&)>(&Lua_PhysXScene_FetchResults))
		.def("GetActors",static_cast<void(*)(lua_State*,PhysXScene&,int)>(&Lua_PhysXScene_GetActors))
		.def("GetActors",static_cast<void(*)(lua_State*,PhysXScene&)>(&Lua_PhysXScene_GetActors))
		.def("AddActor",&Lua_PhysXScene_AddActor)
		.def("RemoveActor",&Lua_PhysXScene_RemoveActor)
		.def("RayCast",static_cast<void(*)(lua_State*,PhysXScene&,Vector3&,Vector3&,float,unsigned int,bool)>(&Lua_PhysXScene_RayCast))
		.def("RayCast",static_cast<void(*)(lua_State*,PhysXScene&,Vector3&,Vector3&,float,unsigned int)>(&Lua_PhysXScene_RayCast))
		.def("RayCast",static_cast<void(*)(lua_State*,PhysXScene&,Vector3&,Vector3&,float)>(&Lua_PhysXScene_RayCast))
		.def("RayCast",static_cast<void(*)(lua_State*,PhysXScene&,Vector3&,Vector3&,float,unsigned int,unsigned int)>(&Lua_PhysXScene_RayCast))
		.def("RayCast",static_cast<void(*)(lua_State*,PhysXScene&,Vector3&,Vector3&,float,unsigned int,luabind::object)>(&Lua_PhysXScene_RayCast))
	];
#endif
}

LuaEntityIterator Lua::ents::create_lua_entity_iterator(lua_State *l,luabind::object oFilter,uint32_t idxFilter,EntityIterator::FilterFlags filterFlags)
{
	auto r = LuaEntityIterator{l,filterFlags};
	if(idxFilter != std::numeric_limits<uint32_t>::max())
	{
		auto t = idxFilter;
		Lua::CheckTable(l,t);
		auto numFilters = Lua::GetObjectLength(l,t);
		for(auto i=decltype(numFilters){0u};i<numFilters;++i)
		{
			Lua::PushInt(l,i +1u);
			Lua::GetTableValue(l,t);

			auto *filter = Lua::CheckEntityIteratorFilter(l,-1);
			r.AttachFilter(*filter);

			Lua::Pop(l,1);
		}
	}
	return r;
}

void Game::RegisterLuaGameClasses(luabind::module_ &gameMod)
{
	auto &modEnts = GetLuaInterface().RegisterLibrary("ents");
	RegisterLuaEntityComponents(modEnts);
	modEnts[
		luabind::def("iterator",static_cast<LuaEntityIterator(*)(lua_State*)>([](lua_State *l) {
			return LuaEntityIterator{l};
		}),luabind::return_stl_iterator)
	];
	modEnts[
		luabind::def("iterator",static_cast<LuaEntityIterator(*)(lua_State*,luabind::object)>([](lua_State *l,luabind::object oFilterOrFlags) {
			auto filterFlags = EntityIterator::FilterFlags::Default;
			auto filterIdx = 1u;
			if(Lua::IsNumber(l,1))
			{
				filterFlags = static_cast<EntityIterator::FilterFlags>(Lua::CheckInt(l,1));
				filterIdx = std::numeric_limits<uint32_t>::max();
			}
			return Lua::ents::create_lua_entity_iterator(l,oFilterOrFlags,filterIdx,filterFlags);
		}),luabind::return_stl_iterator)
	];
	modEnts[
		luabind::def("iterator",static_cast<LuaEntityIterator(*)(lua_State*,luabind::object,luabind::object)>([](lua_State *l,luabind::object oFilterFlags,luabind::object oFilter) {
			Lua::CheckInt(l,1);
			auto filterFlags = static_cast<EntityIterator::FilterFlags>(Lua::CheckInt(l,1));
			return Lua::ents::create_lua_entity_iterator(l,oFilter,2u,filterFlags);
		}),luabind::return_stl_iterator)
	];

	auto defItFilter = luabind::class_<LuaEntityIteratorFilterBase>("IteratorFilter");
	modEnts[defItFilter];

	auto defItFilterClass = luabind::class_<LuaEntityIteratorFilterClass,LuaEntityIteratorFilterBase>("IteratorFilterClass");
	defItFilterClass.def(luabind::constructor<const std::string&>());
	defItFilterClass.def(luabind::constructor<const std::string&,bool>());
	defItFilterClass.def(luabind::constructor<const std::string&,bool,bool>());
	modEnts[defItFilterClass];

	auto defItFilterName = luabind::class_<LuaEntityIteratorFilterName,LuaEntityIteratorFilterBase>("IteratorFilterName");
	defItFilterName.def(luabind::constructor<const std::string&>());
	defItFilterName.def(luabind::constructor<const std::string&,bool>());
	defItFilterName.def(luabind::constructor<const std::string&,bool,bool>());
	modEnts[defItFilterName];

	auto defItFilterNameOrClass = luabind::class_<LuaEntityIteratorFilterNameOrClass,LuaEntityIteratorFilterBase>("IteratorFilterNameOrClass");
	defItFilterNameOrClass.def(luabind::constructor<const std::string&>());
	defItFilterNameOrClass.def(luabind::constructor<const std::string&,bool>());
	defItFilterNameOrClass.def(luabind::constructor<const std::string&,bool,bool>());
	modEnts[defItFilterNameOrClass];

	auto defItFilterEntity = luabind::class_<LuaEntityIteratorFilterEntity,LuaEntityIteratorFilterBase>("IteratorFilterEntity");
	defItFilterEntity.def(luabind::constructor<const std::string&>());
	modEnts[defItFilterEntity];

	auto defItFilterSphere = luabind::class_<LuaEntityIteratorFilterSphere,LuaEntityIteratorFilterBase>("IteratorFilterSphere");
	defItFilterSphere.def(luabind::constructor<const Vector3&,float>());
	modEnts[defItFilterSphere];

	auto defItFilterBox = luabind::class_<LuaEntityIteratorFilterBox,LuaEntityIteratorFilterBase>("IteratorFilterBox");
	defItFilterBox.def(luabind::constructor<const Vector3&,const Vector3&>());
	modEnts[defItFilterBox];

	auto defItFilterCone = luabind::class_<LuaEntityIteratorFilterCone,LuaEntityIteratorFilterBase>("IteratorFilterCone");
	defItFilterCone.def(luabind::constructor<const Vector3&,const Vector3&,float,float>());
	modEnts[defItFilterCone];

	auto defItFilterComponent = luabind::class_<LuaEntityIteratorFilterComponent,LuaEntityIteratorFilterBase>("IteratorFilterComponent");
	defItFilterComponent.def(luabind::constructor<pragma::ComponentId>());
	defItFilterComponent.def(luabind::constructor<lua_State*,const std::string&>());
	modEnts[defItFilterComponent];
	
	Lua::RegisterLibraryEnums(GetLuaState(),"ents",{
		{"ITERATOR_FILTER_BIT_NONE",umath::to_integral(EntityIterator::FilterFlags::None)},
		{"ITERATOR_FILTER_BIT_SPAWNED",umath::to_integral(EntityIterator::FilterFlags::Spawned)},
		{"ITERATOR_FILTER_BIT_PENDING",umath::to_integral(EntityIterator::FilterFlags::Pending)},
		{"ITERATOR_FILTER_BIT_INCLUDE_SHARED",umath::to_integral(EntityIterator::FilterFlags::IncludeShared)},
		{"ITERATOR_FILTER_BIT_INCLUDE_NETWORK_LOCAL",umath::to_integral(EntityIterator::FilterFlags::IncludeNetworkLocal)},

		{"ITERATOR_FILTER_BIT_CHARACTER",umath::to_integral(EntityIterator::FilterFlags::Character)},
		{"ITERATOR_FILTER_BIT_PLAYER",umath::to_integral(EntityIterator::FilterFlags::Player)},
		{"ITERATOR_FILTER_BIT_WEAPON",umath::to_integral(EntityIterator::FilterFlags::Weapon)},
		{"ITERATOR_FILTER_BIT_VEHICLE",umath::to_integral(EntityIterator::FilterFlags::Vehicle)},
		{"ITERATOR_FILTER_BIT_NPC",umath::to_integral(EntityIterator::FilterFlags::NPC)},
		{"ITERATOR_FILTER_BIT_PHYSICAL",umath::to_integral(EntityIterator::FilterFlags::Physical)},
		{"ITERATOR_FILTER_BIT_SCRIPTED",umath::to_integral(EntityIterator::FilterFlags::Scripted)},
		{"ITERATOR_FILTER_BIT_MAP_ENTITY",umath::to_integral(EntityIterator::FilterFlags::MapEntity)},

		{"ITERATOR_FILTER_BIT_HAS_TRANSFORM",umath::to_integral(EntityIterator::FilterFlags::HasTransform)},
		{"ITERATOR_FILTER_BIT_HAS_MODEL",umath::to_integral(EntityIterator::FilterFlags::HasModel)},

		{"ITERATOR_FILTER_ANY_TYPE",umath::to_integral(EntityIterator::FilterFlags::AnyType)},
		{"ITERATOR_FILTER_ANY",umath::to_integral(EntityIterator::FilterFlags::Any)},
		{"ITERATOR_FILTER_DEFAULT",umath::to_integral(EntityIterator::FilterFlags::Default)}
	});

	auto surfaceMatDef = luabind::class_<SurfaceMaterial>("SurfaceMaterial");
	surfaceMatDef.def(luabind::tostring(luabind::self));
	surfaceMatDef.def("GetName",&Lua::SurfaceMaterial::GetName);
	surfaceMatDef.def("GetIndex",&Lua::SurfaceMaterial::GetIndex);
	surfaceMatDef.def("GetFriction",&Lua::SurfaceMaterial::GetFriction);
	surfaceMatDef.def("SetFriction",&Lua::SurfaceMaterial::SetFriction);
	surfaceMatDef.def("GetRestitution",&Lua::SurfaceMaterial::GetRestitution);
	surfaceMatDef.def("SetRestitution",&Lua::SurfaceMaterial::SetRestitution);
	surfaceMatDef.def("GetFootstepSound",&Lua::SurfaceMaterial::GetFootstepType);
	surfaceMatDef.def("SetFootstepSound",&Lua::SurfaceMaterial::SetFootstepType);
	surfaceMatDef.def("SetImpactParticleEffect",&Lua::SurfaceMaterial::SetImpactParticleEffect);
	surfaceMatDef.def("GetImpactParticleEffect",&Lua::SurfaceMaterial::GetImpactParticleEffect);
	surfaceMatDef.def("GetBulletImpactSound",&Lua::SurfaceMaterial::GetBulletImpactSound);
	surfaceMatDef.def("SetBulletImpactSound",&Lua::SurfaceMaterial::SetBulletImpactSound);
	surfaceMatDef.def("SetHardImpactSound",&Lua::SurfaceMaterial::SetHardImpactSound);
	surfaceMatDef.def("GetHardImpactSound",&Lua::SurfaceMaterial::GetHardImpactSound);
	surfaceMatDef.def("SetSoftImpactSound",&Lua::SurfaceMaterial::SetSoftImpactSound);
	surfaceMatDef.def("GetSoftImpactSound",&Lua::SurfaceMaterial::GetSoftImpactSound);

	surfaceMatDef.def("SetAudioLowFrequencyAbsorption",&Lua::SurfaceMaterial::SetAudioLowFrequencyAbsorption);
	surfaceMatDef.def("GetAudioLowFrequencyAbsorption",&Lua::SurfaceMaterial::GetAudioLowFrequencyAbsorption);
	surfaceMatDef.def("SetAudioMidFrequencyAbsorption",&Lua::SurfaceMaterial::SetAudioMidFrequencyAbsorption);
	surfaceMatDef.def("GetAudioMidFrequencyAbsorption",&Lua::SurfaceMaterial::GetAudioMidFrequencyAbsorption);
	surfaceMatDef.def("SetAudioHighFrequencyAbsorption",&Lua::SurfaceMaterial::SetAudioHighFrequencyAbsorption);
	surfaceMatDef.def("GetAudioHighFrequencyAbsorption",&Lua::SurfaceMaterial::GetAudioHighFrequencyAbsorption);
	surfaceMatDef.def("SetAudioScattering",&Lua::SurfaceMaterial::SetAudioScattering);
	surfaceMatDef.def("GetAudioScattering",&Lua::SurfaceMaterial::GetAudioScattering);
	surfaceMatDef.def("SetAudioLowFrequencyTransmission",&Lua::SurfaceMaterial::SetAudioLowFrequencyTransmission);
	surfaceMatDef.def("GetAudioLowFrequencyTransmission",&Lua::SurfaceMaterial::GetAudioLowFrequencyTransmission);
	surfaceMatDef.def("SetAudioMidFrequencyTransmission",&Lua::SurfaceMaterial::SetAudioMidFrequencyTransmission);
	surfaceMatDef.def("GetAudioMidFrequencyTransmission",&Lua::SurfaceMaterial::GetAudioMidFrequencyTransmission);
	surfaceMatDef.def("SetAudioHighFrequencyTransmission",&Lua::SurfaceMaterial::SetAudioHighFrequencyTransmission);
	surfaceMatDef.def("GetAudioHighFrequencyTransmission",&Lua::SurfaceMaterial::GetAudioHighFrequencyTransmission);

	surfaceMatDef.def("GetNavigationFlags",static_cast<void(*)(lua_State*,SurfaceMaterial&)>([](lua_State *l,SurfaceMaterial &surfMat) {
		Lua::PushInt(l,umath::to_integral(surfMat.GetNavigationFlags()));
	}));
	surfaceMatDef.def("SetNavigationFlags",static_cast<void(*)(lua_State*,SurfaceMaterial&,uint32_t)>([](lua_State *l,SurfaceMaterial &surfMat,uint32_t navFlags) {
		surfMat.SetNavigationFlags(static_cast<pragma::nav::PolyFlags>(navFlags));
	}));
	surfaceMatDef.def("SetDensity",static_cast<void(*)(lua_State*,SurfaceMaterial&,float)>([](lua_State *l,SurfaceMaterial &surfMat,float density) {
		surfMat.SetDensity(density);
	}));
	surfaceMatDef.def("GetDensity",static_cast<void(*)(lua_State*,SurfaceMaterial&)>([](lua_State *l,SurfaceMaterial &surfMat) {
		Lua::PushNumber(l,surfMat.GetDensity());
	}));
	surfaceMatDef.def("SetLinearDragCoefficient",static_cast<void(*)(lua_State*,SurfaceMaterial&,float)>([](lua_State *l,SurfaceMaterial &surfMat,float coefficient) {
		surfMat.SetLinearDragCoefficient(coefficient);
	}));
	surfaceMatDef.def("GetLinearDragCoefficient",static_cast<void(*)(lua_State*,SurfaceMaterial&)>([](lua_State *l,SurfaceMaterial &surfMat) {
		Lua::PushNumber(l,surfMat.GetLinearDragCoefficient());
	}));
	surfaceMatDef.def("SetTorqueDragCoefficient",static_cast<void(*)(lua_State*,SurfaceMaterial&,float)>([](lua_State *l,SurfaceMaterial &surfMat,float coefficient) {
		surfMat.SetTorqueDragCoefficient(coefficient);
	}));
	surfaceMatDef.def("GetTorqueDragCoefficient",static_cast<void(*)(lua_State*,SurfaceMaterial&)>([](lua_State *l,SurfaceMaterial &surfMat) {
		Lua::PushNumber(l,surfMat.GetTorqueDragCoefficient());
	}));
	surfaceMatDef.def("SetWaveStiffness",static_cast<void(*)(lua_State*,SurfaceMaterial&,float)>([](lua_State *l,SurfaceMaterial &surfMat,float stiffness) {
		surfMat.SetWaveStiffness(stiffness);
	}));
	surfaceMatDef.def("GetWaveStiffness",static_cast<void(*)(lua_State*,SurfaceMaterial&)>([](lua_State *l,SurfaceMaterial &surfMat) {
		Lua::PushNumber(l,surfMat.GetWaveStiffness());
	}));
	surfaceMatDef.def("SetWavePropagation",static_cast<void(*)(lua_State*,SurfaceMaterial&,float)>([](lua_State *l,SurfaceMaterial &surfMat,float propagation) {
		surfMat.SetWavePropagation(propagation);
	}));
	surfaceMatDef.def("GetWavePropagation",static_cast<void(*)(lua_State*,SurfaceMaterial&)>([](lua_State *l,SurfaceMaterial &surfMat) {
		Lua::PushNumber(l,surfMat.GetWavePropagation());
	}));
	gameMod[surfaceMatDef];

	auto gibletCreateInfo = luabind::class_<GibletCreateInfo>("GibletCreateInfo");
	gibletCreateInfo.def(luabind::constructor<>());
	gibletCreateInfo.def_readwrite("model",&GibletCreateInfo::model);
	gibletCreateInfo.def_readwrite("skin",&GibletCreateInfo::skin);
	gibletCreateInfo.def_readwrite("scale",&GibletCreateInfo::scale);
	gibletCreateInfo.def_readwrite("mass",&GibletCreateInfo::mass);
	gibletCreateInfo.def_readwrite("lifetime",&GibletCreateInfo::lifetime);
	gibletCreateInfo.def_readwrite("position",&GibletCreateInfo::position);
	gibletCreateInfo.def_readwrite("rotation",&GibletCreateInfo::rotation);
	gibletCreateInfo.def_readwrite("velocity",&GibletCreateInfo::velocity);
	gibletCreateInfo.def_readwrite("angularVelocity",&GibletCreateInfo::angularVelocity);

	gibletCreateInfo.def_readwrite("physTranslationOffset",&GibletCreateInfo::physTranslationOffset);
	gibletCreateInfo.def_readwrite("physRotationOffset",&GibletCreateInfo::physRotationOffset);
	gibletCreateInfo.def_readwrite("physRadius",&GibletCreateInfo::physRadius);
	gibletCreateInfo.def_readwrite("physHeight",&GibletCreateInfo::physHeight);
	gibletCreateInfo.def_readwrite("physShape",reinterpret_cast<std::underlying_type_t<decltype(GibletCreateInfo::physShape)> GibletCreateInfo::*>(&GibletCreateInfo::physShape));
	gibletCreateInfo.add_static_constant("PHYS_SHAPE_MODEL",umath::to_integral(GibletCreateInfo::PhysShape::Model));
	gibletCreateInfo.add_static_constant("PHYS_SHAPE_NONE",umath::to_integral(GibletCreateInfo::PhysShape::None));
	gibletCreateInfo.add_static_constant("PHYS_SHAPE_SPHERE",umath::to_integral(GibletCreateInfo::PhysShape::Sphere));
	gibletCreateInfo.add_static_constant("PHYS_SHAPE_BOX",umath::to_integral(GibletCreateInfo::PhysShape::Box));
	gibletCreateInfo.add_static_constant("PHYS_SHAPE_CYLINDER",umath::to_integral(GibletCreateInfo::PhysShape::Cylinder));
	gameMod[gibletCreateInfo];

	auto bulletInfo = luabind::class_<BulletInfo>("BulletInfo");
	bulletInfo.def(luabind::constructor<>());
	bulletInfo.def_readwrite("spread",&BulletInfo::spread);
	bulletInfo.def_readwrite("force",&BulletInfo::force);
	bulletInfo.def_readwrite("distance",&BulletInfo::distance);
	bulletInfo.def_readwrite("damageType",reinterpret_cast<std::underlying_type_t<decltype(BulletInfo::damageType)> BulletInfo::*>(&BulletInfo::damageType));
	bulletInfo.def_readwrite("bulletCount",&BulletInfo::bulletCount);
	bulletInfo.def_readwrite("attacker",&BulletInfo::hAttacker);
	bulletInfo.def_readwrite("inflictor",&BulletInfo::hInflictor);
	bulletInfo.def_readwrite("tracerCount",&BulletInfo::tracerCount);
	bulletInfo.def_readwrite("tracerRadius",&BulletInfo::tracerRadius);
	bulletInfo.def_readwrite("tracerColor",&BulletInfo::tracerColor);
	bulletInfo.def_readwrite("tracerLength",&BulletInfo::tracerLength);
	bulletInfo.def_readwrite("tracerSpeed",&BulletInfo::tracerSpeed);
	bulletInfo.def_readwrite("tracerMaterial",&BulletInfo::tracerMaterial);
	bulletInfo.def_readwrite("tracerBloom",&BulletInfo::tracerBloom);
	bulletInfo.def_readwrite("ammoType",&BulletInfo::ammoType);
	bulletInfo.def_readwrite("direction",&BulletInfo::direction);
	bulletInfo.def_readwrite("effectOrigin",&BulletInfo::effectOrigin);
	bulletInfo.def_readwrite("damage",&BulletInfo::damage);
	gameMod[bulletInfo];

	auto classDefDamageInfo = luabind::class_<DamageInfo>("DamageInfo");
	classDefDamageInfo.def(luabind::constructor<>());
	classDefDamageInfo.def("SetDamage",&Lua::DamageInfo::SetDamage);
	classDefDamageInfo.def("AddDamage",&Lua::DamageInfo::AddDamage);
	classDefDamageInfo.def("ScaleDamage",&Lua::DamageInfo::ScaleDamage);
	classDefDamageInfo.def("GetDamage",&Lua::DamageInfo::GetDamage);
	classDefDamageInfo.def("GetAttacker",&Lua::DamageInfo::GetAttacker);
	classDefDamageInfo.def("SetAttacker",&Lua::DamageInfo::SetAttacker);
	classDefDamageInfo.def("GetInflictor",&Lua::DamageInfo::GetInflictor);
	classDefDamageInfo.def("SetInflictor",&Lua::DamageInfo::SetInflictor);
	classDefDamageInfo.def("GetDamageTypes",&Lua::DamageInfo::GetDamageTypes);
	classDefDamageInfo.def("SetDamageType",&Lua::DamageInfo::SetDamageType);
	classDefDamageInfo.def("AddDamageType",&Lua::DamageInfo::AddDamageType);
	classDefDamageInfo.def("RemoveDamageType",&Lua::DamageInfo::RemoveDamageType);
	classDefDamageInfo.def("IsDamageType",&Lua::DamageInfo::IsDamageType);
	classDefDamageInfo.def("SetSource",&Lua::DamageInfo::SetSource);
	classDefDamageInfo.def("GetSource",&Lua::DamageInfo::GetSource);
	classDefDamageInfo.def("SetHitPosition",&Lua::DamageInfo::SetHitPosition);
	classDefDamageInfo.def("GetHitPosition",&Lua::DamageInfo::GetHitPosition);
	classDefDamageInfo.def("SetForce",&Lua::DamageInfo::SetForce);
	classDefDamageInfo.def("GetForce",&Lua::DamageInfo::GetForce);
	classDefDamageInfo.def("GetHitGroup",&Lua::DamageInfo::GetHitGroup);
	classDefDamageInfo.def("SetHitGroup",&Lua::DamageInfo::SetHitGroup);
	gameMod[classDefDamageInfo];
}

#define LUA_MATRIX_MEMBERS_CLASSDEF(defMat,type) \
	defMat.def(luabind::constructor<>()); \
	defMat.def(luabind::constructor<float>()); \
	defMat.def(luabind::const_self /float()); \
	defMat.def(luabind::const_self *float()); \
	defMat.def(float() /luabind::const_self); \
	defMat.def(float() *luabind::const_self); \
	defMat.def(luabind::tostring(luabind::self)); \
	defMat.def(luabind::constructor<Mat2>()); \
	defMat.def(luabind::constructor<Mat2x3>()); \
	defMat.def(luabind::constructor<Mat2x4>()); \
	defMat.def(luabind::constructor<Mat3>()); \
	defMat.def(luabind::constructor<Mat3x2>()); \
	defMat.def(luabind::constructor<Mat3x4>()); \
	defMat.def(luabind::constructor<Mat4>()); \
	defMat.def(luabind::constructor<Mat4x2>()); \
	defMat.def(luabind::constructor<Mat4x3>()); \
	defMat.def("Copy",&Lua::Mat##type::Copy); \
	defMat.def("Get",&Lua::Mat##type::Get); \
	defMat.def("Set",static_cast<void(*)(lua_State*,Mat##type&,int,int,float)>(&Lua::Mat##type::Set)); \
	defMat.def("Transpose",&Lua::Mat##type::Transpose); \
	defMat.def("GetTranspose",&Lua::Mat##type::GetTransposition);

static void RegisterLuaMatrices(Lua::Interface &lua)
{
	auto &modMath = lua.RegisterLibrary("math");
	auto defMat2 = luabind::class_<Mat2>("Mat2");
	defMat2.def(luabind::constructor<float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat2,2);
	defMat2.def(luabind::const_self ==Mat2());
	defMat2.def(luabind::const_self +Mat2());
	defMat2.def(luabind::const_self -Mat2());
	defMat2.def(luabind::const_self *Mat2());
	defMat2.def(luabind::const_self *Vector2());
	defMat2.def("Inverse",&Lua::Mat2::Inverse);
	defMat2.def("GetInverse",&Lua::Mat2::GetInverse);
	defMat2.def("Set",static_cast<void(*)(lua_State*,::Mat2&,float,float,float,float)>(&Lua::Mat2::Set));
	defMat2.def("Set",static_cast<void(*)(lua_State*,::Mat2&,const ::Mat2&)>(&Lua::Mat2::Set));
	modMath[defMat2];
	
	auto defMat2x3 = luabind::class_<Mat2x3>("Mat2x3");
	defMat2x3.def(luabind::constructor<float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat2x3,2x3);
	defMat2x3.def(luabind::const_self ==Mat2x3());
	defMat2x3.def(luabind::const_self +Mat2x3());
	defMat2x3.def(luabind::const_self -Mat2x3());
	defMat2x3.def(luabind::const_self *Vector2());
	defMat2x3.def("Set",static_cast<void(*)(lua_State*,::Mat2x3&,float,float,float,float,float,float)>(&Lua::Mat2x3::Set));
	defMat2x3.def("Set",static_cast<void(*)(lua_State*,::Mat2x3&,const ::Mat2x3&)>(&Lua::Mat2x3::Set));
	modMath[defMat2x3];

	auto defMat2x4 = luabind::class_<Mat2x4>("Mat2x4");
	defMat2x4.def(luabind::constructor<float,float,float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat2x4,2x4);
	defMat2x4.def(luabind::const_self ==Mat2x4());
	defMat2x4.def(luabind::const_self +Mat2x4());
	defMat2x4.def(luabind::const_self -Mat2x4());
	defMat2x4.def(luabind::const_self *Vector2());
	defMat2x4.def("Set",static_cast<void(*)(lua_State*,::Mat2x4&,float,float,float,float,float,float,float,float)>(&Lua::Mat2x4::Set));
	defMat2x4.def("Set",static_cast<void(*)(lua_State*,::Mat2x4&,const ::Mat2x4&)>(&Lua::Mat2x4::Set));
	modMath[defMat2x4];

	auto defMat3 = luabind::class_<Mat3>("Mat3");
	defMat3.def(luabind::constructor<float,float,float,float,float,float,float,float,float>());
	defMat3.def(luabind::constructor<Quat>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat3,3);
	defMat3.def(luabind::const_self ==Mat3());
	defMat3.def(luabind::const_self +Mat3());
	defMat3.def(luabind::const_self -Mat3());
	defMat3.def(luabind::const_self *Mat3());
	defMat3.def(luabind::const_self *Vector3());
	defMat3.def("Inverse",&Lua::Mat3::Inverse);
	defMat3.def("GetInverse",&Lua::Mat3::GetInverse);
	defMat3.def("Set",static_cast<void(*)(lua_State*,::Mat3&,float,float,float,float,float,float,float,float,float)>(&Lua::Mat3::Set));
	defMat3.def("Set",static_cast<void(*)(lua_State*,::Mat3&,const ::Mat3&)>(&Lua::Mat3::Set));
	defMat3.def("CalcEigenValues",&Lua::Mat3::CalcEigenValues);
	modMath[defMat3];

	auto defMat3x2 = luabind::class_<Mat3x2>("Mat3x2");
	defMat3x2.def(luabind::constructor<float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat3x2,3x2);
	defMat3x2.def(luabind::const_self ==Mat3x2());
	defMat3x2.def(luabind::const_self +Mat3x2());
	defMat3x2.def(luabind::const_self -Mat3x2());
	defMat3x2.def(luabind::const_self *Vector3());
	defMat3x2.def("Set",static_cast<void(*)(lua_State*,::Mat3x2&,float,float,float,float,float,float)>(&Lua::Mat3x2::Set));
	defMat3x2.def("Set",static_cast<void(*)(lua_State*,::Mat3x2&,const ::Mat3x2&)>(&Lua::Mat3x2::Set));
	modMath[defMat3x2];

	auto defMat3x4 = luabind::class_<Mat3x4>("Mat3x4");
	defMat3x4.def(luabind::constructor<float,float,float,float,float,float,float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat3x4,3x4);
	defMat3x4.def(luabind::const_self ==Mat3x4());
	defMat3x4.def(luabind::const_self +Mat3x4());
	defMat3x4.def(luabind::const_self -Mat3x4());
	defMat3x4.def(luabind::const_self *Vector3());
	defMat3x4.def("Set",static_cast<void(*)(lua_State*,::Mat3x4&,float,float,float,float,float,float,float,float,float,float,float,float)>(&Lua::Mat3x4::Set));
	defMat3x4.def("Set",static_cast<void(*)(lua_State*,::Mat3x4&,const ::Mat3x4&)>(&Lua::Mat3x4::Set));
	modMath[defMat3x4];

	auto defMat4 = luabind::class_<Mat4>("Mat4");
	defMat4.def(luabind::constructor<float,float,float,float,float,float,float,float,float,float,float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat4,4);
	defMat4.def("Translate",&Lua::Mat4::Translate);
	defMat4.def("Rotate",static_cast<void(*)(lua_State*,::Mat4&,const EulerAngles&)>(&Lua::Mat4::Rotate));
	defMat4.def("Rotate",static_cast<void(*)(lua_State*,::Mat4&,const Vector3&,float)>(&Lua::Mat4::Rotate));
	defMat4.def("Scale",&Lua::Mat4::Scale);
	defMat4.def("ToEulerAngles",&Lua::Mat4::ToEulerAngles);
	defMat4.def("ToQuaternion",&Lua::Mat4::ToQuaternion);
	defMat4.def("Decompose",&Lua::Mat4::Decompose);
	defMat4.def(luabind::const_self ==Mat4());
	defMat4.def(luabind::const_self +Mat4());
	defMat4.def(luabind::const_self -Mat4());
	defMat4.def(luabind::const_self *Mat4());
	defMat4.def(luabind::const_self *Vector4());
	defMat4.def("Inverse",&Lua::Mat4::Inverse);
	defMat4.def("GetInverse",&Lua::Mat4::GetInverse);
	defMat4.def("Set",static_cast<void(*)(lua_State*,::Mat4&,float,float,float,float,float,float,float,float,float,float,float,float,float,float,float,float)>(&Lua::Mat4::Set));
	defMat4.def("Set",static_cast<void(*)(lua_State*,::Mat4&,const ::Mat4&)>(&Lua::Mat4::Set));
	modMath[defMat4];
	auto _G = luabind::globals(lua.GetState());
	_G["Mat4"] = _G["math"]["Mat4"];

	auto defMat4x2 = luabind::class_<Mat4x2>("Mat4x2");
	defMat4x2.def(luabind::constructor<float,float,float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat4x2,4x2);
	defMat4x2.def(luabind::const_self ==Mat4x2());
	defMat4x2.def(luabind::const_self +Mat4x2());
	defMat4x2.def(luabind::const_self -Mat4x2());
	defMat4x2.def(luabind::const_self *Vector4());
	defMat4x2.def("Set",static_cast<void(*)(lua_State*,::Mat4x2&,float,float,float,float,float,float,float,float)>(&Lua::Mat4x2::Set));
	defMat4x2.def("Set",static_cast<void(*)(lua_State*,::Mat4x2&,const ::Mat4x2&)>(&Lua::Mat4x2::Set));
	modMath[defMat4x2];

	auto defMat4x3 = luabind::class_<Mat4x3>("Mat4x3");
	defMat4x3.def(luabind::constructor<float,float,float,float,float,float,float,float,float,float,float,float>());
	LUA_MATRIX_MEMBERS_CLASSDEF(defMat4x3,4x3);
	defMat4x3.def(luabind::const_self ==Mat4x3());
	defMat4x3.def(luabind::const_self +Mat4x3());
	defMat4x3.def(luabind::const_self -Mat4x3());
	defMat4x3.def(luabind::const_self *Vector4());
	defMat4x3.def("Set",static_cast<void(*)(lua_State*,::Mat4x3&,float,float,float,float,float,float,float,float,float,float,float,float)>(&Lua::Mat4x3::Set));
	defMat4x3.def("Set",static_cast<void(*)(lua_State*,::Mat4x3&,const ::Mat4x3&)>(&Lua::Mat4x3::Set));
	modMath[defMat4x3];
}