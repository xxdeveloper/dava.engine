/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#include "Scene3D/Scene.h"

#include "Render/Texture.h"
#include "Render/Material.h"
#include "Render/3D/StaticMesh.h"
#include "Render/3D/AnimatedMesh.h"
#include "Render/Image/Image.h"
#include "Render/Highlevel/RenderSystem.h"


#include "Platform/SystemTimer.h"
#include "FileSystem/FileSystem.h"
#include "Debug/Stats.h"

#include "Scene3D/SceneFile.h"
#include "Scene3D/SceneFileV2.h"
#include "Scene3D/DataNode.h"
#include "Scene3D/ProxyNode.h"
#include "Scene3D/ShadowVolumeNode.h"
#include "Render/Highlevel/Light.h"
#include "Scene3D/MeshInstanceNode.h"
#include "Render/Highlevel/Landscape.h"
#include "Render/Highlevel/RenderSystem.h"

#include "Entity/SceneSystem.h"
#include "Scene3D/Systems/TransformSystem.h"
#include "Scene3D/Systems/RenderUpdateSystem.h"
#include "Scene3D/Systems/LodSystem.h"
#include "Scene3D/Systems/DebugRenderSystem.h"
#include "Scene3D/Systems/EventSystem.h"
#include "Scene3D/Systems/ParticleEffectSystem.h"
#include "Scene3D/Systems/UpdateSystem.h"
#include "Scene3D/Systems/LightUpdateSystem.h"
#include "Scene3D/Systems/SwitchSystem.h"
#include "Scene3D/Systems/SoundUpdateSystem.h"
#include "Scene3D/Systems/ActionUpdateSystem.h"
#include "Scene3D/Systems/SkyboxSystem.h"
#include "Scene3D/Systems/WindSystem.h"
#include "Scene3D/Systems/WaveSystem.h"
#include "Scene3D/Systems/SkeletonSystem.h"
#include "Scene3D/Systems/AnimationSystem.h"

#include "Sound/SoundSystem.h"

#include "Scene3D/Systems/SpeedTreeUpdateSystem.h"

#include "Scene3D/Systems/StaticOcclusionSystem.h"
#include "Scene3D/Systems/FoliageSystem.h"

#include "Scene3D/Systems/MaterialSystem.h"

#include "Scene3D/Components/ComponentHelpers.h"
#include "Scene3D/SceneCache.h"
#include "UI/UIEvent.h"

#include "Render/Renderer.h"


namespace DAVA 
{

Texture* Scene::stubTexture2d = NULL;
Texture* Scene::stubTextureCube = NULL;
Texture* Scene::stubTexture2dLightmap = NULL; //this texture should be all-pink without checkers
    
    
Scene::Scene(uint32 _systemsMask /* = SCENE_SYSTEM_ALL_MASK */)
	: Entity()
    , transformSystem(0)
    , renderUpdateSystem(0)
    , lodSystem(0)
    , debugRenderSystem(0)
    , particleEffectSystem(0)
    , updatableSystem(0)
    , lightUpdateSystem(0)
    , switchSystem(0)
    , soundSystem(0)
    , actionSystem(0)
    , skyboxSystem(0)
    , staticOcclusionSystem(0)
	, materialSystem(0)
    , foliageSystem(0)
    , windSystem(0)
    , animationSystem(0)
    , staticOcclusionDebugDrawSystem(0)
    , systemsMask(_systemsMask)
    , clearBuffers(0)
    , isDefaultGlobalMaterial(true)
    , sceneGlobalMaterial(0)
    , mainCamera(0)
    , drawCamera(0)
{
	CreateComponents();
	CreateSystems();

    // this will force scene to create hidden global material
    SetGlobalMaterial(NULL);
    
    SceneCache::Instance()->InsertScene(this);
}

void Scene::CreateComponents()
{ }

NMaterial* Scene::GetGlobalMaterial() const
{
    NMaterial *ret = NULL;

    // default global material is for internal use only
    // so all external object should assume, that scene hasn't any global material
    if(!isDefaultGlobalMaterial)
    {
        ret = sceneGlobalMaterial;
    }

    return ret;
}

void Scene::SetGlobalMaterial(NMaterial *globalMaterial)
{
#if RHI_COMPLETE
    SafeRelease(sceneGlobalMaterial);

    if(NULL != globalMaterial)
    {
        DVASSERT(globalMaterial->GetMaterialType() == NMaterial::MATERIALTYPE_GLOBAL);

        isDefaultGlobalMaterial = false;
        sceneGlobalMaterial = SafeRetain(globalMaterial);
    }
    else
    {
        isDefaultGlobalMaterial = true;
        sceneGlobalMaterial = NMaterial::CreateGlobalMaterial(FastName("Scene_Global_Material"));
    }

    InitGlobalMaterial();

    renderSystem->SetGlobalMaterial(sceneGlobalMaterial);
    particleEffectSystem->SetGlobalMaterial(sceneGlobalMaterial);
    
    ImportShadowColor(this);
#endif //RHI_COMPLETE
}

void Scene::InitGlobalMaterial()
{
#if RHI_COMPLETE
    if(NULL == stubTexture2d)
    {
        stubTexture2d = Texture::CreatePink(rhi::TEXTURE_TYPE_2D);
    }

    if(NULL == stubTextureCube)
    {
        stubTextureCube = Texture::CreatePink(rhi::TEXTURE_TYPE_CUBE);
    }

    if(NULL == stubTexture2dLightmap)
    {
        stubTexture2dLightmap = Texture::CreatePink(rhi::TEXTURE_TYPE_2D, false);
    }

    Vector3 defaultVec3;
    Color defaultColor(1.0f, 0.0f, 0.0f, 1.0f);
    //float32 defaultFloat0 = 0.0f;
    float32 defaultFloat05 = 0.5f;
    float32 defaultFloat10 = 1.0f;
    Vector2 defaultVec2;
    Vector2 defaultVec2I(1.f, 1.f);
    float32 defaultLightmapSize = 16.0f;
    float32 defaultFogStart = 0.0f;
    float32 defaultFogEnd = 500.0f;
    float32 defaultFogHeight = 50.0f;
    float32 defaultFogDensity = 0.005f;


    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_ALBEDO).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_ALBEDO, stubTexture2d);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_NORMAL).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_NORMAL, stubTexture2d);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_DETAIL).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_DETAIL, stubTexture2d);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_LIGHTMAP).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_LIGHTMAP, stubTexture2dLightmap);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_DECAL).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_DECAL, stubTexture2d);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_CUBEMAP).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_CUBEMAP, stubTextureCube);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_DECALMASK).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_DECALMASK, stubTexture2d);
    if(sceneGlobalMaterial->GetTexturePath(NMaterialTextureName::TEXTURE_DECALTEXTURE).IsEmpty()) sceneGlobalMaterial->SetTexture(NMaterialTextureName::TEXTURE_DECALTEXTURE, stubTexture2d);

    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_LIGHT_POSITION0)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_LIGHT_POSITION0, Shader::UT_FLOAT_VEC3, 1, defaultVec3.data);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_PROP_AMBIENT_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_PROP_AMBIENT_COLOR, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_PROP_DIFFUSE_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_PROP_DIFFUSE_COLOR, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_PROP_SPECULAR_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_PROP_SPECULAR_COLOR, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_LIGHT_AMBIENT_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_LIGHT_AMBIENT_COLOR, Shader::UT_FLOAT_VEC3, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_LIGHT_DIFFUSE_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_LIGHT_DIFFUSE_COLOR, Shader::UT_FLOAT_VEC3, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_LIGHT_SPECULAR_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_LIGHT_SPECULAR_COLOR, Shader::UT_FLOAT_VEC3, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_LIGHT_INTENSITY0)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_LIGHT_INTENSITY0, Shader::UT_FLOAT, 1, &defaultFloat05);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_MATERIAL_SPECULAR_SHININESS)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_MATERIAL_SPECULAR_SHININESS, Shader::UT_FLOAT, 1, &defaultFloat05);
    
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_LIMIT)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_LIMIT, Shader::UT_FLOAT, 1, &defaultFloat10);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_COLOR, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_DENSITY)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_DENSITY, Shader::UT_FLOAT, 1, &defaultFogDensity);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_START)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_START, Shader::UT_FLOAT, 1, &defaultFogStart);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_END)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_END, Shader::UT_FLOAT, 1, &defaultFogEnd);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_DENSITY)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_DENSITY, Shader::UT_FLOAT, 1, &defaultFogDensity);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_FALLOFF)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_FALLOFF, Shader::UT_FLOAT, 1, &defaultFogDensity);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_HEIGHT)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_HEIGHT, Shader::UT_FLOAT, 1, &defaultFogHeight);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_LIMIT)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_HALFSPACE_LIMIT, Shader::UT_FLOAT, 1, &defaultFloat10);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_COLOR_SUN)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_COLOR_SUN, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_COLOR_SKY)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_COLOR_SKY, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_SCATTERING)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_SCATTERING, Shader::UT_FLOAT, 1, &defaultFloat10);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_DISTANCE)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FOG_ATMOSPHERE_DISTANCE, Shader::UT_FLOAT, 1, &defaultFogEnd);

    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_FLAT_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_FLAT_COLOR, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_TEXTURE0_SHIFT)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_TEXTURE0_SHIFT, Shader::UT_FLOAT_VEC2, 1, defaultVec2.data);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_UV_OFFSET)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_UV_OFFSET, Shader::UT_FLOAT_VEC2, 1, defaultVec2.data);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_UV_SCALE)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_UV_SCALE, Shader::UT_FLOAT_VEC2, 1, defaultVec2.data);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_LIGHTMAP_SIZE)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_LIGHTMAP_SIZE, Shader::UT_FLOAT, 1, &defaultLightmapSize);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_DECAL_TILE_SCALE)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_DECAL_TILE_SCALE, Shader::UT_FLOAT_VEC2, 1, &defaultVec2);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_DECAL_TILE_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_DECAL_TILE_COLOR, Shader::UT_FLOAT_VEC4, 1, &Color::White);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_DETAIL_TILE_SCALE)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_DETAIL_TILE_SCALE, Shader::UT_FLOAT_VEC2, 1, &defaultVec2);
    if(NULL == sceneGlobalMaterial->GetPropertyValue(NMaterialParamName::PARAM_SHADOW_COLOR)) sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_SHADOW_COLOR, Shader::UT_FLOAT_VEC4, 1, &defaultColor);
#endif //RHI_COMPLETE
}

void Scene::CreateSystems()
{
	renderSystem = new RenderSystem();
    eventSystem = new EventSystem();

    if(SCENE_SYSTEM_STATIC_OCCLUSION_FLAG & systemsMask)
    {
        staticOcclusionSystem = new StaticOcclusionSystem(this);
        AddSystem(staticOcclusionSystem, MAKE_COMPONENT_MASK(Component::STATIC_OCCLUSION_DATA_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_ANIMATION_FLAG & systemsMask)
    {
        animationSystem = new AnimationSystem(this);
        AddSystem(animationSystem, MAKE_COMPONENT_MASK(Component::ANIMATION_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_TRANSFORM_FLAG & systemsMask)
    {
        transformSystem = new TransformSystem(this);
        AddSystem(transformSystem, MAKE_COMPONENT_MASK(Component::TRANSFORM_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_LOD_FLAG & systemsMask)
    {
        lodSystem = new LodSystem(this);
        AddSystem(lodSystem, MAKE_COMPONENT_MASK(Component::LOD_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_SWITCH_FLAG & systemsMask)
    {
        switchSystem = new SwitchSystem(this);
        AddSystem(switchSystem, MAKE_COMPONENT_MASK(Component::SWITCH_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_PARTICLE_EFFECT_FLAG & systemsMask)
    {
        particleEffectSystem = new ParticleEffectSystem(this);
        AddSystem(particleEffectSystem, MAKE_COMPONENT_MASK(Component::PARTICLE_EFFECT_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_SOUND_UPDATE_FLAG & systemsMask)
    {
        soundSystem = new SoundUpdateSystem(this);
        AddSystem(soundSystem, MAKE_COMPONENT_MASK(Component::TRANSFORM_COMPONENT) | MAKE_COMPONENT_MASK(Component::SOUND_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_RENDER_UPDATE_FLAG & systemsMask)
    {
        renderUpdateSystem = new RenderUpdateSystem(this);
        AddSystem(renderUpdateSystem, MAKE_COMPONENT_MASK(Component::TRANSFORM_COMPONENT) | MAKE_COMPONENT_MASK(Component::RENDER_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_UPDATEBLE_FLAG & systemsMask)
    {
        updatableSystem = new UpdateSystem(this);
        AddSystem(updatableSystem, MAKE_COMPONENT_MASK(Component::UPDATABLE_COMPONENT));
    }

    if(SCENE_SYSTEM_LIGHT_UPDATE_FLAG & systemsMask)
    {
        lightUpdateSystem = new LightUpdateSystem(this);
        AddSystem(lightUpdateSystem, MAKE_COMPONENT_MASK(Component::TRANSFORM_COMPONENT) | MAKE_COMPONENT_MASK(Component::LIGHT_COMPONENT));
    }

    if(SCENE_SYSTEM_ACTION_UPDATE_FLAG & systemsMask)
    {
        actionSystem = new ActionUpdateSystem(this);
        AddSystem(actionSystem, MAKE_COMPONENT_MASK(Component::ACTION_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_SKYBOX_FLAG & systemsMask)
    {
        skyboxSystem = new SkyboxSystem(this);
        AddSystem(skyboxSystem, MAKE_COMPONENT_MASK(Component::RENDER_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_MATERIAL_FLAG & systemsMask)
    {
        materialSystem = new MaterialSystem(this);
        AddSystem(materialSystem, MAKE_COMPONENT_MASK(Component::RENDER_COMPONENT));
    }

    if(SCENE_SYSTEM_DEBUG_RENDER_FLAG & systemsMask)
    {
        debugRenderSystem = new DebugRenderSystem(this);
        AddSystem(debugRenderSystem, MAKE_COMPONENT_MASK(Component::DEBUG_RENDER_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }
    
    if(SCENE_SYSTEM_FOLIAGE_FLAG & systemsMask)
    {
        foliageSystem = new FoliageSystem(this);
        AddSystem(foliageSystem, MAKE_COMPONENT_MASK(Component::RENDER_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_SPEEDTREE_UPDATE_FLAG & systemsMask)
    {
        speedTreeUpdateSystem = new SpeedTreeUpdateSystem(this);
        AddSystem(speedTreeUpdateSystem, MAKE_COMPONENT_MASK(Component::SPEEDTREE_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_WIND_UPDATE_FLAG & systemsMask)
    {
        windSystem = new WindSystem(this);
        AddSystem(windSystem, MAKE_COMPONENT_MASK(Component::WIND_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_WAVE_UPDATE_FLAG & systemsMask)
    {
        waveSystem = new WaveSystem(this);
        AddSystem(waveSystem, MAKE_COMPONENT_MASK(Component::WAVE_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }

    if(SCENE_SYSTEM_SKELETON_UPDATE_FLAG & systemsMask)
    {
        skeletonSystem = new SkeletonSystem(this);
        AddSystem(skeletonSystem, MAKE_COMPONENT_MASK(Component::SKELETON_COMPONENT), SCENE_SYSTEM_REQUIRE_PROCESS);
    }
}

Scene::~Scene()
{
    SceneCache::Instance()->RemoveScene(this);
    
	for (Vector<AnimatedMesh*>::iterator t = animatedMeshes.begin(); t != animatedMeshes.end(); ++t)
	{
		AnimatedMesh * obj = *t;
		obj->Release();
	}
	animatedMeshes.clear();
	
	for (Vector<Camera*>::iterator t = cameras.begin(); t != cameras.end(); ++t)
	{
		Camera * obj = *t;
		obj->Release();
	}
	cameras.clear();
    
    SafeRelease(mainCamera);
    SafeRelease(drawCamera);
    
    for (ProxyNodeMap::iterator it = rootNodes.begin(); it != rootNodes.end(); ++it)
    {
        SafeRelease(it->second);
    }
    rootNodes.clear();

    // Children should be removed first because they should unregister themselves in managers
	RemoveAllChildren();	

    SafeRelease(sceneGlobalMaterial);

    transformSystem = 0;
    renderUpdateSystem = 0;
    lodSystem = 0;
    debugRenderSystem = 0;
    particleEffectSystem = 0;
    updatableSystem = 0;
    lightUpdateSystem = 0;
    switchSystem = 0;
    soundSystem = 0;
    actionSystem = 0;
    skyboxSystem = 0;
    staticOcclusionSystem = 0;
    materialSystem = 0;
    speedTreeUpdateSystem = 0;
    foliageSystem = 0;
    windSystem = 0;
    waveSystem = 0;
    animationSystem = 0;
    
    uint32 size = (uint32)systems.size();
    for (uint32 k = 0; k < size; ++k)
        SafeDelete(systems[k]);
    systems.clear();

    systemsToProcess.clear();
    systemsToInput.clear();

	SafeDelete(eventSystem);
	SafeDelete(renderSystem);
}
    
void Scene::RegisterEntity(Entity * entity)
{
    for(auto& system : systems)
    {
        system->RegisterEntity(entity);
    }
}

void Scene::UnregisterEntity(Entity * entity)
{
    for(auto& system : systems)
    {
        system->UnregisterEntity(entity);
    }
}

void Scene::RegisterEntitiesInSystemRecursively(SceneSystem *system, Entity * entity)
{
    system->RegisterEntity(entity);
    for (int32 i=0, sz = entity->GetChildrenCount(); i<sz; ++i)
        RegisterEntitiesInSystemRecursively(system, entity->GetChild(i));
}
void Scene::UnregisterEntitiesInSystemRecursively(SceneSystem *system, Entity * entity)
{
    system->UnregisterEntity(entity);
    for (int32 i=0, sz = entity->GetChildrenCount(); i<sz; ++i)
        UnregisterEntitiesInSystemRecursively(system, entity->GetChild(i));
}

void Scene::RegisterComponent(Entity * entity, Component * component)
{
    DVASSERT(entity && component);
    uint32 systemsCount = static_cast<uint32>(systems.size());
    for (uint32 k = 0; k < systemsCount; ++k)
    {
        systems[k]->RegisterComponent(entity, component);
    }
}

void Scene::UnregisterComponent(Entity * entity, Component * component)
{
    DVASSERT(entity && component);
    uint32 systemsCount = static_cast<uint32>(systems.size());
    for (uint32 k = 0; k < systemsCount; ++k)
    {
        systems[k]->UnregisterComponent(entity, component);
    }
    
}


#if 0 // Removed temporarly if everything will work with events can be removed fully.
void Scene::ImmediateEvent(Entity * entity, uint32 componentType, uint32 event)
{
#if 1
    uint32 systemsCount = systems.size();
    uint64 updatedComponentFlag = MAKE_COMPONENT_MASK(componentType);
    uint64 componentsInEntity = entity->GetAvailableComponentFlags();

    for (uint32 k = 0; k < systemsCount; ++k)
    {
        uint64 requiredComponentFlags = systems[k]->GetRequiredComponents();
        
        if (((requiredComponentFlags & updatedComponentFlag) != 0) && ((requiredComponentFlags & componentsInEntity) == requiredComponentFlags))
        {
			eventSystem->NotifySystem(systems[k], entity, event);
        }
    }
#else
    uint64 componentsInEntity = entity->GetAvailableComponentFlags();
    Set<SceneSystem*> & systemSetForType = componentTypeMapping.GetValue(componentsInEntity);
    
    for (Set<SceneSystem*>::iterator it = systemSetForType.begin(); it != systemSetForType.end(); ++it)
    {
        SceneSystem * system = *it;
        uint64 requiredComponentFlags = system->GetRequiredComponents();
        if ((requiredComponentFlags & componentsInEntity) == requiredComponentFlags)
            eventSystem->NotifySystem(system, entity, event);
    }
#endif
}
#endif
    
void Scene::AddSystem(SceneSystem * sceneSystem, uint64 componentFlags, uint32 processFlags /*= 0*/, SceneSystem * insertBeforeSceneForProcess /* = NULL */)
{
    sceneSystem->SetRequiredComponents(componentFlags);
    //Set<SceneSystem*> & systemSetForType = componentTypeMapping.GetValue(componentFlags);
    //systemSetForType.insert(sceneSystem);
    systems.push_back(sceneSystem);

    if(processFlags & SCENE_SYSTEM_REQUIRE_PROCESS)
    {
        bool wasInsertedForUpdate = false;
        if(insertBeforeSceneForProcess)
        {
            Vector<SceneSystem*>::iterator itEnd = systemsToProcess.end();
            for (Vector<SceneSystem*>::iterator it = systemsToProcess.begin(); it != itEnd; ++it)
            {
                if(insertBeforeSceneForProcess == (*it))
                {
                    systemsToProcess.insert(it, sceneSystem);
                    wasInsertedForUpdate = true;
                    break;
                }
            }
        }
        else
        {
            systemsToProcess.push_back(sceneSystem);
            wasInsertedForUpdate = true;
        }
        DVASSERT(wasInsertedForUpdate);
    }
    
    if(processFlags & SCENE_SYSTEM_REQUIRE_INPUT)
    {
        systemsToInput.push_back(sceneSystem);
    }
    
    RegisterEntitiesInSystemRecursively(sceneSystem, this);
}
    
void Scene::RemoveSystem(SceneSystem * sceneSystem)
{
    UnregisterEntitiesInSystemRecursively(sceneSystem, this);
    
    RemoveSystem(systemsToProcess, sceneSystem);
    RemoveSystem(systemsToInput, sceneSystem);

    DVVERIFY(RemoveSystem(systems, sceneSystem));
}

    
bool Scene::RemoveSystem(Vector<SceneSystem*> &storage, SceneSystem *system)
{
    Vector<SceneSystem*>::iterator endIt = storage.end();
    for(Vector<SceneSystem*>::iterator it = storage.begin(); it != endIt; ++it)
    {
        if(*it == system)
        {
            storage.erase(it);
            return true;
        }
    }
    
    return false;
}
    
    
    
Scene * Scene::GetScene()
{
    return this;
}
    
void Scene::AddAnimatedMesh(AnimatedMesh * mesh)
{
	if (mesh)
	{
		mesh->Retain();
		animatedMeshes.push_back(mesh);
	}	
}

void Scene::RemoveAnimatedMesh(AnimatedMesh * mesh)
{
	
}

AnimatedMesh * Scene::GetAnimatedMesh(int32 index)
{
	return animatedMeshes[index];
}
	
	
	
void Scene::AddCamera(Camera * camera)
{
	if (camera)
	{
		camera->Retain();
		cameras.push_back(camera);
	}
}

Camera * Scene::GetCamera(int32 n)
{
	if (n >= 0 && n < (int32)cameras.size())
		return cameras[n];
	
	return NULL;
}


void Scene::AddRootNode(Entity *node, const FilePath &rootNodePath)
{
    ProxyNode * proxyNode = new ProxyNode();
    proxyNode->SetNode(node);
    
	rootNodes[FILEPATH_MAP_KEY(rootNodePath)] = proxyNode;

	//proxyNode->SetName(rootNodePath.GetAbsolutePathname());
}

Entity *Scene::GetRootNode(const FilePath &rootNodePath)
{
	ProxyNodeMap::const_iterator it = rootNodes.find(FILEPATH_MAP_KEY(rootNodePath));
	if (it != rootNodes.end())
	{
        ProxyNode * node = it->second;
		return node->GetNode();
	}
    
    if(rootNodePath.IsEqualToExtension(".sce"))
    {
        SceneFile *file = new SceneFile();
        file->SetDebugLog(true);
        file->LoadScene(rootNodePath, this);
        SafeRelease(file);
    }
    else if(rootNodePath.IsEqualToExtension(".sc2"))
    {
        uint64 startTime = SystemTimer::Instance()->AbsoluteMS();
        SceneFileV2 *file = new SceneFileV2();
        file->EnableDebugLog(false);
        SceneFileV2::eError loadResult = file->LoadScene(rootNodePath, this);
        SafeRelease(file);
				
        uint64 deltaTime = SystemTimer::Instance()->AbsoluteMS() - startTime;
        Logger::FrameworkDebug("[GETROOTNODE TIME] %dms (%ld)", deltaTime, deltaTime);

        if (loadResult != SceneFileV2::ERROR_NO_ERROR)
        {
            return 0;
        }
    }
    
	it = rootNodes.find(FILEPATH_MAP_KEY(rootNodePath));
	if (it != rootNodes.end())
	{
        ProxyNode * node = it->second;
        //int32 nowCount = node->GetNode()->GetChildrenCountRecursive();
		return node->GetNode();
	}
    return 0;
}

void Scene::ReleaseRootNode(const FilePath &rootNodePath)
{
	ProxyNodeMap::iterator it = rootNodes.find(FILEPATH_MAP_KEY(rootNodePath));
	if (it != rootNodes.end())
	{
        it->second->Release();
        rootNodes.erase(it);
	}
}
    
void Scene::ReleaseRootNode(Entity *nodeToRelease)
{
//	for (Map<String, Entity*>::iterator it = rootNodes.begin(); it != rootNodes.end(); ++it)
//	{
//        if (nodeToRelease == it->second) 
//        {
//            Entity * obj = it->second;
//            obj->Release();
//            rootNodes.erase(it);
//            return;
//        }
//	}
}
    
void Scene::SetupTestLighting()
{
#ifdef __DAVAENGINE_IPHONE__
//	glShadeModel(GL_SMOOTH);
//	// enable lighting
//	glEnable(GL_LIGHTING);
//	glEnable(GL_NORMALIZE);
//	
//	// deactivate all lights
//	for (int i=0; i<8; i++)  glDisable(GL_LIGHT0 + i);
//	
//	// ambiental light to nothing
//	GLfloat ambientalLight[]= {0.2f, 0.2f, 0.2f, 1.0f};
//	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientalLight);
//	
////	GLfloat light_ambient[] = { 0.0f, 0.0f, 0.0f, 1.0f };  // delete
//	//GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//	GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//	
//	GLfloat light_diffuse[4];
//	light_diffuse[0]=1.0f;
//	light_diffuse[1]=1.0f;
//	light_diffuse[2]=1.0f;
//	light_diffuse[3]=1.0f;
//	
//	GLfloat lightPos[] = { 0.0f, 0.0f, 1.0f, 0.0f };
//	
//	// activate this light
//	glEnable(GL_LIGHT0);
//	
//	//always position 0,0,0 because light  is moved with transformations
//	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
//	
//	// colors 
//	glLightfv(GL_LIGHT0, GL_AMBIENT, light_diffuse); // now like diffuse color
//	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
//	glLightfv(GL_LIGHT0, GL_SPECULAR,light_specular);
//	
//	//specific values for this light
//	glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1);
//	glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0);
//	glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0);
//	
//	//other values
//	glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 30.0f);
//	glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 0.0f);
//	GLfloat spotdirection[] = { 0.0f, 0.0f, -1.0f, 0.0f }; // irrelevant for this light (I guess)
//	glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, spotdirection); 
#endif
}
    
void Scene::Update(float timeElapsed)
{
    TIME_PROFILE("Scene::Update");
    
    uint64 time = SystemTimer::Instance()->AbsoluteMS();

    bool needShowStaticOcclusion = Renderer::GetOptions()->IsOptionEnabled(RenderOptions::DEBUG_DRAW_STATIC_OCCLUSION);
    if (needShowStaticOcclusion&&!staticOcclusionDebugDrawSystem)
    {
        staticOcclusionDebugDrawSystem = new StaticOcclusionDebugDrawSystem(this);
        AddSystem(staticOcclusionDebugDrawSystem, MAKE_COMPONENT_MASK(Component::STATIC_OCCLUSION_COMPONENT), 0, renderUpdateSystem);
    }else if (!needShowStaticOcclusion&&staticOcclusionDebugDrawSystem)
    {
        RemoveSystem(staticOcclusionDebugDrawSystem);
        SafeDelete(staticOcclusionDebugDrawSystem);
    }

    uint32 size = (uint32)systemsToProcess.size();
    for (uint32 k = 0; k < size; ++k)
    {
        SceneSystem * system = systemsToProcess[k];
        if((systemsMask & SCENE_SYSTEM_UPDATEBLE_FLAG) && system == transformSystem)
        {
            updatableSystem->UpdatePreTransform(timeElapsed);
            transformSystem->Process(timeElapsed);
            updatableSystem->UpdatePostTransform(timeElapsed);
        }
        else if(system == lodSystem)
        {
            if(Renderer::GetOptions()->IsOptionEnabled(RenderOptions::UPDATE_LODS))
            {
                lodSystem->Process(timeElapsed);
            }
        }
        else
        {
            system->Process(timeElapsed);
        }
    }

// 	int32 size;
// 	
// 	size = (int32)animations.size();
// 	for (int32 animationIndex = 0; animationIndex < size; ++animationIndex)
// 	{
// 		SceneNodeAnimationList * anim = animations[animationIndex];
// 		anim->Update(timeElapsed);
// 	}
// 
// 	if(Renderer::GetOptions()->IsOptionEnabled(RenderOptions::UPDATE_ANIMATED_MESHES))
// 	{
// 		size = (int32)animatedMeshes.size();
// 		for (int32 animatedMeshIndex = 0; animatedMeshIndex < size; ++animatedMeshIndex)
// 		{
// 			AnimatedMesh * mesh = animatedMeshes[animatedMeshIndex];
// 			mesh->Update(timeElapsed);
// 		}
// 	}	

    updateTime = SystemTimer::Instance()->AbsoluteMS() - time;
}

void Scene::Draw()
{
    TIME_PROFILE("Scene::Draw");

		
#if RHI_COMPLETE    
    if(NULL != sceneGlobalMaterial)
    {
        NMaterialProperty* propShadowColor = sceneGlobalMaterial->GetMaterialProperty(NMaterialParamName::PARAM_SHADOW_COLOR);
        if(NULL != propShadowColor)
        {
            DVASSERT(Shader::UT_FLOAT_VEC4 == propShadowColor->type);
            
            float32* propDataPtr = (float32*)propShadowColor->data;
            Color shadowColor(propDataPtr[0], propDataPtr[1], propDataPtr[2], propDataPtr[3]);
            renderSystem->SetShadowRectColor(shadowColor);
        }
    }
#endif // RHI_COMPLETE
    
    uint64 time = SystemTimer::Instance()->AbsoluteMS();        
    
    renderSystem->Render(clearBuffers);
    
    //foliageSystem->DebugDrawVegetation();
    
	drawTime = SystemTimer::Instance()->AbsoluteMS() - time;
}
    
void Scene::SceneDidLoaded()
{
    uint32 systemsCount = static_cast<uint32>(systems.size());
    for (uint32 k = 0; k < systemsCount; ++k)
    {
        systems[k]->SceneDidLoaded();
    }
}


	
// void Scene::StopAllAnimations(bool recursive )
// {
// 	int32 size = (int32)animations.size();
// 	for (int32 animationIndex = 0; animationIndex < size; ++animationIndex)
// 	{
// 		SceneNodeAnimationList * anim = animations[animationIndex];
// 		anim->StopAnimation();
// 	}
// 	Entity::StopAllAnimations(recursive);
// }
    
    
void Scene::SetCurrentCamera(Camera * _camera)
{
    SafeRelease(mainCamera);
    mainCamera = SafeRetain(_camera);
    SafeRelease(drawCamera);
    drawCamera = SafeRetain(_camera);
}

Camera * Scene::GetCurrentCamera() const
{
    return mainCamera;
}

void Scene::SetCustomDrawCamera(Camera * _camera)
{
    SafeRelease(drawCamera);
    drawCamera = SafeRetain(_camera);
}

Camera * Scene::GetDrawCamera() const
{
    return drawCamera;
}
 
//void Scene::SetForceLodLayer(int32 layer)
//{
//    forceLodLayer = layer;
//}
//int32 Scene::GetForceLodLayer()
//{
//    return forceLodLayer;
//}
//
//int32 Scene::RegisterLodLayer(float32 nearDistance, float32 farDistance)
//{
//    LodLayer newLevel;
//    newLevel.nearDistance = nearDistance;
//    newLevel.farDistance = farDistance;
//    newLevel.nearDistanceSq = nearDistance * nearDistance;
//    newLevel.farDistanceSq = farDistance * farDistance;
//    int i = 0;
//    
//    for (Vector<LodLayer>::iterator it = lodLayers.begin(); it < lodLayers.end(); it++)
//    {
//        if (nearDistance < it->nearDistance)
//        {
//            lodLayers.insert(it, newLevel);
//            return i;
//        }
//        i++;
//    }
//    
//    lodLayers.push_back(newLevel);
//    return i;
//}
//    
//void Scene::ReplaceLodLayer(int32 layerNum, float32 nearDistance, float32 farDistance)
//{
//    DVASSERT(layerNum < (int32)lodLayers.size());
//    
//    lodLayers[layerNum].nearDistance = nearDistance;
//    lodLayers[layerNum].farDistance = farDistance;
//    lodLayers[layerNum].nearDistanceSq = nearDistance * nearDistance;
//    lodLayers[layerNum].farDistanceSq = farDistance * farDistance;
//    
//    
////    LodLayer newLevel;
////    newLevel.nearDistance = nearDistance;
////    newLevel.farDistance = farDistance;
////    newLevel.nearDistanceSq = nearDistance * nearDistance;
////    newLevel.farDistanceSq = farDistance * farDistance;
////    int i = 0;
////    
////    for (Vector<LodLayer>::iterator it = lodLayers.begin(); it < lodLayers.end(); it++)
////    {
////        if (nearDistance < it->nearDistance)
////        {
////            lodLayers.insert(it, newLevel);
////            return i;
////        }
////        i++;
////    }
////    
////    lodLayers.push_back(newLevel);
////    return i;
//}
//    
    

    
void Scene::UpdateLights()
{            
    
}
    
Light * Scene::GetNearestDynamicLight(Light::eType type, Vector3 position)
{
    switch(type)
    {
        case Light::TYPE_DIRECTIONAL:
            
            break;
            
        default:
            break;
    };
    
	float32 squareMinDistance = 10000000.0f;
	Light * nearestLight = 0;

	Set<Light*> & lights = GetLights();
	const Set<Light*>::iterator & endIt = lights.end();
	for (Set<Light*>::iterator it = lights.begin(); it != endIt; ++it)
	{
		Light * node = *it;
		if(node->IsDynamic())
		{
			const Vector3 & lightPosition = node->GetPosition();

			float32 squareDistanceToLight = (position - lightPosition).SquareLength();
			if (squareDistanceToLight < squareMinDistance)
			{
				squareMinDistance = squareDistanceToLight;
				nearestLight = node;
			}
		}
	}

	return nearestLight;
}

Set<Light*> & Scene::GetLights()
{
    return lights;
}

EventSystem * Scene::GetEventSystem() const
{
	return eventSystem;
}

RenderSystem * Scene::GetRenderSystem() const
{
	return renderSystem;
}

MaterialSystem * Scene::GetMaterialSystem() const
{
    return materialSystem;
}

AnimationSystem * Scene::GetAnimationSystem() const
{
    return animationSystem;
}

/*void Scene::Save(KeyedArchive * archive)
{
    // Perform refactoring and add Matrix4, Vector4 types to VariantType and KeyedArchive
    Entity::Save(archive);
    
    
    
    
    
}

void Scene::Load(KeyedArchive * archive)
{
    Entity::Load(archive);
}*/
    

SceneFileV2::eError Scene::SaveScene(const DAVA::FilePath & pathname, bool saveForGame /*= false*/)
{
    ScopedPtr<SceneFileV2> file(new SceneFileV2());
	file->EnableDebugLog(false);
	file->EnableSaveForGame(saveForGame);
	return file->SaveScene(pathname, this);
}
    
void Scene::OptimizeBeforeExport()
{
#if RHI_COMPLETE
    Set<NMaterial*> materials;
    materialSystem->BuildMaterialList(this, materials);
    
    ImportShadowColor(this);

    Set<NMaterial *>::const_iterator endIt = materials.end();
    for(Set<NMaterial *>::const_iterator it = materials.begin(); it != endIt; ++it)
        (*it)->ReleaseIlluminationParams();


    Entity::OptimizeBeforeExport();
#endif  // RHI_COMPLETE
}

void Scene::ImportShadowColor(Entity * rootNode)
{
#if RHI_COMPLETE
    if(NULL != sceneGlobalMaterial)
    {
		Entity * landscapeNode = FindLandscapeEntity(rootNode);
		if(NULL != landscapeNode)
		{
			// try to get shadow color for landscape
			KeyedArchive * props = GetCustomPropertiesArchieve(landscapeNode);
			if (props->IsKeyExists("ShadowColor"))
			{
				Color shadowColor = props->GetVariant("ShadowColor")->AsColor();
				sceneGlobalMaterial->SetPropertyValue(NMaterialParamName::PARAM_SHADOW_COLOR,
					Shader::UT_FLOAT_VEC4,
					1,
					shadowColor.color);

				props->DeleteKey("ShadowColor");
			}
		}
    }
#endif  // RHI_COMPLETE
}

void Scene::OnSceneReady(Entity * rootNode)
{
    ImportShadowColor(rootNode);
}

void Scene::SetClearBuffers(uint32 buffers) 
{
    clearBuffers = buffers;
}
uint32 Scene::GetClearBuffers() const 
{
    return clearBuffers;
}

    
void Scene::Input(DAVA::UIEvent *event)
{
    uint32 size = (uint32)systemsToInput.size();
    for (uint32 k = 0; k < size; ++k)
    {
        SceneSystem * system = systemsToInput[k];
        system->Input(event);
    }
}
    
};
