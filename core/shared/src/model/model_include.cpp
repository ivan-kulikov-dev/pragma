#include "stdafx_shared.h"
#include "pragma/model/model.h"
#include "pragma/model/modelmesh.h"
#include "pragma/physics/collisionmesh.h"

#pragma optimize("",off)
static void subtract_frame(Frame &frame,const Frame &frameToSubtract)
{
	auto numBones = frameToSubtract.GetBoneCount(); // TODO
	for(auto i=decltype(numBones){0u};i<numBones;++i)
	{
		auto &pos = *frame.GetBonePosition(i);
		pos -= *frameToSubtract.GetBonePosition(i);
		auto inv = uquat::get_inverse(*frameToSubtract.GetBoneOrientation(i));
		uvec::rotate(&pos,inv);
		auto &rot = *frame.GetBoneOrientation(i);
		rot = inv *rot;
	}
}

static void add_frame(Frame &frame,const Frame &frameToAdd)
{
	auto numBones = frame.GetBoneCount(); // TODO
	for(auto i=decltype(numBones){0u};i<numBones;++i)
	{
		auto &rotAdd = *frameToAdd.GetBoneOrientation(i);
		auto &rot = *frame.GetBoneOrientation(i);
		rot = rotAdd *rot;

		auto &pos = *frame.GetBonePosition(i);
		uvec::rotate(&pos,rotAdd);
		pos += *frameToAdd.GetBonePosition(i);
	}
}

void Model::Merge(const Model &other,MergeFlags flags)
{
	std::vector<std::size_t> boneTranslations; // 'other' bone Id to 'this' bone Id
	auto bMerged = false;
	const auto fMergeSkeletons = [this,&other,&boneTranslations,&bMerged]() {
		if(bMerged == true)
			return;
		bMerged = true;
		auto &skeletonOther = other.GetSkeleton();
		auto &bonesOther = skeletonOther.GetBones();
		auto &skeleton = GetSkeleton();
		auto &bones = skeleton.GetBones();
		bones.reserve(bones.size() +bonesOther.size());
		boneTranslations.reserve(bones.size() +bonesOther.size());
		for(auto &boneOther : bonesOther)
		{
			auto it = std::find_if(bones.begin(),bones.end(),[&boneOther](const std::shared_ptr<Bone> &bone) {
				return (bone->name == boneOther->name) ? true : false;
			});
			if(it == bones.end())
			{
				auto boneId = skeleton.AddBone(new Bone());
				auto bone = skeleton.GetBone(boneId).lock();
				bone->name = boneOther->name;
				it = bones.end() -1;
			}
			boneTranslations.push_back(it -bones.begin());
		}
		// Determine new bone parents (Has to be done AFTER all bones have been processed!)
		for(auto idxOther=decltype(boneTranslations.size()){0};idxOther<boneTranslations.size();++idxOther)
		{
			auto &boneOther = bonesOther.at(idxOther);
			if(boneOther->parent.expired() == true)
				continue;
			auto idxThis = boneTranslations.at(idxOther);
			auto &boneThis = bones.at(idxThis);
			boneThis->parent = bones.at(boneTranslations.at(boneOther->parent.lock()->ID));
		}
	};

	if((flags &MergeFlags::BlendControllers) != MergeFlags::None)
	{
		auto &blendControllersOther = other.GetBlendControllers();
		auto &blendControllers = GetBlendControllers();
		blendControllers.reserve(blendControllers.size() +blendControllersOther.size());
		for(auto &blendController : blendControllersOther)
		{
			auto it = std::find_if(blendControllers.begin(),blendControllers.end(),[&blendController](const BlendController &bc) {
				return ustring::compare(bc.name,blendController.name,false);
			});
			if(it != blendControllers.end())
				continue;
			blendControllers.push_back(blendController);
		}
	}
	
	auto refAnim = GetAnimation(LookupAnimation("reference"));
	auto refFrame = refAnim ? refAnim->GetFrame(0) : nullptr;
	auto &skeleton = GetSkeleton();
	auto &bones = skeleton.GetBones();
	if((flags &MergeFlags::Animations) != MergeFlags::None)
	{
		fMergeSkeletons();
		auto &anims = GetAnimations();
		auto &animsOther = other.GetAnimations();
		anims.reserve(anims.size() +animsOther.size());
		auto animOffset = anims.size();
		for(auto i=decltype(animsOther.size()){0};i<animsOther.size();++i)
		{
			auto &animOther = animsOther.at(i);
			auto animName = other.GetAnimationName(i);
			
			auto shareMode = Animation::ShareMode::Events;
			if(other.GetSkeleton().GetBoneCount() >= skeleton.GetBoneCount()) // TODO: Check this properly! The included model has to have all bones of this model (it may also have more, but not fewer!)
			{
				// If the number of bones of the included model is less than this model, we have to make a copy of all animation frames and fill up the missing bones - Otherwise there may be animation issues!
				// TODO: Make missing bones always use transformations of reference animation, then this won't be necessary anymore!
				shareMode |= Animation::ShareMode::Frames;
			}
			auto anim = Animation::Create(*animOther,shareMode);
			auto &boneList = anim->GetBoneList();
			for(auto idx=decltype(boneList.size()){0};idx<boneList.size();++idx)
				anim->SetBoneId(idx,boneTranslations.at(boneList.at(idx)));

			if(anim->HasFlag(FAnim::Autoplay))
				anim->SetFlags(anim->GetFlags() &~FAnim::Autoplay); // TODO: Autoplay gesture animations cause issues in some cases (e.g. gman.wmd). Re-enable this once the issues have been taken care of!
			auto it = m_animationIDs.find(animName);
			if(it != m_animationIDs.end())
				;//anims.at(it->second) = anim;
			else
			{
				anims.push_back(anim);
				m_animationIDs.insert(decltype(m_animationIDs)::value_type(animName,anims.size() -1));

				if(refFrame != nullptr && anim->HasFlag(FAnim::Gesture) == false)
				{
					auto boneList = anim->GetBoneList();
					auto numBones = skeleton.GetBoneCount();
					boneList.reserve(numBones);
					for(auto i=boneList.size();i<numBones;++i)
						boneList.push_back(i);
					anim->SetBoneList(boneList);

					// Fill up all bone transforms for this animation that
					// might be missing but needed for the reference pose.
					for(auto &frame : anim->GetFrames())
					{
						auto numBonesFrame = frame->GetBoneCount();
						frame->SetBoneCount(numBones);
						for(auto boneId=numBonesFrame;boneId<(numBones -1);++boneId)
						{
							auto &bonePos = *refFrame->GetBonePosition(boneId);
							auto &boneRot = *refFrame->GetBoneOrientation(boneId);
							auto *boneScale = refFrame->GetBoneScale(boneId);
							frame->SetBonePosition(boneId,bonePos);
							frame->SetBoneOrientation(boneId,boneRot);
							if(boneScale != nullptr)
								frame->SetBoneScale(boneId,*boneScale);
						}
					}
				}
			}
		}

		// Update blend controllers
		std::unordered_map<std::string,uint32_t> *animIds = nullptr;
		GetAnimations(&animIds);
		for(auto i=animOffset;i<anims.size();++i)
		{
			auto &anim = anims.at(i);
			auto *animBc = anim->GetBlendController();
			if(animBc != nullptr)
			{
				auto *bc = other.GetBlendController(animBc->controller);
				if(bc == nullptr)
				{
					anim->ClearBlendController();
					Con::cwar<<"WARNING: Animation with invalid blend controller! Skipping..."<<Con::endl;
				}
				else
				{
					auto bcId = LookupBlendController(bc->name);
					if(bcId != -1)
					{
						animBc->controller = bcId;
						for(auto &transition : animBc->transitions)
						{
							auto animName = other.GetAnimationName(transition.animation);
							auto it = animIds->find(animName);
							if(it != animIds->end())
								transition.animation = it->second;
							else
							{
								Con::cwar<<"WARNING: Blend controller with invalid animation transition reference! Skipping..."<<Con::endl;
								transition.animation = -1;
							}
						}
					}
					else
						Con::cwar<<"WARNING: Unknown blend controller '"<<bc->name<<"'! Skipping..."<<Con::endl;
				}
			}
		}
	}

	if((flags &MergeFlags::Attachments) != MergeFlags::None)
	{
		fMergeSkeletons();
		auto &skeleton = GetSkeleton();
		auto &attachments = GetAttachments();
		auto &attachmentsOther = other.GetAttachments();
		auto &skeletonOther = other.GetSkeleton();
		auto &bonesOther = skeletonOther.GetBones();
		attachments.reserve(attachments.size() +attachmentsOther.size());
		for(auto &attOther : attachmentsOther)
		{
			auto it = std::find_if(attachments.begin(),attachments.end(),[&attOther](const Attachment &att) {
				return (att.name == attOther.name) ? true : false;
			});
			if(it != attachments.end())
			{
				auto &att = *it;
				att.angles = attOther.angles;
				att.offset = attOther.offset;
				att.bone = boneTranslations.at(attOther.bone);
				continue;
			}
			AddAttachment(attOther.name,boneTranslations.at(attOther.bone),attOther.offset,attOther.angles);
		}
	}
	if((flags &MergeFlags::Hitboxes) != MergeFlags::None)
	{
		fMergeSkeletons();
		auto &hitboxesOther = other.GetHitboxes();
		auto &hitboxes = GetHitboxes();
		hitboxes.reserve(hitboxes.size() +hitboxesOther.size());
		for(auto &pair : hitboxesOther)
		{
			auto &hitboxOther = pair.second;
			auto boneId = boneTranslations.at(pair.first);
			Hitbox hb(hitboxOther.group,hitboxOther.min,hitboxOther.max);
			auto it = hitboxes.find(boneId);
			if(it != hitboxes.end())
				it->second = hb;
			else
				hitboxes.insert(std::make_pair(boneId,hb));
		}
	}
	if((flags &MergeFlags::Joints) != MergeFlags::None)
	{
		fMergeSkeletons();
		auto &jointsOther = other.GetJoints();
		auto &joints = GetJoints();
		joints.reserve(joints.size() +jointsOther.size());
		for(auto &jointOther : jointsOther)
		{
			joints.push_back(JointInfo(jointOther.type,boneTranslations.at(jointOther.src),boneTranslations.at(jointOther.dest)));
			auto &joint = joints.back();
			joint.args = jointOther.args;
			joint.collide = jointOther.collide;
		}
	}
	if((flags &MergeFlags::CollisionMeshes) != MergeFlags::None)
	{
		fMergeSkeletons();
		auto &collisionMeshesOther = other.GetCollisionMeshes();
		auto &collisionMeshes = GetCollisionMeshes();
		collisionMeshes.reserve(collisionMeshes.size() +collisionMeshesOther.size());
		for(auto &colMeshOther : collisionMeshesOther)
		{
			auto colMesh = CollisionMesh::Create(*colMeshOther);
			auto boneParent = colMesh->GetBoneParent();
			if(boneParent >= 0)
				boneParent = boneTranslations.at(boneParent);
			colMesh->SetBoneParent(boneParent);
			collisionMeshes.push_back(colMesh);
		}
	}
	if((flags &MergeFlags::Meshes) != MergeFlags::None)
	{
		fMergeSkeletons();
		auto &meshGroupsOther = other.GetMeshGroups();
		auto &baseMeshesOther = other.GetBaseMeshes();
		auto &meshGroups = other.GetMeshGroups();
		auto &baseMeshes = other.GetBaseMeshes();
		for(auto i=decltype(meshGroupsOther.size()){0};i<meshGroupsOther.size();++i)
		{
			auto &groupOther = meshGroupsOther.at(i);
			std::shared_ptr<ModelMeshGroup> group = nullptr;
			if(i >= meshGroups.size())
				group = AddMeshGroup(groupOther->GetName());
			else
				group = meshGroups.at(i);
			for(auto &meshOther : groupOther->GetMeshes())
			{
				auto mesh = meshOther->Copy();
				for(auto &subMesh : mesh->GetSubMeshes())
				{
					subMesh = subMesh->Copy();
					// TODO: Update vertex weights
				}
			}
		}
	}
}
#pragma optimize("",on)
