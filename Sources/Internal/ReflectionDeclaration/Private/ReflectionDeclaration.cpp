#include "Engine/Engine.h"

#include "ReflectionDeclaration/ReflectionDeclaration.h"
#include "ReflectionDeclaration/Private/AnyCasts.h"
#include "ReflectionDeclaration/Private/AnyHashRegistration.h"

#include "Reflection/Reflection.h"
#include "Reflection/ReflectionRegistrator.h"

#include "Entity/Component.h"
#include "Math/AABBox3.h"
#include "Math/Color.h"
#include "Math/Rect.h"
#include "Math/Vector.h"
#include "Render/3D/PolygonGroup.h"
#include "Render/Highlevel/BillboardRenderObject.h"
#include "Render/Highlevel/DecalRenderObject.h"
#include "Render/Highlevel/Heightmap.h"
#include "Render/Highlevel/Landscape.h"
#include "Render/Highlevel/LandscapeSubdivision.h"
#include "Render/Highlevel/Light.h"
#include "Render/Highlevel/RenderBatch.h"
#include "Render/Highlevel/RenderObject.h"
#include "Render/Highlevel/SpeedTreeObject.h"
#include "Render/Highlevel/Vegetation/VegetationRenderObject.h"
#include "Scene3D/Components/ActionComponent.h"
#include "Scene3D/Components/AnimationComponent.h"
#include "Scene3D/Components/BulletComponent.h"
#include "Scene3D/Components/CameraComponent.h"
#include "Scene3D/Components/Controller/RotationControllerComponent.h"
#include "Scene3D/Components/Controller/SnapToLandscapeControllerComponent.h"
#include "Scene3D/Components/Controller/WASDControllerComponent.h"
#include "Scene3D/Components/CustomPropertiesComponent.h"
#include "Scene3D/Components/DebugRenderComponent.h"
#include "Scene3D/Components/DecalComponent.h"
#include "Scene3D/Components/EnvironmentComponent.h"
#include "Scene3D/Components/GeoDecalComponent.h"
#include "Scene3D/Components/LandscapeComponent.h"
#include "Scene3D/Components/LightComponent.h"
#include "Scene3D/Components/LightmapComponent.h"
#include "Scene3D/Components/LightmapDataComponent.h"
#include "Scene3D/Components/LightRenderComponent.h"
#include "Scene3D/Components/MeshComponent.h"
#include "Scene3D/Components/MotionComponent.h"
#include "Scene3D/Components/ParticleEffectComponent.h"
#include "Scene3D/Components/PostEffectComponent.h"
#include "Scene3D/Components/PostEffectDebugComponent.h"
#include "Scene3D/Components/PrefabComponent.h"
#include "Scene3D/Components/QualitySettingsComponent.h"
#include "Scene3D/Components/ReflectionComponent.h"
#include "Scene3D/Components/RenderComponent.h"
#include "Scene3D/Components/RuntimeEntityMarkComponent.h"
#include "Scene3D/Components/SkeletonComponent.h"
#include "Scene3D/Components/SlotComponent.h"
#include "Scene3D/Components/SoundComponent.h"
#include "Scene3D/Components/SpeedTreeComponent.h"
#include "Scene3D/Components/SplineComponent.h"
#include "Scene3D/Components/StaticOcclusionComponent.h"
#include "Scene3D/Components/SwitchComponent.h"
#include "Scene3D/Components/TextComponent.h"
#include "Scene3D/Components/TransformComponent.h"
#include "Scene3D/Components/TransformInterpolationComponent.h"
#include "Scene3D/Components/UpdatableComponent.h"
#include "Scene3D/Components/UserComponent.h"
#include "Scene3D/Components/VisibilityCheckComponent.h"
#include "Scene3D/Components/VTDecalComponent.h"
#include "Scene3D/Components/WaveComponent.h"
#include "Scene3D/Components/Waypoint/EdgeComponent.h"
#include "Scene3D/Components/Waypoint/PathComponent.h"
#include "Scene3D/Components/Waypoint/WaypointComponent.h"
#include "Scene3D/Components/WindComponent.h"
#include "Scene3D/Components/StreamingSettingsComponent.h"
#include "Scene3D/Components/SingleComponents/ActionsSingleComponent.h"
#include "Scene3D/Components/SingleComponents/ChangedSystemsSingleComponent.h"
#include "Scene3D/Components/SingleComponents/LightmapSingleComponent.h"
#include "Scene3D/Components/SingleComponents/MotionSingleComponent.h"
#include "Scene3D/Components/SingleComponents/PrefabSingleComponent.h"
#include "Scene3D/Components/SingleComponents/RenderObjectSingleComponent.h"
#include "Scene3D/Components/SingleComponents/TransformSingleComponent.h"
#include "Scene3D/Components/SingleComponents/VTSingleComponent.h"
#include "Scene3D/Entity.h"
#include "Scene3D/Lod/LodComponent.h"
#include "Scene3D/Lod/LodSystem.h"
#include "Entity/Component.h"
#include "Entity/ComponentManager.h"
#include "Entity/SceneSystem.h"
#include "Scene3D/Systems/ActionCollectSystem.h"
#include "Scene3D/Systems/ActionUpdateSystem.h"
#include "Scene3D/Systems/AnimationSystem.h"
#include "Scene3D/Systems/BaseSimulationSystem.h"
#include "Scene3D/Systems/Controller/RotationControllerSystem.h"
#include "Scene3D/Systems/Controller/SnapToLandscapeControllerSystem.h"
#include "Scene3D/Systems/Controller/WASDControllerSystem.h"
#include "Scene3D/Systems/DebugRenderSystem.h"
#include "Scene3D/Systems/DiffMonitoringSystem.h"
#include "Scene3D/Systems/EventSystem.h"
#include "Scene3D/Systems/FoliageSystem.h"
#include "Scene3D/Systems/GeoDecalSystem.h"
#include "Scene3D/Systems/LandscapeSystem.h"
#include "Scene3D/Systems/LightUpdateSystem.h"
#include "Scene3D/Systems/MotionSystem.h"
#include "Scene3D/Systems/ParticleEffectDebugDrawSystem.h"
#include "Scene3D/Systems/ParticleEffectSystem.h"
#include "Scene3D/Systems/PrefabLoadingSystem.h"
#include "Scene3D/Systems/RenderUpdateSystem.h"
#include "Scene3D/Systems/SkeletonSystem.h"
#include "Scene3D/Systems/SlotSystem.h"
#include "Scene3D/Systems/SoundUpdateSystem.h"
#include "Scene3D/Systems/SpeedTreeUpdateSystem.h"
#include "Scene3D/Systems/StaticOcclusionBuildSystem.h"
#include "Scene3D/Systems/StaticOcclusionSystem.h"
#include "Scene3D/Systems/SwitchSystem.h"
#include "Scene3D/Systems/TransformSystem.h"
#include "Scene3D/Systems/UpdateSystem.h"
#include "Scene3D/Systems/WaveSystem.h"
#include "Scene3D/Systems/WindSystem.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleLayer.h"
#include "Particles/ParticleForce.h"
#include "Render/3D/PolygonGroup.h"
#include "Render/Highlevel/LandscapeSubdivision.h"
#include "Render/Highlevel/RenderObject.h"
#include "Render/Highlevel/RenderBatch.h"
#include "Render/Highlevel/Vegetation/VegetationRenderObject.h"
#include "Render/Highlevel/BillboardRenderObject.h"
#include "Render/Highlevel/Heightmap.h"
#include "Render/Highlevel/Landscape.h"
#include "Render/Highlevel/Light.h"
#include "Render/Highlevel/SpeedTreeObject.h"
#include "Render/Material/NMaterial.h"
#include "Math/Vector.h"
#include "Math/Rect.h"
#include "Math/AABBox3.h"
#include "Math/Color.h"
#include "UI/Script/UIScriptComponent.h"
#include "UI/Script/UIScriptComponentController.h"
#include "UI/Script/Private/UILuaScriptComponentController.h"
#include "UI/UI3DView.h"
#include "UI/UIButton.h"
#include "UI/UIControl.h"
#include "UI/UIControl.h"
#include "UI/UIJoypad.h"
#include "UI/UIList.h"
#include "UI/UIListCell.h"
#include "UI/UIMovieView.h"
#include "UI/UIParticles.h"
#include "UI/UIScrollBar.h"
#include "UI/UIScrollView.h"
#include "UI/UIScrollViewContainer.h"
#include "UI/UISlider.h"
#include "UI/UISpinner.h"
#include "UI/UIStaticText.h"
#include "UI/UISwitch.h"
#include "UI/UITextField.h"
#include "UI/UIWebView.h"
#include "UI/Components/UIComponent.h"
#include "UI/Events/UIEventBindingComponent.h"
#include "UI/Events/UIMovieEventComponent.h"
#include "UI/Events/UIInputEventComponent.h"
#include "UI/Events/UIShortcutEventComponent.h"
#include "UI/Focus/UIFocusComponent.h"
#include "UI/Focus/UIFocusGroupComponent.h"
#include "UI/Focus/UINavigationComponent.h"
#include "UI/Focus/UITabOrderComponent.h"
#include "UI/Input/UIModalInputComponent.h"
#include "UI/Joypad/UIJoypadComponent.h"
#include "UI/Layouts/UIAnchorComponent.h"
#include "UI/Layouts/UIAnchorSafeAreaComponent.h"
#include "UI/Layouts/UIFlowLayoutComponent.h"
#include "UI/Layouts/UIFlowLayoutHintComponent.h"
#include "UI/Layouts/UIIgnoreLayoutComponent.h"
#include "UI/Layouts/UILinearLayoutComponent.h"
#include "UI/Layouts/UISizePolicyComponent.h"
#include "UI/Layouts/UILayoutSourceRectComponent.h"
#include "UI/Layouts/UILayoutIsolationComponent.h"
#include "UI/Render/UIDebugRenderComponent.h"
#include "UI/Render/UIClipContentComponent.h"
#include "UI/Scene3D/UISceneComponent.h"
#include "UI/Scene3D/UIEntityMarkerComponent.h"
#include "UI/Scene3D/UIEntityMarkersContainerComponent.h"
#include "UI/Scroll/UIScrollBarDelegateComponent.h"
#include "UI/Sound/UISoundComponent.h"
#include "UI/Sound/UISoundValueFilterComponent.h"
#include "UI/Update/UIUpdateComponent.h"
#include "UI/Update/UICustomUpdateDeltaComponent.h"
#include "UI/RichContent/UIRichContentComponent.h"
#include "UI/RichContent/UIRichContentObjectComponent.h"
#include "UI/RichContent/UIRichContentAliasesComponent.h"
#include "UI/Scroll/UIScrollComponent.h"
#include "UI/Text/UITextComponent.h"
#include "UI/Flow/UIFlowContext.h"
#include "UI/Flow/UIFlowController.h"
#include "UI/Flow/UIFlowControllerComponent.h"
#include "UI/Flow/UIFlowService.h"
#include "UI/Flow/UIFlowStateComponent.h"
#include "UI/Flow/UIFlowTransitionComponent.h"
#include "UI/Flow/UIFlowViewComponent.h"
#include "UI/Flow/Private/UIFlowLuaController.h"
#include "UI/Flow/Services/UIFlowDataService.h"
#include "UI/Flow/Services/UIFlowEventsService.h"
#include "UI/Flow/Services/UIFlowEngineService.h"
#include "UI/Flow/Services/UIFlowSystemService.h"

namespace DAVA
{
namespace ReflectionDeclarationDetail
{
float32 GetMinX(AABBox3* box)
{
    return box->min.x;
}

void SetMinX(AABBox3* box, float32 v)
{
    box->min.x = v;
}

float32 GetMinY(AABBox3* box)
{
    return box->min.y;
}

void SetMinY(AABBox3* box, float32 v)
{
    box->min.y = v;
}

float32 GetMinZ(AABBox3* box)
{
    return box->min.z;
}

void SetMinZ(AABBox3* box, float32 v)
{
    box->min.z = v;
}

float32 GetMaxX(AABBox3* box)
{
    return box->max.x;
}

void SetMaxX(AABBox3* box, float32 v)
{
    box->max.x = v;
}

float32 GetMaxY(AABBox3* box)
{
    return box->max.y;
}

void SetMaxY(AABBox3* box, float32 v)
{
    box->max.y = v;
}

float32 GetMaxZ(AABBox3* box)
{
    return box->max.z;
}

void SetMaxZ(AABBox3* box, float32 v)
{
    box->max.z = v;
}
}

void RegisterVector2()
{
    ReflectionRegistrator<Vector2>::Begin()
    .Field("X", &Vector2::x)[M::SubProperty()]
    .Field("Y", &Vector2::y)[M::SubProperty()]
    .End();
}

void RegisterVector3()
{
    ReflectionRegistrator<Vector3>::Begin()
    .Field("X", &Vector3::x)[M::SubProperty()]
    .Field("Y", &Vector3::y)[M::SubProperty()]
    .Field("Z", &Vector3::z)[M::SubProperty()]
    .End();
}

void RegisterVector4()
{
    ReflectionRegistrator<Vector4>::Begin()
    .Field("X", &Vector4::x)[M::SubProperty()]
    .Field("Y", &Vector4::y)[M::SubProperty()]
    .Field("Z", &Vector4::z)[M::SubProperty()]
    .Field("W", &Vector4::w)[M::SubProperty()]
    .End();
}

void RegisterRect()
{
    ReflectionRegistrator<Rect>::Begin()
    .Field("X", &Rect::x)[M::SubProperty()]
    .Field("Y", &Rect::y)[M::SubProperty()]
    .Field("Width", &Rect::dx)[M::SubProperty()]
    .Field("Height", &Rect::dy)[M::SubProperty()]
    .End();
}

void RegisterAABBox3()
{
    ReflectionRegistrator<AABBox3>::Begin()
    .Field("MinX", &ReflectionDeclarationDetail::GetMinX, &ReflectionDeclarationDetail::SetMinX)[M::SubProperty()]
    .Field("MinY", &ReflectionDeclarationDetail::GetMinY, &ReflectionDeclarationDetail::SetMinY)[M::SubProperty()]
    .Field("MinZ", &ReflectionDeclarationDetail::GetMinZ, &ReflectionDeclarationDetail::SetMinZ)[M::SubProperty()]
    .Field("MaxX", &ReflectionDeclarationDetail::GetMaxX, &ReflectionDeclarationDetail::SetMaxX)[M::SubProperty()]
    .Field("MaxY", &ReflectionDeclarationDetail::GetMaxY, &ReflectionDeclarationDetail::SetMaxY)[M::SubProperty()]
    .Field("MaxZ", &ReflectionDeclarationDetail::GetMaxZ, &ReflectionDeclarationDetail::SetMaxZ)[M::SubProperty()]
    .End();
}

void RegisterColor()
{
    ReflectionRegistrator<Color>::Begin()
    .Field("R", &Color::r)
    .Field("G", &Color::g)
    .Field("B", &Color::b)
    .Field("A", &Color::a)
    .End();
}

void RegisterPermanentNames()
{
    /* PLEASE KEEP LOGICAL & ALPHABETICAL ORDER */

    // Engine classes
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(Engine);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(EngineContext);

    // Common classes
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(BaseObject);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(KeyedArchive);

    // Components
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(Component);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SingletonComponent);

    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ActionComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ActionComponent::Action);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(AnimationComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(BulletComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(CameraComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(CustomPropertiesComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(DebugRenderComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(EdgeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(GeoDecalComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LightComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LightRenderComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LodComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(MotionComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ParticleEffectComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PathComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(QualitySettingsComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RenderComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RotationControllerComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SkeletonComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SlotComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SnapToLandscapeControllerComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SoundComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SoundComponentElement);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SpeedTreeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticOcclusionComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticOcclusionDataComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticOcclusionDebugDrawComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SwitchComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(TextComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(TransformComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(TransformInterpolationComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UpdatableComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UserComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VisibilityCheckComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WASDControllerComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WaveComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WaypointComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WindComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StreamingSettingsComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RuntimeEntityMarkComponent);

    DAVA_REFLECTION_REGISTER_CUSTOM_PERMANENT_NAME(PathComponent::Waypoint, "Waypoint");
    DAVA_REFLECTION_REGISTER_CUSTOM_PERMANENT_NAME(PathComponent::Edge, "Edge");
    DAVA_REFLECTION_REGISTER_CUSTOM_PERMANENT_NAME(SkeletonComponent::Joint, "Joint");

    // Single components
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ActionsSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ChangedSystemsSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(MotionSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(TransformSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VTSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LightmapSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RenderObjectSingleComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PrefabSingleComponent);

    // Systems
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(BaseSimulationSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SceneSystem);

    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ActionCollectSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ActionUpdateSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(AnimationSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(DebugRenderSystem);
#ifdef DIFF_MONITORING_ENABLED
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(DiffMonitoringSystem);
#endif
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(FoliageSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(GeoDecalSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LandscapeSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LightUpdateSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LodSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(MotionSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ParticleEffectDebugDrawSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ParticleEffectSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PostEffectComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PostEffectDebugComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PrefabComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(MeshComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LandscapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LightmapDataComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LightmapComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RenderUpdateSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ReflectionComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RotationControllerSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SnapToLandscapeControllerSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SkeletonSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SlotSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SoundUpdateSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SpeedTreeUpdateSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticOcclusionBuildSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticOcclusionDebugDrawSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticOcclusionSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SwitchSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(TransformSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UpdateSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WASDControllerSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WaveSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WindSystem);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PrefabLoadingSystem);

    // 3D classes
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PolygonGroup);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RenderObject);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RenderBatchWithOptions);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LandscapeSubdivision);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(LandscapeSubdivision::SubdivisionMetrics);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(RenderBatch);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VegetationRenderObject);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(BillboardRenderObject);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(Heightmap);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(Landscape);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(Light);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SpeedTreeObject);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(Entity);

    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ParticleEmitterInstance);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ParticleLayer);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ParticleForce);

    DAVA_REFLECTION_REGISTER_CUSTOM_PERMANENT_NAME(PartilceEmitterLoadProxy, "ParticleEmitter3D");
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(DecalComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(DecalRenderObject);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VTDecalComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SplineComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(EnvironmentComponent);

    // UI controls
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UI3DView);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIButton);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIControl);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIJoypad);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIList);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIListCell);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIMovieView);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIParticles);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIScrollBar);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIScrollView);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIScrollViewContainer);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UISlider);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UISpinner);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIStaticText);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UISwitch);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UITextField);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIWebView);

    // UI components
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIComponent);

// clang-format off
#define DECL_UI_COMPONENT(type, string) \
    DAVA_REFLECTION_REGISTER_CUSTOM_PERMANENT_NAME(type, string); \
    GetEngineContext()->componentManager->RegisterComponent<type>()
    // clang-format on

    DECL_UI_COMPONENT(UILinearLayoutComponent, "LinearLayout");
    DECL_UI_COMPONENT(UIFlowLayoutComponent, "FlowLayout");
    DECL_UI_COMPONENT(UIFlowLayoutHintComponent, "FlowLayoutHint");
    DECL_UI_COMPONENT(UIIgnoreLayoutComponent, "IgnoreLayout");
    DECL_UI_COMPONENT(UISizePolicyComponent, "SizePolicy");
    DECL_UI_COMPONENT(UIAnchorComponent, "Anchor");
    DECL_UI_COMPONENT(UIAnchorSafeAreaComponent, "UIAnchorSafeAreaComponent");
    DECL_UI_COMPONENT(UILayoutSourceRectComponent, "UILayoutSourceRectComponent");
    DECL_UI_COMPONENT(UILayoutIsolationComponent, "UILayoutIsolationComponent");
    DECL_UI_COMPONENT(UIControlBackground, "Background");
    DECL_UI_COMPONENT(UIModalInputComponent, "ModalInput");
    DECL_UI_COMPONENT(UIFocusComponent, "Focus");
    DECL_UI_COMPONENT(UIFocusGroupComponent, "FocusGroup");
    DECL_UI_COMPONENT(UINavigationComponent, "Navigation");
    DECL_UI_COMPONENT(UITabOrderComponent, "TabOrder");
    DECL_UI_COMPONENT(UIEventBindingComponent, "UIEventBindingComponent");
    DECL_UI_COMPONENT(UIInputEventComponent, "UIInputEventComponent");
    DECL_UI_COMPONENT(UIMovieEventComponent, "UIMovieEventComponent");
    DECL_UI_COMPONENT(UIShortcutEventComponent, "UIShortcutEventComponent");
    DECL_UI_COMPONENT(UIScrollBarDelegateComponent, "ScrollBarDelegate");
    DECL_UI_COMPONENT(UIScrollComponent, "ScrollComponent");
    DECL_UI_COMPONENT(UISoundComponent, "Sound");
    DECL_UI_COMPONENT(UISoundValueFilterComponent, "SoundValueFilter");
    DECL_UI_COMPONENT(UIUpdateComponent, "Update");
    DECL_UI_COMPONENT(UICustomUpdateDeltaComponent, "CustomDeltaUpdate");
    DECL_UI_COMPONENT(UIRichContentComponent, "RichContent");
    DECL_UI_COMPONENT(UIRichContentAliasesComponent, "RichContentAliases");
    DECL_UI_COMPONENT(UIRichContentObjectComponent, "RichContentObject");
    DECL_UI_COMPONENT(UIDebugRenderComponent, "DebugRender");
    DECL_UI_COMPONENT(UIClipContentComponent, "ClipContent");
    DECL_UI_COMPONENT(UISceneComponent, "SceneComponent");
    DECL_UI_COMPONENT(UIEntityMarkerComponent, "UIEntityMarkerComponent");
    DECL_UI_COMPONENT(UIEntityMarkersContainerComponent, "UIEntityMarkersContainerComponent");
    DECL_UI_COMPONENT(UITextComponent, "UITextComponent");
    DECL_UI_COMPONENT(UIScriptComponent, "UIScriptComponent");

    DECL_UI_COMPONENT(UIFlowControllerComponent, "UIFlowControllerComponent");
    DECL_UI_COMPONENT(UIFlowStateComponent, "UIFlowStateComponent");
    DECL_UI_COMPONENT(UIFlowTransitionComponent, "UIFlowTransitionComponent");
    DECL_UI_COMPONENT(UIFlowViewComponent, "UIFlowViewComponent");
    DECL_UI_COMPONENT(UIJoypadComponent, "UIJoypadComponent");
        
#undef DECL_UI_COMPONENT

    // Script types
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIScriptComponentController);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UILuaScriptComponentController);
    // Flow base types
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowContext);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowController);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowLuaController);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowService);
    // Flow services
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowDataService);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowEngineService);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowEventsService);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(UIFlowSystemService);
}

void RegisterReflectionForBaseTypes()
{
    RegisterAnyCasts();
    RegisterAnyHash();

    RegisterVector2();
    RegisterVector3();
    RegisterVector4();
    RegisterRect();
    RegisterAABBox3();
    RegisterColor();

    RegisterPermanentNames();
}
} // namespace DAVA
