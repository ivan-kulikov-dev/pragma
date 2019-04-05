#include "stdafx_shared.h"
#include "luasystem.h"
#include "pragma/lua/classes/lphysics.h"
#include "pragma/physics/physenvironment.h"
#include "pragma/lua/classes/ldef_vector.h"
#include "pragma/lua/classes/ldef_quaternion.h"
#include "pragma/physics/collisionmesh.h"
#include "pragma/model/modelmesh.h"

extern DLLENGINE Engine *engine;

namespace Lua
{
	namespace PhysShape
	{
		static void GetBounds(lua_State *l,LPhysShape &shape);
		static void IsConvex(lua_State *l,LPhysShape &shape);
		static void IsConvexHull(lua_State *l,LPhysShape &shape);
		static void IsHeightField(lua_State *l,LPhysShape &shape);
		static void IsTriangleShape(lua_State *l,LPhysShape &shape);
	};
	namespace PhysConvexShape
	{
		static void GetCollisionMesh(lua_State *l,LPhysConvexHullShape &shape);
	};
	namespace PhysConvexHullShape
	{
		static void AddPoint(lua_State *l,LPhysConvexHullShape &shape,Vector3 &point);
	};
	namespace PhysHeightfield
	{
		static void GetHeight(lua_State *l,LPhysHeightfield &shape,uint32_t x,uint32_t y);
		static void SetHeight(lua_State *l,LPhysHeightfield &shape,uint32_t x,uint32_t y,float height);
		static void GetWidth(lua_State *l,LPhysHeightfield &shape);
		static void GetLength(lua_State *l,LPhysHeightfield &shape);
		static void GetMaxHeight(lua_State *l,LPhysHeightfield &shape);
		static void GetUpAxis(lua_State *l,LPhysHeightfield &shape);
	};
};

void Lua::PhysShape::register_class(lua_State *l,luabind::module_ &mod)
{
	auto classDef = luabind::class_<LPhysShape>("Shape");
	classDef.def("GetBounds",&GetBounds);
	classDef.def("IsConvex",&IsConvex);
	classDef.def("IsConvexHull",&IsConvexHull);
	classDef.def("IsHeightfield",&IsHeightField);
	classDef.def("IsTriangleShape",&IsTriangleShape);
	classDef.def("CalculateLocalInertia",static_cast<void(*)(lua_State*,LPhysShape&,float)>([](lua_State *l,LPhysShape &shape,float mass) {
		Vector3 localInertia;
		shape->CalculateLocalInertia(mass,&localInertia);
		Lua::Push<Vector3>(l,localInertia);
	}));
	mod[classDef];

	auto convexClassDef = luabind::class_<LPhysConvexShape,LPhysShape>("ConvexShape");
	convexClassDef.def("GetCollisionMesh",PhysConvexShape::GetCollisionMesh);
	mod[convexClassDef];

	auto hullClassDef = luabind::class_<LPhysConvexHullShape,LPhysConvexShape>("ConvexHullShape");
	hullClassDef.def("AddPoint",PhysConvexHullShape::AddPoint);
	mod[hullClassDef];

	auto heightfieldClassDef = luabind::class_<LPhysHeightfield,LPhysShape>("Heightfield");
	heightfieldClassDef.def("GetHeight",PhysHeightfield::GetHeight);
	heightfieldClassDef.def("SetHeight",PhysHeightfield::SetHeight);
	heightfieldClassDef.def("GetWidth",PhysHeightfield::GetWidth);
	heightfieldClassDef.def("GetLength",PhysHeightfield::GetLength);
	heightfieldClassDef.def("GetMaxHeight",PhysHeightfield::GetMaxHeight);
	heightfieldClassDef.def("GetUpAxis",PhysHeightfield::GetUpAxis);
	mod[heightfieldClassDef];

	auto triangleShapeClassDef = luabind::class_<LPhysTriangleShape,LPhysShape>("TriangleShape");
	triangleShapeClassDef.def("Test",static_cast<void(*)(lua_State*,LPhysTriangleShape&,EntityHandle&,PhysRigidBodyHandle&,std::shared_ptr<::ModelSubMesh>&,const Vector3&,float,float)>([](lua_State *l,LPhysTriangleShape &shape,EntityHandle &hEnt,PhysRigidBodyHandle &hBody,std::shared_ptr<::ModelSubMesh> &subMesh,const Vector3 &origin,float radius,float power) {
		auto *iva = static_cast<PhysTriangleShape*>(shape.get())->GetBtIndexVertexArray();
		if(iva == nullptr)
			return;
		uint8_t *vertexBase = nullptr;
		int32_t numVerts = 0;
		int32_t vertexStride = 0;
		PHY_ScalarType scalarType {};
		uint8_t *indexBase = nullptr;
		int32_t indexStride = 0;
		int32_t numFaces = 0;
		PHY_ScalarType indexType {};
		int32_t subPart = 0;
		iva->getLockedVertexIndexBase(&vertexBase,numVerts,scalarType,vertexStride,&indexBase,indexStride,numFaces,indexType,subPart);
		
		auto offset = 0u;
		for(auto i=decltype(numVerts){0};i<numVerts;++i)
		{
			switch(scalarType)
			{
				case PHY_ScalarType::PHY_FLOAT:
				{
					auto *v = reinterpret_cast<float*>(vertexBase +i *vertexStride);

					auto d = uvec::distance(origin,Vector3(v[0],v[1],v[2]) /static_cast<float>(PhysEnv::WORLD_SCALE));
					if(d < radius)
					{
						v[1] += (power *(1.f -(d /radius))) *PhysEnv::WORLD_SCALE;
					}
					//v[1] += 20.f *PhysEnv::WORLD_SCALE;
					break;
				}
				case PHY_ScalarType::PHY_DOUBLE:
				{
					auto *v = reinterpret_cast<double*>(vertexBase +i *vertexStride);

					auto d = uvec::distance(origin,Vector3(v[0],v[1],v[2]) /static_cast<float>(PhysEnv::WORLD_SCALE));
					if(d < radius)
					{
						v[1] += (power *(1.f -(d /radius))) *PhysEnv::WORLD_SCALE;
					}
					//v[1] += 20.0 *PhysEnv::WORLD_SCALE;
					break;
				}
			}
		}

		iva->unLockVertexBase(subPart);
		/*auto &verts = static_cast<PhysTriangleShape*>(shape.get())->GetVertices();
		for(auto &v : verts)
		{
			auto d = uvec::distance(origin,Vector3(v[0],v[1],v[2]) /static_cast<float>(PhysEnv::WORLD_SCALE));
			if(d < radius)
			{
				v[1] += (power *(1.f -(d /radius))) *PhysEnv::WORLD_SCALE;
			}
		}*/
		static_cast<PhysTriangleShape*>(shape.get())->GenerateInternalEdgeInfo();

		static_cast<PhysTriangleShape*>(shape.get())->Build();
		if(hBody.IsValid())
		{
			//PhysEnv *world = hEnt->GetNetworkState()->GetGameState()->GetPhysicsEnvironment();
			//auto *colObj = hBody->GetCollisionObject();
			//world->GetWorld()->removeCollisionObject(colObj);
			auto ptr = shape.GetSharedPointer();
			hBody->SetCollisionShape(ptr);
			//world->GetWorld()->addCollisionObject(colObj,umath::to_integral(hBody->GetCollisionFilterGroup()),umath::to_integral(hBody->GetCollisionFilterMask()));
		}

		auto &meshVerts = subMesh->GetVertices();
		for(auto &v : meshVerts)
		{
			auto d = uvec::distance(origin,v.position);
			if(d < radius)
			{
				v.position.y += (power *(1.f -(d /radius)));
			}
		}
		subMesh->Update(ModelUpdateFlags::UpdateVertexBuffer);
	}));
	mod[triangleShapeClassDef];
}
void Lua::PhysShape::GetBounds(lua_State *l,LPhysShape &shape)
{
	Vector3 min,max;
	shape->GetAABB(min,max);
	Lua::Push<Vector3>(l,min);
	Lua::Push<Vector3>(l,max);
}
void Lua::PhysShape::IsConvex(lua_State *l,LPhysShape &shape)
{
	Lua::PushBool(l,shape->IsConvex());
}
void Lua::PhysShape::IsConvexHull(lua_State *l,LPhysShape &shape)
{
	Lua::PushBool(l,shape->IsConvexHull());
}
void Lua::PhysShape::IsHeightField(lua_State *l,LPhysShape &shape)
{
	Lua::PushBool(l,shape->IsHeightfield());
}
void Lua::PhysShape::IsTriangleShape(lua_State *l,LPhysShape &shape)
{
	Lua::PushBool(l,shape->IsTriangleShape());
}

///////////////////////////////

void Lua::PhysConvexShape::GetCollisionMesh(lua_State *l,LPhysConvexHullShape &shape)
{
	auto *colMesh = static_cast<::PhysConvexShape*>(shape.get())->GetCollisionMesh();
	if(colMesh == nullptr)
		return;
	Lua::Push<std::shared_ptr<::CollisionMesh>>(l,colMesh->shared_from_this());
}

///////////////////////////////

void Lua::PhysConvexHullShape::AddPoint(lua_State*,LPhysConvexHullShape &shape,Vector3 &point)
{
	static_cast<::PhysConvexHullShape*>(shape.get())->AddPoint(point);
}

///////////////////////////////

void Lua::PhysHeightfield::GetHeight(lua_State *l,LPhysHeightfield &shape,uint32_t x,uint32_t y)
{
	Lua::PushInt(l,static_cast<::PhysHeightfield*>(shape.get())->GetHeight(x,y));
}
void Lua::PhysHeightfield::SetHeight(lua_State *l,LPhysHeightfield &shape,uint32_t x,uint32_t y,float height)
{
	static_cast<::PhysHeightfield*>(shape.get())->SetHeight(x,y,height);
}
void Lua::PhysHeightfield::GetWidth(lua_State *l,LPhysHeightfield &shape)
{
	Lua::PushInt(l,static_cast<::PhysHeightfield*>(shape.get())->GetWidth());
}
void Lua::PhysHeightfield::GetLength(lua_State *l,LPhysHeightfield &shape)
{
	Lua::PushInt(l,static_cast<::PhysHeightfield*>(shape.get())->GetLength());
}
void Lua::PhysHeightfield::GetMaxHeight(lua_State *l,LPhysHeightfield &shape)
{
	Lua::PushNumber(l,static_cast<::PhysHeightfield*>(shape.get())->GetMaxHeight());
}
void Lua::PhysHeightfield::GetUpAxis(lua_State *l,LPhysHeightfield &shape)
{
	Lua::PushInt(l,static_cast<::PhysHeightfield*>(shape.get())->GetUpAxis());
}
