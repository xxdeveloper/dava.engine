#pragma once

#include "Base/BaseMath.h"
#include "Base/BaseTypes.h"
#include "Base/UnordererMap.h"
#include "Reflection/Reflection.h"
#include "Scene3D/SkeletonAnimation/SkeletonPose.h"

#include "Private/MotionState.h"
#include "Private/MotionTransition.h"

namespace DAVA
{
class BlendTree;
class SkeletonComponent;
class YamlNode;
class Motion;
class MotionState;
class MotionStateSequence;
struct MotionTransitionInfo;

class Motion
{
    Motion();

public:
    enum eMotionBlend
    {
        BLEND_OVERRIDE,
        BLEND_ADD,
        BLEND_DIFF,
        BLEND_LERP,

        BLEND_COUNT
    };

    ~Motion();

    static Motion* LoadFromYaml(const YamlNode* motionNode);

    const FastName& GetName() const;
    eMotionBlend GetBlendMode() const;
    const SkeletonPose& GetCurrentSkeletonPose() const;
    const Vector3& GetCurrentRootOffsetDelta() const;

    bool RequestState(const FastName& stateID);
    const FastName& GetRequestedState() const;

    void Update(float32 dTime);

    void BindSkeleton(const SkeletonComponent* skeleton);

    const Vector<FastName>& GetParameterIDs() const;
    bool BindParameter(const FastName& parameterID, const float32* param);
    bool UnbindParameter(const FastName& parameterID);
    void UnbindParameters();

    const Vector<FastName>& GetStateIDs() const;

    const Vector<std::pair<FastName, FastName>> GetEndedPhases() const;

protected:
    uint32 GetTransitionIndex(const MotionState* srcState, const MotionState* dstState) const;
    MotionTransitionInfo* GetTransition(const MotionState* srcState, const MotionState* dstState) const;

    FastName name;
    eMotionBlend blendMode = BLEND_COUNT;

    Vector<MotionState> states;
    UnorderedMap<FastName, MotionState*> statesMap;
    Vector<MotionTransitionInfo*> transitions;

    Vector<FastName> statesIDs;
    Vector<FastName> parameterIDs;

    MotionStateSequence* primaryStateSequence = nullptr;
    MotionStateSequence* secondaryStateSequence = nullptr;
    MotionState* pendingState = nullptr;

    MotionTransition currentTransition; //use for transition between main and second
    bool transitionIsActive = false;

    Vector3 currentRootOffsetDelta;
    SkeletonPose currentPose;
    Vector<std::pair<FastName, FastName>> endedPhases; /*[state-id, phase-id]*/

    //////////////////////////////////////////////////////////////////////////
    //temporary for debug
    void SetStateID(const FastName& id)
    {
        RequestState(id);
    }
    //////////////////////////////////////////////////////////////////////////

    friend class MotionStateSequence;

    DAVA_REFLECTION(Motion);
};

inline const FastName& Motion::GetName() const
{
    return name;
}

inline Motion::eMotionBlend Motion::GetBlendMode() const
{
    return blendMode;
}

inline const SkeletonPose& Motion::GetCurrentSkeletonPose() const
{
    return currentPose;
}

inline const Vector3& Motion::GetCurrentRootOffsetDelta() const
{
    return currentRootOffsetDelta;
}

inline const Vector<FastName>& Motion::GetParameterIDs() const
{
    return parameterIDs;
}

inline const Vector<std::pair<FastName, FastName>> Motion::GetEndedPhases() const
{
    return endedPhases;
}

} //ns