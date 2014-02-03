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


#include "Render/Material/NMaterial.h"
#include "Scene3D/Systems/MaterialSystem.h"
#include "Render/Highlevel/SkyboxRenderObject.h"
#include <Render/TextureDescriptor.h>

#include "Render/Material/NMaterialNames.h"

namespace DAVA
{

	//do not create lower cube face
	const int SKYBOX_VERTEX_COUNT = (5 * 6);

	SkyboxRenderObject::SkyboxRenderObject()
	:
	offsetZ(0.0f),
	rotationZ(0.0f),
	nonClippingDistance(0.0f)
	{
		bbox.AddPoint(Vector3(0, 0, 0));
		bbox.AddPoint(Vector3(1, 1, 1));
		
		type = RenderObject::TYPE_SKYBOX;
		AddFlag(RenderObject::ALWAYS_CLIPPING_VISIBLE);
	}
	
	SkyboxRenderObject::~SkyboxRenderObject()
	{
	}
		
	void SkyboxRenderObject::SetRenderSystem(RenderSystem * renderSystem)
	{
		if(GetRenderSystem())
		{
			GetRenderSystem()->UnregisterFromUpdate(this);
		}
		
		RenderObject::SetRenderSystem(renderSystem);
		
		if(GetRenderSystem())
		{
			GetRenderSystem()->RegisterForUpdate(this);
		}
	}
	
	void SkyboxRenderObject::Initialize(AABBox3& box)
	{
		bbox = box;
		
		CreateRenderData();
		BuildSkybox();
	}
	
	void SkyboxRenderObject::CreateRenderData()
	{
		if(renderBatchArray.size() == 0)
		{
			RenderDataObject* renderDataObj = new RenderDataObject();
							
			NMaterial* skyboxMaterial = NMaterial::CreateMaterialInstance(FastName("Skybox_material"),
																		  NMaterialName::SKYBOX,
																		  NMaterial::DEFAULT_QUALITY_NAME);
			
			RenderBatch* skyboxRenderBatch = new RenderBatch();
			skyboxRenderBatch->SetRenderDataObject(renderDataObj);
			skyboxRenderBatch->SetMaterial(skyboxMaterial);
			
			RenderObject::AddRenderBatch(skyboxRenderBatch);
			
			SafeRelease(renderDataObj);
			SafeRelease(skyboxMaterial);
			SafeRelease(skyboxRenderBatch);
		}
	}
	
	void SkyboxRenderObject::BuildSkybox()
	{
		DVASSERT(renderBatchArray.size() != 0);
		
		if(renderBatchArray.size() == 0)
		{
			return;
		}
		
		Vector3 cubeTexCoords[SKYBOX_VERTEX_COUNT] = {
			Vector3(1, 1, -1), Vector3(-1, 1, -1), Vector3(1, 1, 1), Vector3(1, 1, 1), Vector3(-1, 1, -1), Vector3(-1, 1, 1),
			Vector3(1, -1, -1), Vector3(1, 1, -1), Vector3(1, -1, 1), Vector3(1, -1, 1), Vector3(1, 1, -1), Vector3(1, 1, 1),
			Vector3(-1, -1, -1), Vector3(1, -1, -1), Vector3(-1, -1, 1), Vector3(-1, -1, 1), Vector3(1, -1, -1), Vector3(1, -1, 1),
			Vector3(-1, 1, -1), Vector3(-1, -1, -1), Vector3(-1, 1, 1), Vector3(-1, 1, 1), Vector3(-1, -1, -1), Vector3(-1, -1, 1),
			Vector3(1, 1, 1), Vector3(-1, 1, 1), Vector3(1, -1, 1), Vector3(1, -1, 1), Vector3(-1, 1, 1), Vector3(-1, -1, 1)
		};
		
		PolygonGroup* polygonGroup = new PolygonGroup();
		polygonGroup->AllocateData(EVF_VERTEX | EVF_CUBETEXCOORD0, SKYBOX_VERTEX_COUNT, SKYBOX_VERTEX_COUNT);
		
		//face 0 (right)+
		polygonGroup->SetCoord(0, Vector3(bbox.min.x, bbox.min.y, bbox.min.z));
		polygonGroup->SetCoord(1, Vector3(bbox.min.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(2, Vector3(bbox.max.x, bbox.min.y, bbox.min.z));
		polygonGroup->SetCoord(3, Vector3(bbox.max.x, bbox.min.y, bbox.min.z));
		polygonGroup->SetCoord(4, Vector3(bbox.min.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(5, Vector3(bbox.max.x, bbox.min.y, bbox.max.z));
		
		//face 1 (front)+
		polygonGroup->SetCoord(6, Vector3(bbox.max.x, bbox.min.y, bbox.min.z));
		polygonGroup->SetCoord(7, Vector3(bbox.max.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(8, Vector3(bbox.max.x, bbox.max.y, bbox.min.z));
		polygonGroup->SetCoord(9, Vector3(bbox.max.x, bbox.max.y, bbox.min.z));
		polygonGroup->SetCoord(10, Vector3(bbox.max.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(11, Vector3(bbox.max.x, bbox.max.y, bbox.max.z));

		//face 2 (left)
		polygonGroup->SetCoord(12, Vector3(bbox.max.x, bbox.max.y, bbox.min.z));
		polygonGroup->SetCoord(13, Vector3(bbox.max.x, bbox.max.y, bbox.max.z));
		polygonGroup->SetCoord(14, Vector3(bbox.min.x, bbox.max.y, bbox.min.z));
		polygonGroup->SetCoord(15, Vector3(bbox.min.x, bbox.max.y, bbox.min.z));
		polygonGroup->SetCoord(16, Vector3(bbox.max.x, bbox.max.y, bbox.max.z));
		polygonGroup->SetCoord(17, Vector3(bbox.min.x, bbox.max.y, bbox.max.z));
				
		//face 3 (back)
		polygonGroup->SetCoord(18, Vector3(bbox.min.x, bbox.max.y, bbox.min.z));
		polygonGroup->SetCoord(19, Vector3(bbox.min.x, bbox.max.y, bbox.max.z));
		polygonGroup->SetCoord(20, Vector3(bbox.min.x, bbox.min.y, bbox.min.z));
		polygonGroup->SetCoord(21, Vector3(bbox.min.x, bbox.min.y, bbox.min.z));
		polygonGroup->SetCoord(22, Vector3(bbox.min.x, bbox.max.y, bbox.max.z));
		polygonGroup->SetCoord(23, Vector3(bbox.min.x, bbox.min.y, bbox.max.z));
		
		//face 4 (top)
		polygonGroup->SetCoord(24, Vector3(bbox.min.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(25, Vector3(bbox.min.x, bbox.max.y, bbox.max.z));
		polygonGroup->SetCoord(26, Vector3(bbox.max.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(27, Vector3(bbox.max.x, bbox.min.y, bbox.max.z));
		polygonGroup->SetCoord(28, Vector3(bbox.min.x, bbox.max.y, bbox.max.z));
		polygonGroup->SetCoord(29, Vector3(bbox.max.x, bbox.max.y, bbox.max.z));
		
		for(int i = 0; i < SKYBOX_VERTEX_COUNT; ++i)
		{
			polygonGroup->SetIndex(i, i);
			polygonGroup->SetCubeTexcoord(0, i, cubeTexCoords[i]);
		}
				
		//could be any pair of opposite diagonal vertices
		Vector3 coord0;
		Vector3 coord1;
		polygonGroup->GetCoord(24, coord0);
		polygonGroup->GetCoord(29, coord1);
		Vector3 maxDistanceBetweenVertices = coord1 - coord0;
		nonClippingDistance = 0.5f * maxDistanceBetweenVertices.Length();
		
		polygonGroup->BuildBuffers();
		renderBatchArray[0]->SetPolygonGroup(polygonGroup);
		SafeRelease(polygonGroup);
	}
		
	void SkyboxRenderObject::RenderUpdate(Camera *camera, float32 timeElapsed)
	{
		Vector3 camPos = camera->GetPosition();
		
		//scale cube so it's not get clipped by zNear plane
		float32 zNear = camera->GetZNear();
		float32 scale = (nonClippingDistance + zNear) / nonClippingDistance;
		
		camPos.z += offsetZ;
		
		Matrix4& finalMatrix = *worldTransform;
		finalMatrix = Matrix4::MakeRotation(Vector3(0.0f, 0.0f, 1.0f), rotationZ) *
			Matrix4::MakeScale(Vector3(scale, scale, scale)) *
			Matrix4::MakeTranslation(camPos);
	}
	
	RenderObject* SkyboxRenderObject::Clone(RenderObject *newObject)
	{
		if(!newObject)
		{
			DVASSERT_MSG(IsPointerToExactClass<SkyboxRenderObject>(this), "Can clone only SkyboxRenderObject");
			newObject = new SkyboxRenderObject();
		}
		
		SkyboxRenderObject* skyboxRenderObject = static_cast<SkyboxRenderObject*>(newObject);
		
		skyboxRenderObject->type = type;
		skyboxRenderObject->flags = flags;
		skyboxRenderObject->RemoveFlag(RenderObject::MARKED_FOR_UPDATE);
		skyboxRenderObject->debugFlags = debugFlags;
		skyboxRenderObject->ownerDebugInfo = ownerDebugInfo;
		
		skyboxRenderObject->bbox = bbox;
		skyboxRenderObject->offsetZ = offsetZ;
		skyboxRenderObject->rotationZ = rotationZ;
		skyboxRenderObject->nonClippingDistance = nonClippingDistance;
		
		uint32 size = GetRenderBatchCount();
        skyboxRenderObject->renderBatchArray.reserve(size);
		for(uint32 i = 0; i < size; ++i)
		{
			RenderBatch *batch = GetRenderBatch(i)->Clone();
			skyboxRenderObject->AddRenderBatch(batch);
			batch->Release();
		}
		
		skyboxRenderObject->BuildSkybox();
		
		return newObject;
	}
	
	void SkyboxRenderObject::Save(KeyedArchive *archive, SerializationContext *serializationContext)
	{
		RenderObject::Save(archive, serializationContext);
		
		if(archive != NULL)
		{
			archive->SetFloat("skbxro.verticalOffset", offsetZ);
			archive->SetFloat("skbxro.rotation", rotationZ);
			archive->SetFloat("skbxro.noclipdist", nonClippingDistance);
		}
	}
	
	void SkyboxRenderObject::Load(KeyedArchive *archive, SerializationContext *serializationContext)
	{
		RenderObject::Load(archive, serializationContext);
		
		if(archive != NULL)
		{
			offsetZ = archive->GetFloat("skbxro.verticalOffset");
			rotationZ = archive->GetFloat("skbxro.rotation");
			nonClippingDistance = archive->GetFloat("skbxro.noclipdist");
		}
				
		bbox = renderBatchArray[0]->GetBoundingBox();
	}

	void SkyboxRenderObject::SetTexture(const FilePath& texPath)
	{
        DVASSERT(renderBatchArray.size() > 0);
        
        NMaterial* skyboxMaterial = renderBatchArray[0]->GetMaterial();
        
        //since the renderBatchArray is entirely controlled by SkyboxRenderObject
        //we can safely assume that objects in render batch array are properly initialized
        //and have material in place (no need to check for NULL)
        
        NMaterial* topParent = NULL;
        bool textureSet = false;
        while(skyboxMaterial)
        {
            Texture* tx = skyboxMaterial->GetTexture(NMaterial::TEXTURE_CUBEMAP);
            if(NULL != tx)
            {
                DAVA::Texture* tx = DAVA::Texture::CreateFromFile(texPath, Texture::TEXTURE_CUBE);
                skyboxMaterial->SetTexture(NMaterial::TEXTURE_CUBEMAP, tx);
                SafeRelease(tx);
                
                textureSet = true;
                
                break;
            }
            
            if(NULL == skyboxMaterial->GetParent())
            {
                topParent = skyboxMaterial;
            }
            
            skyboxMaterial = skyboxMaterial->GetParent();
        }
        
        if(!textureSet)
        {
            DAVA::Texture* tx = DAVA::Texture::CreateFromFile(texPath, Texture::TEXTURE_CUBE);
            topParent->SetTexture(NMaterial::TEXTURE_CUBEMAP, tx);
            SafeRelease(tx);
        }
	}
	
	FilePath SkyboxRenderObject::GetTexture()
	{
        DVASSERT(renderBatchArray.size() > 0);
        
        FilePath path;
        
        NMaterial* skyboxMaterial = renderBatchArray[0]->GetMaterial();
        
        while(skyboxMaterial)
        {
            Texture* tx = skyboxMaterial->GetTexture(NMaterial::TEXTURE_CUBEMAP);
            if(NULL != tx)
            {
                path = skyboxMaterial->GetTexturePath(NMaterial::TEXTURE_CUBEMAP);
                break;
            }
            
            skyboxMaterial = skyboxMaterial->GetParent();
        }
        
        return path;
	}
	
	void SkyboxRenderObject::SetOffsetZ(const float32& offset)
	{
		//VI: do not allow to set offset more that 1/2 of skybox height
		//VI see DF-1766
		if(renderBatchArray.size() > 0)
		{
			AABBox3 transformedBox;
			bbox.GetTransformedBox(*worldTransform, transformedBox);
			float32 maxOffset = 0.5f * (transformedBox.max.z - transformedBox.min.z);
			
			if(offset > maxOffset ||
			   offset < -maxOffset)
			{
				return;
			}
		}
		
		offsetZ = offset;
	}
	
	void SkyboxRenderObject::ForceSetOffsetZ(float32 offset)
	{
		offsetZ = offset;
	}
	
	float32 SkyboxRenderObject::GetOffsetZ()
	{
		return offsetZ;
	}
	
	void SkyboxRenderObject::SetRotationZ(const float32& rotation)
	{
		rotationZ = rotation;
	}
	
	float32 SkyboxRenderObject::GetRotationZ()
	{
		return rotationZ;
	}

};