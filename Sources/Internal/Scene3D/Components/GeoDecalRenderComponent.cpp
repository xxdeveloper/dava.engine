#include "Scene3D/Components/GeoDecalRenderComponent.h"
#include "Scene3D/Entity.h"

namespace DAVA
{
DAVA_VIRTUAL_REFLECTION_IMPL(GeoDecalRenderComponent)
{
    ReflectionRegistrator<GeoDecalRenderComponent>::Begin()[M::CantBeCreatedManualyComponent()]
    .ConstructorByPointer()
    .End();
}

GeoDecalRenderComponent::GeoDecalRenderComponent()
{
}

Component* GeoDecalRenderComponent::Clone(Entity* toEntity)
{
    GeoDecalRenderComponent* result = new GeoDecalRenderComponent();
    result->SetEntity(toEntity);
    return result;
}

void GeoDecalRenderComponent::Serialize(KeyedArchive* archive, SerializationContext* serializationContext)
{
}

void GeoDecalRenderComponent::Deserialize(KeyedArchive* archive, SerializationContext* serializationContext)
{
}
}
