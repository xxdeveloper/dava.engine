#include "Physics/Controllers/BoxCharacterControllerComponent.h"
#include "Physics/Core/BoxShapeComponent.h"
#include "Physics/Controllers/CapsuleCharacterControllerComponent.h"
#include "Physics/Core/CapsuleShapeComponent.h"
#include "Physics/Core/CollisionShapeComponent.h"
#include "Physics/CollisionSingleComponent.h"
#include "Physics/Core/ConvexHullShapeComponent.h"
#include "Physics/Core/HeightFieldShapeComponent.h"
#include "Physics/Core/MeshShapeComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/PhysicsConfigs.h"
#include "Physics/PhysicsGeometryCache.h"
#include "Physics/PhysicsModule.h"
#include "Physics/PhysicsSystem.h"
#include "Physics/Core/PhysicsUtils.h"
#include "Physics/Vehicles/PhysicsVehiclesSubsystem.h"
#include "Physics/Core/PlaneShapeComponent.h"
#include "Physics/Core/Private/PhysicsMath.h"
#include "Physics/Core/SphereShapeComponent.h"
#include "Physics/Vehicles/VehicleCarComponent.h"
#include "Physics/Vehicles/VehicleTankComponent.h"

#include <Base/Type.h>
#include <Debug/ProfilerCPU.h>
#include <Engine/Engine.h>
#include <Engine/EngineContext.h>
#include <Entity/Component.h>
#include <FileSystem/KeyedArchive.h>
#include <Logger/Logger.h>
#include <ModuleManager/ModuleManager.h>
#include <Reflection/ReflectionRegistrator.h>
#include <Render/Highlevel/Landscape.h>
#include <Render/Highlevel/RenderBatch.h>
#include <Render/Highlevel/RenderObject.h>
#include <Render/Highlevel/RenderSystem.h>
#include <Render/Highlevel/SkinnedMesh.h>
#include <Render/Highlevel/Landscape.h>
#include <Render/RenderHelper.h>
#include <Scene3D/Components/ComponentHelpers.h>
#include <Scene3D/Components/RenderComponent.h>
#include <Scene3D/Components/SingleComponents/TransformSingleComponent.h>
#include <Scene3D/Components/SwitchComponent.h>
#include <Scene3D/Components/TransformComponent.h>
#include <Scene3D/Components/SkeletonComponent.h>
#include <Scene3D/Entity.h>
#include <Scene3D/Scene.h>
#include <Utils/Utils.h>

#include <PxShared/foundation/PxAllocatorCallback.h>
#include <PxShared/foundation/PxFoundation.h>

#include <physx/PxRigidActor.h>
#include <physx/PxRigidDynamic.h>
#include <physx/PxScene.h>
#include <physx/common/PxRenderBuffer.h>

#include <functional>

// #include <../../../NetworkCore/Sources/NetworkCore/Scene3D/Components/SingleComponents/NetworkTimeSingleComponent.h>

namespace DAVA
{
DAVA_VIRTUAL_REFLECTION_IMPL(PhysicsSystem)
{
    ReflectionRegistrator<PhysicsSystem>::Begin()[M::Tags("base", "physics")]
    .ConstructorByPointer<Scene*>()
    // Run simulation at the end of a frame, after gameplay code, so that user always works with the same physics state during a frame. It also gives opportunity to enable multithreaded simulation in the future
    // Fetch results at the beginning of the next frame, before network snapshot system captures results
    // Copy changed transforms to physics right after network replication system, so that every entity we received this frame had correct state in physics
    .Method("ProcessFixedFetch", &PhysicsSystem::ProcessFixedFetch)[M::SystemProcess(SP::Group::ENGINE_BEGIN, SP::Type::FIXED, 4.0f)]
    .Method("ProcessFixedUpdateTransforms", &PhysicsSystem::ProcessFixedUpdateTransforms)[M::SystemProcess(SP::Group::ENGINE_BEGIN, SP::Type::FIXED, 21.0f)]
    .Method("ProcessFixedSimulate", &PhysicsSystem::ProcessFixedSimulate)[M::SystemProcess(SP::Group::ENGINE_END, SP::Type::FIXED, 1.0f)]
    .End();
}

void LogVehicleCar(VehicleCarComponent* carComponent, String const& header)
{
    using namespace physx;

    Entity* entity = carComponent->GetEntity();
    DVASSERT(entity != nullptr);

    DynamicBodyComponent* dynamicBodyComponent = entity->GetComponent<DynamicBodyComponent>();
    PxVehicleDriveNW* pxVehicle = static_cast<PxVehicleDriveNW*>(carComponent->GetPxVehicle());

    if (pxVehicle == nullptr)
        return;

    PxRigidDynamic* actor = pxVehicle->getRigidDynamicActor();
    if (pxVehicle == nullptr)
        return;

    std::stringstream ss;

    const Transform& t = entity->GetComponent<TransformComponent>()->GetWorldTransform();

    //ss << Format("VEHICLE STATE %s (frame: %d) =============", header.c_str(), carComponent->GetEntity()->GetScene()->GetSingleComponent<NetworkTimeSingleComponent>()->GetFrameId()) << "\n";

    ss << Format("Actor global position: (%.10e, %.10e, %.10e), global rotation: (%.10e, %.10e, %.10e, %.10e)", actor->getGlobalPose().p.x, actor->getGlobalPose().p.y, actor->getGlobalPose().p.z, actor->getGlobalPose().q.x, actor->getGlobalPose().q.y, actor->getGlobalPose().q.z, actor->getGlobalPose().q.w) << "\n";
    ss << Format("Entity transform: (%.10e, %.10e, %.10e), (%.10e, %.10e, %.10e, %.10e)", t.GetTranslation().x, t.GetTranslation().y, t.GetTranslation().z, t.GetRotation().x, t.GetRotation().y, t.GetRotation().z, t.GetRotation().w) << "\n";

    ss << Format("Actor mass: %f", actor->getMass()) << "\n";

    static const uint32 NUM_MAX_SHAPES = 5;
    PxShape* shapes[NUM_MAX_SHAPES];
    actor->getShapes(shapes, NUM_MAX_SHAPES);
    for (uint32 i = 0; i < NUM_MAX_SHAPES; ++i)
    {
        PxShape* shape = shapes[i];
        if (shape != nullptr)
        {
            ss << Format("Shape %d local position: (%.10e, %.10e, %.10e), local rotation: (%.10e, %.10e, %.10e, %.10e)", i, shape->getLocalPose().p.x, shape->getLocalPose().p.y, shape->getLocalPose().p.z, shape->getLocalPose().q.x, shape->getLocalPose().q.y, shape->getLocalPose().q.z, shape->getLocalPose().q.w) << "\n";

            CollisionShapeComponent* shapeComponent = static_cast<CollisionShapeComponent*>(shape->userData);
            ss << Format("Shape component %d local position: (%.10e, %.10e, %.10e), local rotation: (%.10e, %.10e, %.10e, %.10e)", i, shapeComponent->GetLocalPosition().x, shapeComponent->GetLocalPosition().y, shapeComponent->GetLocalPosition().z, shapeComponent->GetLocalOrientation().x, shapeComponent->GetLocalOrientation().y, shapeComponent->GetLocalOrientation().z, shapeComponent->GetLocalOrientation().w) << "\n";
        }
    }

    ss << Format("Linear velocity: %.10e, %.10e, %.10e", actor->getLinearVelocity().x, actor->getLinearVelocity().y, actor->getLinearVelocity().z) << "\n";
    ss << Format("Angular velocity: %.10e, %.10e, %.10e", actor->getAngularVelocity().x, actor->getAngularVelocity().y, actor->getAngularVelocity().z) << "\n";
    ss << Format("Engine speed: %f", pxVehicle->mDriveDynData.getEngineRotationSpeed()) << "\n";
    ss << Format("Gear: %u", pxVehicle->mDriveDynData.getCurrentGear()) << "\n";
    ss << Format("Gear change: %u", pxVehicle->mDriveDynData.getGearChange()) << "\n";
    ss << Format("Gear down: %u", (uint32)pxVehicle->mDriveDynData.getGearDown()) << "\n";
    ss << Format("Gear up: %u", (uint32)pxVehicle->mDriveDynData.getGearUp()) << "\n";
    ss << Format("Target gear: %u", pxVehicle->mDriveDynData.getTargetGear()) << "\n";
    ss << Format("Linear damping = %f, angular damping = %f", actor->getLinearDamping(), actor->getAngularDamping()) << "\n";
    ss << Format("CMass: (%f, %f, %f), (%f, %f, %f %f)", actor->getCMassLocalPose().p.x, actor->getCMassLocalPose().p.y, actor->getCMassLocalPose().p.z, actor->getCMassLocalPose().q.x, actor->getCMassLocalPose().q.y, actor->getCMassLocalPose().q.z, actor->getCMassLocalPose().q.w) << "\n";
    ss << Format("AABB: min (%f, %f, %f), max (%f, %f, %f)", actor->getWorldBounds().minimum.x, actor->getWorldBounds().minimum.y, actor->getWorldBounds().minimum.z, actor->getWorldBounds().maximum.x, actor->getWorldBounds().maximum.y, actor->getWorldBounds().maximum.z) << "\n";
    ss << Format("Computed forward speed: %f", pxVehicle->computeForwardSpeed()) << "\n";
    ss << Format("Computed sideway speed: %f", pxVehicle->computeSidewaysSpeed()) << "\n";
    ss << Format("Autobox switch time: %f", pxVehicle->mDriveDynData.getAutoBoxSwitchTime()) << "\n";
    ss << Format("Gear switch time: %f", pxVehicle->mDriveDynData.getGearSwitchTime()) << "\n";
    ss << Format("Use autogears: %u", (uint32)pxVehicle->mDriveDynData.getUseAutoGears()) << "\n";
    ss << Format("Num inputs: %u", pxVehicle->mDriveDynData.getNbAnalogInput()) << "\n";

    float32 jounces[4];
    pxVehicle->mWheelsDynData.getWheels4InternalJounces(jounces);

    for (uint32 i = 0; i < 4; ++i)
    {
        float rotationSpeed;
        float correctedRotationSpeed;
        pxVehicle->mWheelsDynData.getWheelRotationSpeed(i, rotationSpeed, correctedRotationSpeed);

        ss << Format("Wheel %u, rotation speed %f, corrected rotation speed %f", i, rotationSpeed, correctedRotationSpeed) << "\n";
        ss << Format("Wheel %u, rotation angle %f", i, pxVehicle->mWheelsDynData.getWheelRotationAngle(i)) << "\n";
    }

    for (uint32 i = 0; i < 4; ++i)
    {
        //ss << Format("Jounce %u, value %f (component value: %f)", i, jounces[i], carComponent->GetEntity()->GetComponent<VehicleWheelComponent>(i)->jounce) << "\n";
    }

    ss << Format("Component input: acceleration = %f", carComponent->GetAnalogAcceleration()) << "\n";
    ss << Format("Component input: steer = %f", carComponent->GetAnalogSteer()) << "\n";
    ss << Format("Component input: brake = %f", carComponent->GetAnalogBrake()) << "\n";
    ss << Format("PxVehicle analog input: steer = %f, %f", pxVehicle->mDriveDynData.getAnalogInput(PxVehicleDriveNWControl::eANALOG_INPUT_STEER_LEFT), pxVehicle->mDriveDynData.getAnalogInput(PxVehicleDriveNWControl::eANALOG_INPUT_STEER_RIGHT)) << "\n";
    ss << Format("PxVehicle analog input: accel = %f, brake = %f, handbrake = %f", pxVehicle->mDriveDynData.getAnalogInput(PxVehicleDriveNWControl::eANALOG_INPUT_ACCEL), pxVehicle->mDriveDynData.getAnalogInput(PxVehicleDriveNWControl::eANALOG_INPUT_BRAKE), pxVehicle->mDriveDynData.getAnalogInput(PxVehicleDriveNWControl::eANALOG_INPUT_HANDBRAKE)) << "\n";

    ss << Format("Actor is sleeping: %d", static_cast<uint32>(actor->isSleeping())) << "\n";
    ss << Format("Actor's wake counter: %f, sleep threshold: %f", actor->getWakeCounter(), actor->getSleepThreshold()) << "\n";

    ss << "===========================";

    Logger::Info(ss.str().c_str());
}

namespace PhysicsSystemDetail
{
template <typename T>
void EraseComponent(T* component, Vector<T*>& pendingComponents, Vector<T*>& components)
{
    auto addIter = std::find(pendingComponents.begin(), pendingComponents.end(), component);
    if (addIter != pendingComponents.end())
    {
        RemoveExchangingWithLast(pendingComponents, std::distance(pendingComponents.begin(), addIter));
    }
    else
    {
        auto iter = std::find(components.begin(), components.end(), component);
        if (iter != components.end())
        {
            RemoveExchangingWithLast(components, std::distance(components.begin(), iter));
        }
    }
}

bool IsCollisionShapeType(const Type* componentType)
{
    PhysicsModule* module = GetEngineContext()->moduleManager->GetModule<PhysicsModule>();
    const Vector<const Type*>& shapeComponents = module->GetShapeComponentTypes();
    return std::any_of(shapeComponents.begin(), shapeComponents.end(), [componentType](const Type* type) {
        return componentType == type;
    });
}

bool IsCharacterControllerType(const Type* componentType)
{
    return componentType->Is<BoxCharacterControllerComponent>() || componentType->Is<CapsuleCharacterControllerComponent>();
}

Vector3 AccumulateMeshInfo(Entity* e, Vector<PolygonGroup*>& groups)
{
    RenderObject* ro = GetRenderObject(e);
    if (ro != nullptr)
    {
        uint32 batchesCount = ro->GetRenderBatchCount();
        int32 maxLod = ro->GetMaxLodIndex();
        for (uint32 i = 0; i < batchesCount; ++i)
        {
            int32 lodIndex = -1;
            int32 switchIndex = -1;
            RenderBatch* batch = ro->GetRenderBatch(i, lodIndex, switchIndex);
            if (lodIndex == maxLod)
            {
                PolygonGroup* group = batch->GetPolygonGroup();
                if (group != nullptr)
                {
                    groups.push_back(group);
                }
            }
        }
    }

    return GetTransformComponent(e)->GetWorldTransform().GetScale();
}

PhysicsComponent* GetParentPhysicsComponent(Entity* entity)
{
    PhysicsComponent* physicsComponent = static_cast<PhysicsComponent*>(entity->GetComponent<StaticBodyComponent>());
    if (physicsComponent == nullptr)
    {
        physicsComponent = static_cast<PhysicsComponent*>(entity->GetComponent<DynamicBodyComponent>());
    }

    if (physicsComponent != nullptr)
    {
        return physicsComponent;
    }
    else
    {
        // Move up in the hierarchy
        Entity* parent = entity->GetParent();
        if (parent != nullptr)
        {
            return GetParentPhysicsComponent(parent);
        }
        else
        {
            return nullptr;
        }
    }
}

bool IsPhysicsEntity(Entity& entity, Component& componentToBeRemoved)
{
    PhysicsComponent* bodyComponent = PhysicsUtils::GetBodyComponent(&entity);
    if (bodyComponent != nullptr && bodyComponent != &componentToBeRemoved)
    {
        return true;
    }

    Vector<CollisionShapeComponent*> shapeComponents = PhysicsUtils::GetShapeComponents(&entity);
    if (shapeComponents.size() > 0)
    {
        if (shapeComponents.size() != 1 || shapeComponents[0] != &componentToBeRemoved)
        {
            return true;
        }
    }

    CharacterControllerComponent* cctComponent = PhysicsUtils::GetCharacterControllerComponent(&entity);
    if (cctComponent != nullptr && cctComponent != &componentToBeRemoved)
    {
        return true;
    }

    return false;
}

bool IsBodySleeping(PhysicsComponent* body)
{
    DVASSERT(body != nullptr);

    if (body->GetType()->Is<DynamicBodyComponent*>())
    {
        DynamicBodyComponent* dynamicBody = static_cast<DynamicBodyComponent*>(body);
        return dynamicBody->GetPxActor()->is<physx::PxRigidDynamic>()->isSleeping();
    }

    return false;
}

const uint32 DEFAULT_SIMULATION_BLOCK_SIZE = 16 * 1024 * 512;
} // namespace

physx::PxFilterFlags FilterShader(physx::PxFilterObjectAttributes attributes0,
                                  physx::PxFilterData filterData0,
                                  physx::PxFilterObjectAttributes attributes1,
                                  physx::PxFilterData filterData1,
                                  physx::PxPairFlags& pairFlags,
                                  const void* constantBlock,
                                  physx::PxU32 constantBlockSize)
{
    PX_UNUSED(attributes0);
    PX_UNUSED(attributes1);
    PX_UNUSED(constantBlockSize);
    PX_UNUSED(constantBlock);

    // PxFilterData for a shape is used this way:
    // - PxFilterData.word0 is used for engine-specific features (i.e. for CCD)
    // - PxFilterData.word1 is a bitmask for encoding type of object
    // - PxFilterData.word2 is a bitmask for encoding types of objects this object collides with
    // - PxFilterData.word3 is not used right now
    // Type of a shape and types it collides with can be set using CollisionShapeComponent::SetTypeMask and CollisionShapeComponent::SetTypeMaskToCollideWith methods

    if ((filterData0.word1 & filterData1.word2) == 0 &&
        (filterData1.word1 & filterData0.word2) == 0)
    {
        // If these types of objects do not collide, ignore this pair unless filter data for either of them changes
        return physx::PxFilterFlag::eSUPPRESS;
    }

    pairFlags =
    physx::PxPairFlag::eCONTACT_DEFAULT | // default collision processing
    physx::PxPairFlag::eNOTIFY_TOUCH_FOUND | // notify about a first contact
    physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS | // notify about ongoing contacts
    physx::PxPairFlag::eNOTIFY_CONTACT_POINTS; // report contact points

    if (CollisionShapeComponent::IsCCDEnabled(filterData0) || CollisionShapeComponent::IsCCDEnabled(filterData1))
    {
        pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT; // report continuous collision detection contacts
    }

    return physx::PxFilterFlag::eDEFAULT;
}

class CCTQueryFilterCallback final : public physx::PxQueryFilterCallback
{
    virtual physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData0, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags)
    {
        const physx::PxFilterData& filterData1 = shape->getSimulationFilterData();

        if ((filterData0.word1 & filterData1.word2) == 0 &&
            (filterData1.word1 & filterData0.word2) == 0)
        {
            return physx::PxQueryHitType::eNONE;
        }

        return physx::PxQueryHitType::eBLOCK;
    }

    virtual physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit)
    {
        // Post filter should be turned off
        DVASSERT(false);
        return physx::PxQueryHitType::eNONE;
    }
};

class CCTAndCCTQueryFilterCallback final : public physx::PxControllerFilterCallback
{
    bool filter(const physx::PxController& cct0, const physx::PxController& cct1) override
    {
        physx::PxShape* shape0 = nullptr;
        physx::PxShape* shape1 = nullptr;

        cct0.getActor()->getShapes(&shape0, 1, 0);
        DVASSERT(shape0 != nullptr);

        cct1.getActor()->getShapes(&shape1, 1, 0);
        DVASSERT(shape1 != nullptr);

        const physx::PxFilterData& filterData0 = shape0->getSimulationFilterData();
        const physx::PxFilterData& filterData1 = shape1->getSimulationFilterData();

        if ((filterData0.word1 & filterData1.word2) == 0 &&
            (filterData1.word1 & filterData0.word2) == 0)
        {
            return false;
        }

        return true;
    }
};

PhysicsSystem::SimulationEventCallback::SimulationEventCallback(DAVA::CollisionSingleComponent* targetCollisionSingleComponent)
    : targetCollisionSingleComponent(targetCollisionSingleComponent)
{
    DVASSERT(targetCollisionSingleComponent != nullptr);
}

void PhysicsSystem::SimulationEventCallback::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32)
{
}

void PhysicsSystem::SimulationEventCallback::onWake(physx::PxActor**, physx::PxU32)
{
}

void PhysicsSystem::SimulationEventCallback::onSleep(physx::PxActor**, physx::PxU32)
{
}

void PhysicsSystem::SimulationEventCallback::onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
{
    for (physx::PxU32 i = 0; i < count; ++i)
    {
        physx::PxTriggerPair& pair = pairs[i];

        if (pair.flags & (physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER |
                          physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
        {
            // ignore pairs when shapes have been deleted
            continue;
        }

        CollisionShapeComponent* triggerCollisionComponent = CollisionShapeComponent::GetComponent(pair.triggerShape);
        DVASSERT(triggerCollisionComponent);

        CollisionShapeComponent* otherCollisionComponent = CollisionShapeComponent::GetComponent(pair.otherShape);
        DVASSERT(otherCollisionComponent);

        Entity* triggerEntity = triggerCollisionComponent->GetEntity();
        DVASSERT(triggerEntity);

        Entity* otherEntity = otherCollisionComponent->GetEntity();
        DVASSERT(otherEntity);

        // Register trigger event
        TriggerInfo triggerInfo;
        triggerInfo.trigger = triggerEntity;
        triggerInfo.other = otherEntity;

        targetCollisionSingleComponent->activeTriggers.push_back(triggerInfo);
    }
}

void PhysicsSystem::SimulationEventCallback::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32)
{
}

void PhysicsSystem::SimulationEventCallback::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
{
    // Buffer for extracting physx contact points
    static const size_t MAX_CONTACT_POINTS_COUNT = 10;
    static physx::PxContactPairPoint physxContactPoints[MAX_CONTACT_POINTS_COUNT];

    for (physx::PxU32 i = 0; i < nbPairs; ++i)
    {
        const physx::PxContactPair& pair = pairs[i];

        if (pair.contactCount > 0)
        {
            if ((pair.flags & physx::PxContactPairFlag::eREMOVED_SHAPE_0) ||
                (pair.flags & physx::PxContactPairFlag::eREMOVED_SHAPE_1))
            {
                // Either first or second shape has been removed from the scene
                // Do not report such contacts
                continue;
            }

            // Extract physx points
            const physx::PxU32 contactPointsCount = pair.extractContacts(&physxContactPoints[0],
                                                                         MAX_CONTACT_POINTS_COUNT);
            DVASSERT(contactPointsCount > 0);

            Vector<CollisionPoint> davaContactPoints(contactPointsCount);

            // Convert each contact point from physx structure to engine structure
            for (size_t j = 0; j < contactPointsCount; ++j)
            {
                CollisionPoint& davaPoint = davaContactPoints[j];
                physx::PxContactPairPoint& physxPoint = physxContactPoints[j];

                davaPoint.position = PhysicsMath::PxVec3ToVector3(physxPoint.position);
                davaPoint.normal = PhysicsMath::PxVec3ToVector3(physxPoint.normal);
                davaPoint.impulse = PhysicsMath::PxVec3ToVector3(physxPoint.impulse);
            }

            Component* firstCollisionComponent = static_cast<Component*>(pair.shapes[0]->userData);
            DVASSERT(firstCollisionComponent != nullptr);

            Component* secondCollisionComponent = static_cast<Component*>(pair.shapes[1]->userData);
            DVASSERT(secondCollisionComponent != nullptr);

            Entity* firstEntity = firstCollisionComponent->GetEntity();
            DVASSERT(firstEntity != nullptr);

            Entity* secondEntity = secondCollisionComponent->GetEntity();
            DVASSERT(secondEntity != nullptr);

            // Register collision
            CollisionInfo collisionInfo;
            collisionInfo.first = firstEntity;
            collisionInfo.second = secondEntity;
            collisionInfo.points = std::move(davaContactPoints);
            targetCollisionSingleComponent->collisions.push_back(collisionInfo);
        }
    }
}

PhysicsSystem::PhysicsSystem(Scene* scene)
    : BaseSimulationSystem(scene, ComponentMask())
{
    Engine* engine = Engine::Instance();

    uint32 threadCount = 2;
    Vector3 gravity(0.0, 0.0, -9.81f);

    simulationBlockSize = PhysicsSystemDetail::DEFAULT_SIMULATION_BLOCK_SIZE;
    simulationEventCallback = SimulationEventCallback(scene->GetSingleComponentForWrite<CollisionSingleComponent>(this));

    if (engine != nullptr)
    {
        const KeyedArchive* options = engine->GetOptions();

        simulationBlockSize = options->GetUInt32("physics.simulationBlockSize", PhysicsSystemDetail::DEFAULT_SIMULATION_BLOCK_SIZE);
        DVASSERT((simulationBlockSize % (16 * 1024)) == 0); // simulationBlockSize must be 16K multiplier

        gravity = options->GetVector3("physics.gravity", gravity);
        threadCount = options->GetUInt32("physics.threadCount", threadCount);
    }

    const EngineContext* ctx = GetEngineContext();
    PhysicsModule* physics = ctx->moduleManager->GetModule<PhysicsModule>();
    simulationBlock = physics->Allocate(simulationBlockSize, "SimulationBlock", __FILE__, __LINE__);

    PhysicsSceneConfig sceneConfig;
    sceneConfig.gravity = gravity;
    sceneConfig.threadCount = threadCount;

    geometryCache = new PhysicsGeometryCache();

    physicsScene = physics->CreateScene(sceneConfig, FilterShader, &simulationEventCallback);

    vehiclesSubsystem = new PhysicsVehiclesSubsystem(scene, physicsScene);
    controllerManager = PxCreateControllerManager(*physicsScene);
    controllerManager->setOverlapRecoveryModule(false); // TODO: remove this when cars prediction is done

    // Component groups for all physics components
    staticBodies = scene->AquireComponentGroup<StaticBodyComponent, StaticBodyComponent>();
    dynamicBodies = scene->AquireComponentGroup<DynamicBodyComponent, DynamicBodyComponent>();
    shapes = scene->AquireComponentGroupWithMatcher<AnyOfEntityMatcher, BaseOfTypeMatcher,
                                                    CollisionShapeComponent,
                                                    BoxShapeComponent, SphereShapeComponent, CapsuleShapeComponent, PlaneShapeComponent, MeshShapeComponent, ConvexHullShapeComponent, HeightFieldShapeComponent>();
    boxCCTs = scene->AquireComponentGroup<BoxCharacterControllerComponent, BoxCharacterControllerComponent>();
    capsuleCCTs = scene->AquireComponentGroup<CapsuleCharacterControllerComponent, CapsuleCharacterControllerComponent>();

    staticBodies->onComponentRemoved->Connect(this, [this](StaticBodyComponent* body) { DeinitBodyComponent(body); });
    dynamicBodies->onComponentRemoved->Connect(this, [this](DynamicBodyComponent* body) { DeinitBodyComponent(body); });
    shapes->onComponentRemoved->Connect(this, [this](CollisionShapeComponent* shape) { DeinitShapeComponent(shape); });
    boxCCTs->onComponentRemoved->Connect(this, [this](BoxCharacterControllerComponent* cct) { DeinitCCTComponent(cct); });
    capsuleCCTs->onComponentRemoved->Connect(this, [this](CapsuleCharacterControllerComponent* cct) { DeinitCCTComponent(cct); });

    // Entity groups for render component dependent shapes
    convexHullAndRenderEntities = scene->AquireEntityGroup<RenderComponent, ConvexHullShapeComponent>();
    meshAndRenderEntities = scene->AquireEntityGroup<RenderComponent, MeshShapeComponent>();
    heightFieldAndRenderEntities = scene->AquireEntityGroup<RenderComponent, HeightFieldShapeComponent>();

    convexHullAndRenderEntities->onEntityAdded->Connect(this, &PhysicsSystem::OnRenderedEntityReady);
    convexHullAndRenderEntities->onEntityRemoved->Connect(this, &PhysicsSystem::OnRenderedEntityNotReady);
    meshAndRenderEntities->onEntityAdded->Connect(this, &PhysicsSystem::OnRenderedEntityReady);
    meshAndRenderEntities->onEntityRemoved->Connect(this, &PhysicsSystem::OnRenderedEntityNotReady);
    heightFieldAndRenderEntities->onEntityAdded->Connect(this, &PhysicsSystem::OnRenderedEntityReady);
    heightFieldAndRenderEntities->onEntityRemoved->Connect(this, &PhysicsSystem::OnRenderedEntityNotReady);

    // Component groups for newly created shapes
    staticBodiesPendingAdd = scene->AquireComponentGroupOnAdd(staticBodies, this);
    dynamicBodiesPendingAdd = scene->AquireComponentGroupOnAdd(dynamicBodies, this);
    shapesPendingAdd = scene->AquireComponentGroupOnAdd(shapes, this);
    boxCCTsPendingAdd = scene->AquireComponentGroupOnAdd(boxCCTs, this);
    capsuleCCTsPendingAdd = scene->AquireComponentGroupOnAdd(capsuleCCTs, this);

    SetDebugDrawEnabled(false);
}

PhysicsSystem::~PhysicsSystem()
{
    if (isSimulationRunning)
    {
        FetchResults(true);
    }

    SafeDelete(vehiclesSubsystem);

    DVASSERT(simulationBlock != nullptr);
    SafeDelete(geometryCache);

    controllerManager->release();

    const EngineContext* ctx = GetEngineContext();
    PhysicsModule* physics = ctx->moduleManager->GetModule<PhysicsModule>();
    physics->Deallocate(simulationBlock);
    simulationBlock = nullptr;
    physicsScene->release();

    staticBodies->onComponentRemoved->Disconnect(this);
    dynamicBodies->onComponentRemoved->Disconnect(this);
    shapes->onComponentRemoved->Disconnect(this);
    boxCCTs->onComponentRemoved->Disconnect(this);
    capsuleCCTs->onComponentRemoved->Disconnect(this);

    convexHullAndRenderEntities->onEntityAdded->Disconnect(this);
    convexHullAndRenderEntities->onEntityRemoved->Disconnect(this);
    meshAndRenderEntities->onEntityAdded->Disconnect(this);
    meshAndRenderEntities->onEntityRemoved->Disconnect(this);
    heightFieldAndRenderEntities->onEntityAdded->Disconnect(this);
    heightFieldAndRenderEntities->onEntityRemoved->Disconnect(this);
}

void PhysicsSystem::UnregisterEntity(Entity* entity)
{
    GetScene()->GetSingleComponentForWrite<CollisionSingleComponent>(this)->RemoveCollisionsWithEntity(entity);

    physicsEntities.erase(entity);
}

void PhysicsSystem::PrepareForRemove()
{
    physicsComponensUpdatePending.clear();
    collisionComponentsUpdatePending.clear();
    characterControllerComponentsUpdatePending.clear();
    teleportedCcts.clear();
    readyRenderedEntities.clear();
    physicsEntities.clear();

    for (CollisionShapeComponent* component : shapes->components)
    {
        DeinitShapeComponent(component);
    }

    ExecuteForEachBody(MakeFunction(this, &PhysicsSystem::DeinitBodyComponent));
    ExecuteForEachCCT(MakeFunction(this, &PhysicsSystem::DeinitCCTComponent));
}

void PhysicsSystem::ProcessFixed(float32 timeElapsed)
{
    ProcessFixedFetch(timeElapsed);
    ProcessFixedSimulate(timeElapsed);
}

void PhysicsSystem::ProcessFixedFetch(float32 timeElapsed)
{
    DAVA_PROFILER_CPU_SCOPE("PhysicsSystem::ProcessFixedFetch");

    MoveCharacterControllers(timeElapsed);
    FetchResults(true);

    SyncJointsTransformsWithPhysics();

    for (VehicleCarComponent* car : vehiclesSubsystem->cars->components)
    {
        vehiclesSubsystem->SaveSimulationParams(car);
    }
}

void PhysicsSystem::ProcessFixedUpdateTransforms(float32 timeElapsed)
{
    if (isSimulationEnabled)
    {
        SyncTransformToPhysx();
        SyncJointsTransformsWithPhysics();
    }
}

void PhysicsSystem::ProcessFixedSimulate(float32 timeElapsed)
{
    DAVA_PROFILER_CPU_SCOPE("PhysicsSystem::ProcessFixedSimulate");

    // If we're resimulating, unfreeze all dynamics,
    // and restore vehicles params
    // TODO: should probably refactor other physics components to work the same way?
    if (IsReSimulating())
    {
        for (DynamicBodyComponent* body : dynamicBodies->components)
        {
            DVASSERT(body != nullptr);
            if (frozenDynamicBodiesParams.count(body) != 0)
            {
                UnfreezeResimulatedBody(body);

                VehicleCarComponent* car = body->GetEntity()->GetComponent<VehicleCarComponent>();
                if (car != nullptr)
                {
                    vehiclesSubsystem->RestoreSimulationParams(car);
                }
            }
        }
    }

    InitNewObjects();
    UpdateComponents();

    if (isSimulationEnabled)
    {
        SyncTransformToPhysx();

        // If we're resimulating, restore wake counters
        // Have to do that after SyncTransformToPhysx since if transform was also updated from a snapshot,
        // wake counter was restored in UpdateActorGlobalPose
        if (IsReSimulating())
        {
            for (DynamicBodyComponent* body : dynamicBodies->components)
            {
                DVASSERT(body != nullptr);
                if (frozenDynamicBodiesParams.count(body) == 0)
                {
                    if (!body->GetIsKinematic())
                    {
                        physx::PxRigidActor* actor = body->GetPxActor();
                        if (actor != nullptr)
                        {
                            actor->is<physx::PxRigidDynamic>()->setWakeCounter(body->wakeCounter);
                        }
                    }
                }
            }
        }

        DrawDebugInfo();

        ApplyForces();

        // Teleported CCTs should have their filter data set to zero
        // to avoid colliding with other objects when physx applies speed to reach kinematic target
        for (auto cctInfo : teleportedCcts)
        {
            UpdateCCTFilterData(cctInfo.first, 0, 0);
        }

        vehiclesSubsystem->ProcessFixed(timeElapsed);
        physicsScene->simulate(timeElapsed, nullptr, simulationBlock, simulationBlockSize);

        // Restore CCT's original filter data
        for (auto cctInfo : teleportedCcts)
        {
            UpdateCCTFilterData(cctInfo.first, cctInfo.second.word1, cctInfo.second.word2);
        }
        teleportedCcts.clear();

        isSimulationRunning = true;
    }
    else
    {
        SyncTransformToPhysx();
    }
}

void PhysicsSystem::SetSimulationEnabled(bool isEnabled)
{
    if (isSimulationEnabled != isEnabled)
    {
        if (isSimulationRunning == true)
        {
            DVASSERT(isSimulationEnabled == true);
            bool success = FetchResults(true);
            DVASSERT(success == true);
        }

        isSimulationEnabled = isEnabled;

        vehiclesSubsystem->OnSimulationEnabled(isSimulationEnabled);
    }
}

bool PhysicsSystem::IsSimulationEnabled() const
{
    return isSimulationEnabled;
}

void PhysicsSystem::SetDebugDrawEnabled(bool drawDebugInfo_)
{
    drawDebugInfo = drawDebugInfo_;
    physx::PxReal enabled = drawDebugInfo == true ? 1.0f : 0.0f;
    physicsScene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_LIN_VELOCITY, enabled);
    physicsScene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_ANG_VELOCITY, enabled);
}

bool PhysicsSystem::IsDebugDrawEnabled() const
{
    return drawDebugInfo;
}

bool PhysicsSystem::FetchResults(bool waitForFetchFinish)
{
    auto updateEntityFromPhysxResults = [this](PhysicsComponent* physicsComponent)
    {
        Entity* entity = physicsComponent->GetEntity();

        physx::PxRigidActor* rigidActor = physicsComponent->GetPxActor();
        if (rigidActor == nullptr)
        {
            return;
        }

        // Update entity's transform and its shapes down the hierarchy recursively
        TransformComponent* transform = entity->GetComponent<TransformComponent>();
        transform->SetLocalTransform(Transform(PhysicsMath::PxVec3ToVector3(rigidActor->getGlobalPose().p), physicsComponent->currentScale, PhysicsMath::PxQuatToQuaternion(rigidActor->getGlobalPose().q)));

        Vector<CollisionShapeComponent*> shapes = PhysicsUtils::GetShapeComponents(entity);
        if (shapes.size() > 0)
        {
            for (CollisionShapeComponent* shape : shapes)
            {
                physx::PxShape* pxShape = shape->GetPxShape();

                if (pxShape != nullptr)
                {
                    physx::PxTransform transform = pxShape->getLocalPose();
                    shape->localPosition = PhysicsMath::PxVec3ToVector3(transform.p);
                    shape->localOrientation = PhysicsMath::PxQuatToQuaternion(transform.q);
                }
            }
        }

        Vector<Entity*> children;
        entity->GetChildEntitiesWithCondition(children, [physicsComponent](Entity* e) { return PhysicsSystemDetail::GetParentPhysicsComponent(e) == physicsComponent; });

        for (Entity* child : children)
        {
            DVASSERT(child != nullptr);

            shapes = PhysicsUtils::GetShapeComponents(child);
            if (shapes.size() > 0)
            {
                // Update entity using just first shape for now
                CollisionShapeComponent* shape = shapes[0];
                if (shape->GetPxShape() != nullptr)
                {
                    const physx::PxTransform shapeLocalPos = shape->GetPxShape()->getLocalPose();
                    TransformComponent* childTransform = child->GetComponent<TransformComponent>();
                    childTransform->SetLocalTransform(Transform(PhysicsMath::PxVec3ToVector3(shapeLocalPos.p), shape->scale, PhysicsMath::PxQuatToQuaternion(shapeLocalPos.q)));
                }
            }
        }

        if (physicsComponent->GetType()->Is<DynamicBodyComponent>())
        {
            DynamicBodyComponent* dynamicBodyComponent = static_cast<DynamicBodyComponent*>(physicsComponent);

            physx::PxRigidDynamic* dynamicActor = rigidActor->is<physx::PxRigidDynamic>();

            if (dynamicBodyComponent->GetIsKinematic() == false)
            {
                // Do not use SetLinearVelocity/SetAngularVelocity since it will trigger ScheduleUpdate which we do not need
                dynamicBodyComponent->linearVelocity = PhysicsMath::PxVec3ToVector3(dynamicActor->getLinearVelocity());
                dynamicBodyComponent->angularVelocity = PhysicsMath::PxVec3ToVector3(dynamicActor->getAngularVelocity());

                dynamicBodyComponent->wakeCounter = dynamicActor->getWakeCounter();
            }
        }

    };

    bool isFetched = false;

    if (isSimulationRunning)
    {
        isFetched = physicsScene->fetchResults(waitForFetchFinish);
        if (isFetched == true)
        {
            for (Entity* entity : previouslyActiveEntities)
            {
                // Only fetch results for previously active body if it's not active anymore
                // Otherwise, it will be fetched in the loop below
                // To avoid fetching results for the same entity twice
                PhysicsComponent* physicsComponent = PhysicsUtils::GetBodyComponent(entity);
                if (physicsComponent != nullptr && PhysicsSystemDetail::IsBodySleeping(physicsComponent))
                {
                    updateEntityFromPhysxResults(physicsComponent);
                }
            }
            previouslyActiveEntities.clear();

            isSimulationRunning = false;
            physx::PxU32 actorsCount = 0;
            physx::PxActor** actors = physicsScene->getActiveActors(actorsCount);

            for (physx::PxU32 i = 0; i < actorsCount; ++i)
            {
                physx::PxActor* actor = actors[i];

                Component* baseComponent = reinterpret_cast<Component*>(actor->userData);
                DVASSERT(baseComponent != nullptr);

                // When character controller is created, actor is created by physx implicitly
                // In this case there is no PhysicsComponent attached to this entity, only CharacterControllerComponent. We ignore those
                if (baseComponent->GetType()->Is<DynamicBodyComponent>() || baseComponent->GetType()->Is<StaticBodyComponent>())
                {
                    PhysicsComponent* physicsComponent = PhysicsComponent::GetComponent(actor);
                    DVASSERT(physicsComponent != nullptr);

                    updateEntityFromPhysxResults(physicsComponent);

                    previouslyActiveEntities.push_back(physicsComponent->GetEntity());
                }
            }
        }
    }

    return isFetched;
}

void PhysicsSystem::DrawDebugInfo()
{
    DVASSERT(isSimulationRunning == false);
    DVASSERT(isSimulationEnabled == true);
    if (IsDebugDrawEnabled() == false)
    {
        return;
    }

    RenderHelper* renderHelper = GetScene()->GetRenderSystem()->GetDebugDrawer();
    const physx::PxRenderBuffer& rb = physicsScene->getRenderBuffer();
    const physx::PxDebugLine* lines = rb.getLines();
    for (physx::PxU32 i = 0; i < rb.getNbLines(); ++i)
    {
        const physx::PxDebugLine& line = lines[i];
        renderHelper->DrawLine(PhysicsMath::PxVec3ToVector3(line.pos0), PhysicsMath::PxVec3ToVector3(line.pos1),
                               PhysicsMath::PxColorToColor(line.color0));
    }

    const physx::PxDebugTriangle* triangles = rb.getTriangles();
    for (physx::PxU32 i = 0; i < rb.getNbTriangles(); ++i)
    {
        const physx::PxDebugTriangle& triangle = triangles[i];
        Polygon3 polygon;
        polygon.AddPoint(PhysicsMath::PxVec3ToVector3(triangle.pos0));
        polygon.AddPoint(PhysicsMath::PxVec3ToVector3(triangle.pos1));
        polygon.AddPoint(PhysicsMath::PxVec3ToVector3(triangle.pos2));
        renderHelper->DrawPolygon(polygon, PhysicsMath::PxColorToColor(triangle.color0), RenderHelper::DRAW_WIRE_DEPTH);
    }

    const physx::PxDebugPoint* points = rb.getPoints();
    for (physx::PxU32 i = 0; i < rb.getNbPoints(); ++i)
    {
        const physx::PxDebugPoint& point = points[i];
        renderHelper->DrawIcosahedron(PhysicsMath::PxVec3ToVector3(point.pos), 5.0f, PhysicsMath::PxColorToColor(point.color), RenderHelper::DRAW_WIRE_DEPTH);
    }
}

void PhysicsSystem::InitNewObjects()
{
    ExecuteForEachPendingBody([this](PhysicsComponent* body) { InitBodyComponent(body); });
    for (CollisionShapeComponent* component : shapesPendingAdd->components)
    {
        InitShapeComponent(component);
    }
    shapesPendingAdd->components.clear();
    ExecuteForEachPendingCCT([this](CharacterControllerComponent* cct) { InitCCTComponent(cct); });

    for (Entity* e : readyRenderedEntities)
    {
        auto processRenderDependentShapes = [this](Entity* entity, const Type* shapeType)
        {
            uint32 numShapes = entity->GetComponentCount(shapeType);
            for (uint32 i = 0; i < numShapes; ++i)
            {
                CollisionShapeComponent* shapeComponent = static_cast<CollisionShapeComponent*>(entity->GetComponent(shapeType, i));

                if (shapeComponent->GetPxShape() == nullptr)
                {
                    InitShapeComponent(shapeComponent);
                    DVASSERT(shapeComponent->shape != nullptr);
                }
            }
        };

        processRenderDependentShapes(e, Type::Instance<ConvexHullShapeComponent>());
        processRenderDependentShapes(e, Type::Instance<MeshShapeComponent>());
        processRenderDependentShapes(e, Type::Instance<HeightFieldShapeComponent>());
    }
    readyRenderedEntities.clear();
}

void PhysicsSystem::AttachShape(PhysicsComponent* bodyComponent, CollisionShapeComponent* shapeComponent, const Vector3& scale)
{
    physx::PxActor* actor = bodyComponent->GetPxActor();
    DVASSERT(actor);
    physx::PxRigidActor* rigidActor = actor->is<physx::PxRigidActor>();
    DVASSERT(rigidActor);

    shapeComponent->scale = scale;

    physx::PxShape* shape = shapeComponent->GetPxShape();
    if (shape != nullptr)
    {
        rigidActor->attachShape(*shape);
        ScheduleUpdate(shapeComponent);
        ScheduleUpdate(bodyComponent);
    }
}

void PhysicsSystem::AttachShapesRecursively(Entity* entity, PhysicsComponent* bodyComponent, const Vector3& scale)
{
    for (const Type* type : GetEngineContext()->moduleManager->GetModule<PhysicsModule>()->GetShapeComponentTypes())
    {
        for (uint32 i = 0; i < entity->GetComponentCount(type); ++i)
        {
            AttachShape(bodyComponent, static_cast<CollisionShapeComponent*>(entity->GetComponent(type, i)), scale);
        }
    }

    const int32 childrenCount = entity->GetChildrenCount();
    for (int32 i = 0; i < childrenCount; ++i)
    {
        Entity* child = entity->GetChild(i);
        if (child->GetComponent<DynamicBodyComponent>() || child->GetComponent<StaticBodyComponent>())
        {
            continue;
        }

        AttachShapesRecursively(child, bodyComponent, scale);
    }
}

physx::PxShape* PhysicsSystem::CreateShape(CollisionShapeComponent* component, PhysicsModule* physics)
{
    using namespace PhysicsSystemDetail;
    physx::PxShape* shape = nullptr;

    const Type* componentType = component->GetType();

    if (componentType->Is<BoxShapeComponent>())
    {
        BoxShapeComponent* boxShape = static_cast<BoxShapeComponent*>(component);
        shape = physics->CreateBoxShape(boxShape->GetHalfSize(), component->GetMaterialName());
    }

    else if (componentType->Is<CapsuleShapeComponent>())
    {
        CapsuleShapeComponent* capsuleShape = static_cast<CapsuleShapeComponent*>(component);
        shape = physics->CreateCapsuleShape(capsuleShape->GetRadius(), capsuleShape->GetHalfHeight(), component->GetMaterialName());
    }

    else if (componentType->Is<SphereShapeComponent>())
    {
        SphereShapeComponent* sphereShape = static_cast<SphereShapeComponent*>(component);
        shape = physics->CreateSphereShape(sphereShape->GetRadius(), component->GetMaterialName());
    }

    else if (componentType->Is<PlaneShapeComponent>())
    {
        shape = physics->CreatePlaneShape(component->GetMaterialName());
    }

    // TODO: rebuild meshes for convex hull and mesh shapes when joint name is changed

    else if (componentType->Is<ConvexHullShapeComponent>())
    {
        Vector<PolygonGroup*> groups;
        Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);

        Entity* entity = component->GetEntity();
        DVASSERT(entity != nullptr);

        // If shape is binded to a joint, we use part of the mesh that associated with the joint
        bool boundToJoint = component->jointName.IsValid() && component->jointName.size() > 0;
        if (boundToJoint)
        {
            PolygonGroup* group = CreatePolygonGroupFromJoint(entity, component->jointName);
            if (group != nullptr)
            {
                groups.push_back(group);
            }
        }
        else
        {
            scale = AccumulateMeshInfo(entity, groups);
        }

        if (groups.empty() == false)
        {
            shape = physics->CreateConvexHullShape(std::move(groups), scale, component->GetMaterialName(), geometryCache);
        }
    }

    else if (componentType->Is<MeshShapeComponent>())
    {
        Vector<PolygonGroup*> groups;
        Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);

        Entity* entity = component->GetEntity();
        DVASSERT(entity != nullptr);

        // If shape is binded to a joint, we use part of the mesh that associated with the joint
        bool bindedToJoint = component->jointName.IsValid() && !component->jointName.empty();
        if (bindedToJoint)
        {
            groups.push_back(CreatePolygonGroupFromJoint(entity, component->jointName));
        }
        else
        {
            scale = AccumulateMeshInfo(entity, groups);
        }

        if (groups.empty() == false)
        {
            shape = physics->CreateMeshShape(std::move(groups), scale, component->GetMaterialName(), geometryCache);
        }
    }

    else if (componentType->Is<HeightFieldShapeComponent>())
    {
        Entity* entity = component->GetEntity();
        Landscape* landscape = GetLandscape(entity);
        if (landscape != nullptr)
        {
            DVASSERT(landscape->GetHeightmap() != nullptr);

            Matrix4 localPose;
            shape = physics->CreateHeightField(landscape, component->GetMaterialName(), localPose);
            component->SetLocalPosition(localPose.GetTranslationVector());
            component->SetLocalOrientation(localPose.GetRotation());
        }
    }

    else
    {
        DVASSERT(false);
    }

    if (shape != nullptr)
    {
        component->SetPxShape(shape);

        if (component->GetType()->Is<HeightFieldShapeComponent>() || component->GetType()->Is<PlaneShapeComponent>())
        {
            vehiclesSubsystem->SetupDrivableSurface(component);
        }
    }

    return shape;
}

void PhysicsSystem::SyncTransformToPhysx()
{
    const TransformSingleComponent* transformSingle = GetScene()->GetSingleComponentForRead<TransformSingleComponent>(this);
    for (Entity* entity : transformSingle->localTransformChanged)
    {
        // Check for perfomance reason: if this entity is not participating in physics simulation,
        // there is no need to handle it
        if (physicsEntities.find(entity) != physicsEntities.end())
        {
            CharacterControllerComponent* controllerComponent = PhysicsUtils::GetCharacterControllerComponent(entity);
            const physx::PxController* pxController = (controllerComponent == nullptr) ? nullptr : controllerComponent->GetPxController();

            physx::PxExtendedVec3 previousCctPos;
            if (pxController != nullptr)
            {
                previousCctPos = pxController->getFootPosition();
            }

            PhysicsUtils::CopyTransformToPhysics(entity);

            if (pxController != nullptr)
            {
                physx::PxExtendedVec3 newCctPos = pxController->getFootPosition();

                // If we moved CCT, remember it
                // Will be used later to disable any collision while we teleport it during simulate step
                if (!FLOAT_EQUAL(newCctPos.x, previousCctPos.x) ||
                    !FLOAT_EQUAL(newCctPos.y, previousCctPos.y) ||
                    !FLOAT_EQUAL(newCctPos.z, previousCctPos.z))
                {
                    physx::PxShape* cctShape = nullptr;
                    pxController->getActor()->getShapes(&cctShape, 1, 0);
                    DVASSERT(cctShape != nullptr);

                    teleportedCcts.insert({ controllerComponent, cctShape->getSimulationFilterData() });
                }
            }
        }
    }
}

void PhysicsSystem::ReleaseShape(CollisionShapeComponent* component)
{
    physx::PxShape* shape = component->GetPxShape();
    if (shape == nullptr)
    {
        return;
    }
    DVASSERT(shape->isExclusive() == true);

    physx::PxActor* actor = shape->getActor();
    if (actor != nullptr)
    {
        actor->is<physx::PxRigidActor>()->detachShape(*shape);
    }

    component->ReleasePxShape();
}

void PhysicsSystem::ScheduleUpdate(PhysicsComponent* component)
{
    physicsComponensUpdatePending.insert(component);
}

void PhysicsSystem::ScheduleUpdate(CollisionShapeComponent* component)
{
    collisionComponentsUpdatePending.insert(component);
}

void PhysicsSystem::ScheduleUpdate(CharacterControllerComponent* component)
{
    characterControllerComponentsUpdatePending.insert(component);
}

bool PhysicsSystem::Raycast(const Vector3& origin, const Vector3& direction, float32 distance, physx::PxRaycastCallback& callback, const physx::PxQueryFilterData& filterData, physx::PxQueryFilterCallback* filterCall)
{
    using namespace physx;

    return physicsScene->raycast(PhysicsMath::Vector3ToPxVec3(origin), PhysicsMath::Vector3ToPxVec3(Normalize(direction)),
                                 static_cast<PxReal>(distance), callback, PxHitFlags(PxHitFlag::eDEFAULT), filterData, filterCall);
}

PhysicsVehiclesSubsystem* PhysicsSystem::GetVehiclesSystem()
{
    return vehiclesSubsystem;
}

void PhysicsSystem::UpdateComponents()
{
    PhysicsModule* module = GetEngineContext()->moduleManager->GetModule<PhysicsModule>();
    for (CollisionShapeComponent* shapeComponent : collisionComponentsUpdatePending)
    {
        shapeComponent->UpdateLocalProperties();
        physx::PxShape* shape = shapeComponent->GetPxShape();
        DVASSERT(shape != nullptr);
        physx::PxMaterial* material = module->GetMaterial(shapeComponent->GetMaterialName());
        shape->setMaterials(&material, 1);
        physx::PxActor* actor = shape->getActor();
        if (actor != nullptr)
        {
            PhysicsComponent* bodyComponent = PhysicsComponent::GetComponent(actor);
            physicsComponensUpdatePending.insert(bodyComponent);
        }
    }

    for (PhysicsComponent* bodyComponent : physicsComponensUpdatePending)
    {
        bodyComponent->UpdateLocalProperties();

        // Recalculate mass
        // Ignore vehicles, VehiclesSubsystem is responsible for setting correct values

        Entity* entity = bodyComponent->GetEntity();
        if (entity->GetComponent<VehicleCarComponent>() == nullptr &&
            entity->GetComponent<VehicleTankComponent>() == nullptr)
        {
            physx::PxRigidDynamic* dynamicActor = bodyComponent->GetPxActor()->is<physx::PxRigidDynamic>();
            if (dynamicActor != nullptr)
            {
                physx::PxU32 shapesCount = dynamicActor->getNbShapes();
                if (shapesCount > 0)
                {
                    Vector<physx::PxShape*> shapes(shapesCount, nullptr);
                    physx::PxU32 extractedShapesCount = dynamicActor->getShapes(shapes.data(), shapesCount);
                    DVASSERT(shapesCount == extractedShapesCount);

                    Vector<physx::PxReal> masses;
                    masses.reserve(shapesCount);

                    for (physx::PxShape* shape : shapes)
                    {
                        CollisionShapeComponent* shapeComponent = CollisionShapeComponent::GetComponent(shape);
                        masses.push_back(shapeComponent->GetMass());
                    }

                    physx::PxRigidBodyExt::setMassAndUpdateInertia(*dynamicActor, masses.data(), static_cast<physx::PxU32>(masses.size()));
                }
            }
        }

        if (bodyComponent->GetType()->Is<DynamicBodyComponent>())
        {
            DynamicBodyComponent* dynamicBody = static_cast<DynamicBodyComponent*>(bodyComponent);
            bool isCCDEnabled = dynamicBody->IsCCDEnabled();

            physx::PxRigidDynamic* actor = dynamicBody->GetPxActor()->is<physx::PxRigidDynamic>();
            DVASSERT(actor != nullptr);

            physx::PxU32 shapesCount = actor->getNbShapes();
            for (physx::PxU32 shapeIndex = 0; shapeIndex < shapesCount; ++shapeIndex)
            {
                physx::PxShape* shape = nullptr;
                actor->getShapes(&shape, 1, shapeIndex);
                CollisionShapeComponent::SetCCDEnabled(shape, isCCDEnabled);
            }
        }
    }

    for (CharacterControllerComponent* controllerComponent : characterControllerComponentsUpdatePending)
    {
        physx::PxController* controller = controllerComponent->controller;
        if (controller != nullptr)
        {
            // Update geometry if needed
            if (controllerComponent->geometryChanged)
            {
                if (controllerComponent->GetType()->Is<BoxCharacterControllerComponent>())
                {
                    BoxCharacterControllerComponent* boxComponent = static_cast<BoxCharacterControllerComponent*>(controllerComponent);
                    physx::PxBoxController* boxController = static_cast<physx::PxBoxController*>(controller);

                    boxController->setHalfHeight(boxComponent->GetHalfHeight());
                    boxController->setHalfForwardExtent(boxComponent->GetHalfForwardExtent());
                    boxController->setHalfSideExtent(boxComponent->GetHalfSideExtent());
                }
                else if (controllerComponent->GetType()->Is<CapsuleCharacterControllerComponent>())
                {
                    CapsuleCharacterControllerComponent* capsuleComponent = static_cast<CapsuleCharacterControllerComponent*>(controllerComponent);
                    physx::PxCapsuleController* capsuleController = static_cast<physx::PxCapsuleController*>(controller);

                    capsuleController->setRadius(capsuleComponent->GetRadius());
                    capsuleController->setHeight(capsuleComponent->GetHeight());
                }

                controllerComponent->geometryChanged = false;
            }

            controller->setContactOffset(controllerComponent->GetContactOffset());

            // Teleport if needed
            if (controllerComponent->teleported)
            {
                controller->setFootPosition(PhysicsMath::Vector3ToPxExtendedVec3(controllerComponent->teleportDestination));
                controllerComponent->teleported = false;

                physx::PxShape* cctShape = nullptr;
                controller->getActor()->getShapes(&cctShape, 1, 0);
                DVASSERT(cctShape != nullptr);
                teleportedCcts.insert({ controllerComponent, cctShape->getSimulationFilterData() });
            }

            // Update filter data
            UpdateCCTFilterData(controllerComponent, controllerComponent->GetTypeMask(), controllerComponent->GetTypeMaskToCollideWith());
        }
    }

    collisionComponentsUpdatePending.clear();
    physicsComponensUpdatePending.clear();
    characterControllerComponentsUpdatePending.clear();
}

void PhysicsSystem::MoveCharacterControllers(float32 timeElapsed)
{
    ExecuteForEachCCT([this, timeElapsed](CharacterControllerComponent* controllerComponent)
                      {
                          physx::PxController* controller = controllerComponent->controller;
                          if (controller != nullptr)
                          {
                              // Apply movement

                              physx::PxShape* cctShape = nullptr;
                              controller->getActor()->getShapes(&cctShape, 1, 0);
                              DVASSERT(cctShape != nullptr);

                              beforeCCTMove.Emit(controllerComponent);

                              CCTQueryFilterCallback filterCallback;
                              CCTAndCCTQueryFilterCallback cctFilterCallback;

                              physx::PxFilterData filterData = cctShape->getSimulationFilterData();

                              physx::PxControllerFilters filter;
                              filter.mFilterCallback = &filterCallback;
                              filter.mCCTFilterCallback = &cctFilterCallback;
                              filter.mFilterData = &filterData;
                              filter.mFilterFlags = physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC;

                              physx::PxControllerCollisionFlags collisionFlags;
                              if (controllerComponent->GetMovementMode() == CharacterControllerComponent::MovementMode::Flying)
                              {
                                  collisionFlags = controller->move(PhysicsMath::Vector3ToPxVec3(controllerComponent->totalDisplacement), 0.0f, timeElapsed, filter);
                              }
                              else
                              {
                                  DVASSERT(controllerComponent->GetMovementMode() == CharacterControllerComponent::MovementMode::Walking);

                                  // Ignore displacement along z axis
                                  Vector3 displacement = controllerComponent->totalDisplacement;
                                  displacement.z = 0.0f;

                                  // Apply gravity
                                  displacement += PhysicsMath::PxVec3ToVector3(physicsScene->getGravity()) * timeElapsed;
                                  collisionFlags = controller->move(PhysicsMath::Vector3ToPxVec3(displacement), 0.0f, timeElapsed, filter);
                              }

                              controllerComponent->grounded = (collisionFlags & physx::PxControllerCollisionFlag::eCOLLISION_DOWN);
                              controllerComponent->totalDisplacement = Vector3::Zero;

                              // Sync entity's transform

                              Entity* entity = controllerComponent->GetEntity();
                              DVASSERT(entity != nullptr);

                              TransformComponent* transformComponent = entity->GetComponent<TransformComponent>();
                              DVASSERT(transformComponent != nullptr);

                              Vector3 newPosition = PhysicsMath::PxExtendedVec3ToVector3(controller->getFootPosition());
                              Vector3 const& oldPosition = transformComponent->GetWorldTransform().GetTranslation();
                              if (!FLOAT_EQUAL(newPosition.x, oldPosition.x) || !FLOAT_EQUAL(newPosition.y, oldPosition.y) || !FLOAT_EQUAL(newPosition.z, oldPosition.z))
                              {
                                  transformComponent->SetLocalTranslation(newPosition);
                              }

                              afterCCTMove.Emit(controllerComponent);
                          }
                      });
}

void PhysicsSystem::UpdateCCTFilterData(CharacterControllerComponent* cctComponent, uint32 typeMask, uint32 typeMaskToCollideWith)
{
    DVASSERT(cctComponent != nullptr);

    physx::PxController* controller = cctComponent->GetPxController();
    if (controller == nullptr)
    {
        return;
    }

    physx::PxShape* controllerShape = nullptr;
    controller->getActor()->getShapes(&controllerShape, 1, 0);
    DVASSERT(controllerShape != nullptr);

    // Setup word 0 to be the same for every CCT since physx filters out CCTs whose words do not intersect for some reason
    // See NpQueryShared.h, physx::applyFilterEquation function

    physx::PxFilterData simFilterData = controllerShape->getSimulationFilterData();
    simFilterData.word0 |= (1 << 31);
    simFilterData.word1 = typeMask;
    simFilterData.word2 = typeMaskToCollideWith;
    controllerShape->setSimulationFilterData(simFilterData);

    physx::PxFilterData queryFilterData = controllerShape->getQueryFilterData();
    queryFilterData.word0 |= (1 << 31);
    queryFilterData.word1 = typeMask;
    queryFilterData.word2 = typeMaskToCollideWith;
    controllerShape->setQueryFilterData(queryFilterData);

    cctComponent->controller->invalidateCache();
}

void PhysicsSystem::AddForce(DynamicBodyComponent* component, const Vector3& force, physx::PxForceMode::Enum mode)
{
    PendingForce pendingForce;
    pendingForce.component = component;
    pendingForce.force = force;
    pendingForce.mode = mode;

    forces.push_back(pendingForce);
}

void PhysicsSystem::ApplyForces()
{
    DAVA_PROFILER_CPU_SCOPE("PhysicsSystem::ApplyForces");

    for (const PendingForce& force : forces)
    {
        physx::PxActor* actor = force.component->GetPxActor();
        DVASSERT(actor != nullptr);
        physx::PxRigidBody* rigidBody = actor->is<physx::PxRigidBody>();
        DVASSERT(rigidBody != nullptr);

        rigidBody->addForce(PhysicsMath::Vector3ToPxVec3(force.force), force.mode);
    }

    forces.clear();
}

void PhysicsSystem::InitBodyComponent(PhysicsComponent* bodyComponent)
{
    PhysicsModule* physics = GetEngineContext()->moduleManager->GetModule<PhysicsModule>();
    DVASSERT(physics != nullptr);

    Entity* entity = bodyComponent->GetEntity();
    DVASSERT(entity != nullptr);

    // Body components are only allowed to be root objects
    DVASSERT(entity->GetParent() == entity->GetScene());

    // Create underlying PxActor
    physx::PxRigidActor* createdActor = nullptr;
    if (bodyComponent->GetType()->Is<StaticBodyComponent>())
    {
        createdActor = physics->CreateStaticActor();
    }
    else
    {
        DVASSERT(bodyComponent->GetType()->Is<DynamicBodyComponent>());
        createdActor = physics->CreateDynamicActor();
    }
    DVASSERT(createdActor != nullptr);

    bodyComponent->SetPxActor(createdActor);

    TransformComponent* transformComponent = entity->GetComponent<TransformComponent>();
    DVASSERT(transformComponent != nullptr);

    const Transform& transform = transformComponent->GetLocalTransform();

    PhysicsUtils::SetActorTransform(createdActor, transform.GetTranslation(), transform.GetRotation());
    bodyComponent->currentScale = transform.GetScale();

    AttachShapesRecursively(entity, bodyComponent, transform.GetScale());

    physicsScene->addActor(*createdActor);

    physicsEntities.insert(entity);
}

void PhysicsSystem::InitShapeComponent(CollisionShapeComponent* shapeComponent)
{
    PhysicsModule* physics = GetEngineContext()->moduleManager->GetModule<PhysicsModule>();
    DVASSERT(physics != nullptr);

    Entity* entity = shapeComponent->GetEntity();
    DVASSERT(entity != nullptr);

    physx::PxShape* shape = CreateShape(shapeComponent, physics);

    // Shape can be equal to null if it's a mesh or convex hull but no render component has been added to the entity yet
    // If it's null, we will return to this component later when render info is available
    if (shape != nullptr)
    {
        TransformComponent* transformComponent = entity->GetComponent<TransformComponent>();
        DVASSERT(transformComponent != nullptr);

        PhysicsComponent* physicsComponent = PhysicsSystemDetail::GetParentPhysicsComponent(entity);
        if (physicsComponent != nullptr)
        {
            AttachShape(physicsComponent, shapeComponent, transformComponent->GetLocalTransform().GetScale());

            if (physicsComponent->GetType()->Is<DynamicBodyComponent>())
            {
                physx::PxRigidDynamic* dynamicActor = physicsComponent->GetPxActor()->is<physx::PxRigidDynamic>();
                DVASSERT(dynamicActor != nullptr);

                if (dynamicActor->getActorFlags().isSet(physx::PxActorFlag::eDISABLE_SIMULATION) == false &&
                    dynamicActor->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC) == false)
                {
                    dynamicActor->wakeUp();
                }
            }
        }
        shape->release();
        physicsEntities.insert(entity);
    }
}

void PhysicsSystem::InitCCTComponent(CharacterControllerComponent* cctComponent)
{
    Entity* entity = cctComponent->GetEntity();
    DVASSERT(entity != nullptr);

    // Character controllers are only allowed to be root objects
    DVASSERT(entity->GetParent() == entity->GetScene());

    PhysicsModule* physics = GetEngineContext()->moduleManager->GetModule<PhysicsModule>();
    DVASSERT(physics != nullptr);

    TransformComponent* transformComp = entity->GetComponent<TransformComponent>();
    DVASSERT(transformComp != nullptr);

    physx::PxController* controller = nullptr;
    if (cctComponent->GetType()->Is<BoxCharacterControllerComponent>())
    {
        BoxCharacterControllerComponent* boxCharacterControllerComponent = static_cast<BoxCharacterControllerComponent*>(cctComponent);

        physx::PxBoxControllerDesc desc;
        desc.position = PhysicsMath::Vector3ToPxExtendedVec3(transformComp->GetLocalTransform().GetTranslation());
        desc.halfHeight = boxCharacterControllerComponent->GetHalfHeight();
        desc.halfForwardExtent = boxCharacterControllerComponent->GetHalfForwardExtent();
        desc.halfSideExtent = boxCharacterControllerComponent->GetHalfSideExtent();
        desc.upDirection = PhysicsMath::Vector3ToPxVec3(Vector3::UnitZ);
        desc.contactOffset = boxCharacterControllerComponent->GetContactOffset();
        desc.scaleCoeff = boxCharacterControllerComponent->GetScaleCoeff();
        desc.material = physics->GetMaterial(FastName());
        DVASSERT(desc.isValid());

        controller = controllerManager->createController(desc);
    }
    else if (cctComponent->GetType()->Is<CapsuleCharacterControllerComponent>())
    {
        CapsuleCharacterControllerComponent* capsuleCharacterControllerComponent = static_cast<CapsuleCharacterControllerComponent*>(cctComponent);

        physx::PxCapsuleControllerDesc desc;
        desc.position = PhysicsMath::Vector3ToPxExtendedVec3(transformComp->GetLocalTransform().GetTranslation());
        desc.radius = capsuleCharacterControllerComponent->GetRadius();
        desc.height = capsuleCharacterControllerComponent->GetHeight();
        desc.contactOffset = capsuleCharacterControllerComponent->GetContactOffset();
        desc.scaleCoeff = capsuleCharacterControllerComponent->GetScaleCoeff();
        desc.material = physics->GetMaterial(FastName());
        desc.upDirection = PhysicsMath::Vector3ToPxVec3(Vector3::UnitZ);
        DVASSERT(desc.isValid());

        controller = controllerManager->createController(desc);

        static_cast<physx::PxCapsuleController*>(controller)->setClimbingMode(physx::PxCapsuleClimbingMode::eCONSTRAINED);
    }

    DVASSERT(controller != nullptr);

    // CCT actor references dava component
    controller->getActor()->userData = static_cast<void*>(cctComponent);

    // CCT actor shapes also reference dava component
    physx::PxShape* cctShape = nullptr;
    controller->getActor()->getShapes(&cctShape, 1, 0);
    DVASSERT(cctShape != nullptr);
    cctShape->userData = static_cast<void*>(cctComponent);

    controller->setStepOffset(0.02f);

    cctComponent->controller = controller;

    UpdateCCTFilterData(cctComponent, cctComponent->GetTypeMask(), cctComponent->GetTypeMaskToCollideWith());

    physicsEntities.insert(entity);
}

void PhysicsSystem::DeinitBodyComponent(PhysicsComponent* bodyComponent)
{
    physicsComponensUpdatePending.erase(bodyComponent);

    physx::PxRigidActor* actor = bodyComponent->GetPxActor();
    if (actor != nullptr)
    {
        physx::PxU32 shapesCount = actor->getNbShapes();
        Vector<physx::PxShape*> shapes(shapesCount, nullptr);
        actor->getShapes(shapes.data(), shapesCount);

        for (physx::PxShape* shape : shapes)
        {
            DVASSERT(shape != nullptr);
            actor->detachShape(*shape);
        }

        physicsScene->removeActor(*actor);
        bodyComponent->ReleasePxActor();
    }

    if (bodyComponent->GetType()->Is<DynamicBodyComponent>())
    {
        size_t index = 0;
        while (index < forces.size())
        {
            PendingForce& force = forces[index];
            if (force.component == bodyComponent)
            {
                RemoveExchangingWithLast(forces, index);
            }
            else
            {
                ++index;
            }
        }
    }

    Entity* entity = bodyComponent->GetEntity();
    if (!PhysicsSystemDetail::IsPhysicsEntity(*entity, *bodyComponent))
    {
        physicsEntities.erase(entity);
    }
}

void PhysicsSystem::DeinitShapeComponent(CollisionShapeComponent* shapeComponent)
{
    DVASSERT(shapeComponent != nullptr);

    Entity* entity = shapeComponent->GetEntity();
    DVASSERT(entity != nullptr);

    collisionComponentsUpdatePending.erase(shapeComponent);

    ReleaseShape(shapeComponent);

    if (!PhysicsSystemDetail::IsPhysicsEntity(*entity, *shapeComponent))
    {
        physicsEntities.erase(entity);
    }
}

void PhysicsSystem::DeinitCCTComponent(CharacterControllerComponent* cctComponent)
{
    if (cctComponent->controller != nullptr)
    {
        cctComponent->controller->release();
    }

    Entity* entity = cctComponent->GetEntity();
    if (!PhysicsSystemDetail::IsPhysicsEntity(*entity, *cctComponent))
    {
        physicsEntities.erase(entity);
    }
}

void PhysicsSystem::OnRenderedEntityReady(Entity* entity)
{
    readyRenderedEntities.insert(entity);
}

void PhysicsSystem::OnRenderedEntityNotReady(Entity* entity)
{
    readyRenderedEntities.erase(entity);

    auto processRenderDependentShapes = [this, entity](const Type* componentType)
    {
        for (uint32 i = 0; i < entity->GetComponentCount(componentType); ++i)
        {
            CollisionShapeComponent* component = static_cast<CollisionShapeComponent*>(entity->GetComponent(componentType, i));
            ReleaseShape(component);
        }
    };

    processRenderDependentShapes(Type::Instance<ConvexHullShapeComponent>());
    processRenderDependentShapes(Type::Instance<MeshShapeComponent>());
    processRenderDependentShapes(Type::Instance<HeightFieldShapeComponent>());
}

void PhysicsSystem::ExecuteForEachBody(Function<void(PhysicsComponent*)> func)
{
    for (StaticBodyComponent* component : staticBodies->components)
    {
        func(component);
    }

    for (DynamicBodyComponent* component : dynamicBodies->components)
    {
        func(component);
    }
}

void PhysicsSystem::ExecuteForEachPendingBody(Function<void(PhysicsComponent*)> func)
{
    for (StaticBodyComponent* component : staticBodiesPendingAdd->components)
    {
        func(component);
    }

    for (DynamicBodyComponent* component : dynamicBodiesPendingAdd->components)
    {
        func(component);
    }

    staticBodiesPendingAdd->components.clear();
    dynamicBodiesPendingAdd->components.clear();
}

void PhysicsSystem::ExecuteForEachCCT(Function<void(CharacterControllerComponent*)> func)
{
    for (BoxCharacterControllerComponent* component : boxCCTs->components)
    {
        func(component);
    }

    for (CapsuleCharacterControllerComponent* component : capsuleCCTs->components)
    {
        func(component);
    }
}

void PhysicsSystem::ExecuteForEachPendingCCT(Function<void(CharacterControllerComponent*)> func)
{
    for (BoxCharacterControllerComponent* component : boxCCTsPendingAdd->components)
    {
        func(component);
    }

    for (CapsuleCharacterControllerComponent* component : capsuleCCTsPendingAdd->components)
    {
        func(component);
    }

    boxCCTsPendingAdd->components.clear();
    capsuleCCTsPendingAdd->components.clear();
}

PolygonGroup* PhysicsSystem::CreatePolygonGroupFromJoint(Entity* entity, const FastName& jointName)
{
    DVASSERT(entity != nullptr);
    DVASSERT(jointName.IsValid() && jointName.size() > 0);

    SkeletonComponent* skeletonComponent = entity->GetComponent<SkeletonComponent>();
    if (skeletonComponent == nullptr)
    {
        return nullptr;
    }

    uint32 jointIndex = skeletonComponent->GetJointIndex(jointName);
    RenderObject* renderObject = GetRenderObject(entity);
    DVASSERT(renderObject->GetType() == RenderObject::TYPE_SKINNED_MESH);

    SkinnedMesh* skinnedMesh = static_cast<SkinnedMesh*>(renderObject);
    DVASSERT(skinnedMesh != nullptr);

    // Counter for indices
    uint16 shapeIndex = 0;

    Vector<Vector3> objectSpaceVertices;

    Vector<Vector3> vertices;
    Vector<uint16> indices;

    for (uint32 i = 0; i < skinnedMesh->GetRenderBatchCount(); ++i)
    {
        RenderBatch* renderBatch = skinnedMesh->GetRenderBatch(i);
        SkinnedMesh::JointTargets const& jointTargets = skinnedMesh->GetJointTargets(renderBatch);

        uint32 jointTargetIndex = UINT32_MAX;
        for (uint32 j = 0; j < jointTargets.size(); ++j)
        {
            if (jointTargets[j] == jointIndex)
            {
                jointTargetIndex = j;
                break;
            }
        }

        if (jointTargetIndex == UINT32_MAX)
        {
            continue;
        }

        PolygonGroup* polygonGroup = renderBatch->GetPolygonGroup();
        for (int32 index = 0; index < polygonGroup->GetIndexCount(); index += 3)
        {
            int32 index1;
            int32 index2;
            int32 index3;

            polygonGroup->GetIndex(index, index1);
            polygonGroup->GetIndex(index + 1, index2);
            polygonGroup->GetIndex(index + 2, index3);

            int32 jointIndex;
            polygonGroup->GetHardJointIndex(index1, jointIndex);
            if (jointIndex == jointTargetIndex)
            {
                Vector3 vertex1;
                Vector3 vertex2;
                Vector3 vertex3;

                polygonGroup->GetCoord(index1, vertex1);
                polygonGroup->GetCoord(index2, vertex2);
                polygonGroup->GetCoord(index3, vertex3);

                objectSpaceVertices.push_back(vertex1);
                objectSpaceVertices.push_back(vertex2);
                objectSpaceVertices.push_back(vertex3);
            }
        }
    }

    AABBox3 shapeAabb;
    for (Vector3& v : objectSpaceVertices)
    {
        shapeAabb.AddPoint(v);
    }

    Vector3 center = shapeAabb.GetCenter();
    for (Vector3& v : objectSpaceVertices)
    {
        v -= center;

        vertices.push_back(v);
        indices.push_back(shapeIndex++);
    }

    PolygonGroup* polygonGroup = new PolygonGroup();
    polygonGroup->SetPrimitiveType(rhi::PrimitiveType::PRIMITIVE_TRIANGLELIST);
    polygonGroup->AllocateData(eVertexFormat::EVF_VERTEX, static_cast<int32>(vertices.size()), static_cast<int32>(indices.size()), 0);
    memcpy(polygonGroup->vertexArray, vertices.data(), vertices.size() * sizeof(Vector3));
    memcpy(polygonGroup->indexArray, indices.data(), indices.size() * sizeof(uint16));
    polygonGroup->BuildBuffers();
    polygonGroup->RecalcAABBox();

    return polygonGroup;
}

void PhysicsSystem::SyncJointsTransformsWithPhysics()
{
    for (DynamicBodyComponent* dynamicBody : dynamicBodies->components)
    {
        DVASSERT(dynamicBody != nullptr);

        Entity* entity = dynamicBody->GetEntity();
        DVASSERT(entity != nullptr);

        PhysicsUtils::SyncJointsTransformsWithPhysics(entity);
    }
}

void PhysicsSystem::FreezeEverything()
{
    DVASSERT(IsReSimulating());
    DVASSERT(frozenDynamicBodiesParams.size() == 0);

    for (DynamicBodyComponent* body : dynamicBodies->components)
    {
        Entity* currentEntity = body->GetEntity();
        DVASSERT(currentEntity != nullptr);

        if (body->GetIsKinematic() == false)
        {
            frozenDynamicBodiesParams[body] = std::make_tuple(body->GetLinearVelocity(), body->GetAngularVelocity());
            body->SetIsKinematic(true);
        }
    }
}

void PhysicsSystem::UnfreezeEverything()
{
    DVASSERT(IsReSimulating());

    for (auto& frozenDynamicBodyKvp : frozenDynamicBodiesParams)
    {
        DynamicBodyComponent* dynamicBody = frozenDynamicBodyKvp.first;
        DVASSERT(dynamicBody != nullptr);

        auto& params = frozenDynamicBodyKvp.second;

        dynamicBody->SetIsKinematic(false);
        dynamicBody->SetLinearVelocity(std::get<0>(params));
        dynamicBody->SetAngularVelocity(std::get<1>(params));
    }

    frozenDynamicBodiesParams.clear();
}

void PhysicsSystem::UnfreezeResimulatedBody(DynamicBodyComponent* body)
{
    DVASSERT(IsReSimulating());
    DVASSERT(body != nullptr && body->GetIsKinematic());

    auto it = frozenDynamicBodiesParams.find(body);
    DVASSERT(it != frozenDynamicBodiesParams.end());

    body->SetIsKinematic(false);

    // We do not restore velocity for a body we will resimulate
    // Since it was set by a snapshot system and should be updated in UpdateComponents

    frozenDynamicBodiesParams.erase(it);
}

void PhysicsSystem::ReSimulationStart()
{
    // Freeze everything
    // Entities that we will resimulate will be unfrozen in ProcessFixedSimulate
    FreezeEverything();
}

void PhysicsSystem::ReSimulationEnd()
{
    UnfreezeEverything();
}
} // namespace DAVA
