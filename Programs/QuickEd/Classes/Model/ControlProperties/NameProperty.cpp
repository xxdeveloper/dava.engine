#include "Classes/Model/ControlProperties/NameProperty.h"

#include "Classes/Model/ControlProperties/PropertyVisitor.h"
#include "Classes/Model/PackageHierarchy/ControlNode.h"

#include <UI/UIControl.h>
#include <UI/UIControlHelpers.h>

NameProperty::NameProperty(ControlNode* controlNode_, const NameProperty* sourceProperty, eCloneType cloneType)
    : ValueProperty("Name", DAVA::Type::Instance<DAVA::String>())
    , controlNode(controlNode_)
{
    using namespace DAVA;

    FastName name;
    if (sourceProperty != nullptr)
    {
        name = sourceProperty->GetValue().Cast<FastName>();

        if (cloneType == CT_INHERIT && controlNode->GetCreationType() == ControlNode::CREATED_FROM_PROTOTYPE_CHILD)
        {
            AttachPrototypeProperty(sourceProperty);
        }
    }
    else
    {
        name = controlNode->GetControl()->GetName();
    }

    if (name.IsValid() == false)
    {
        name = FastName("");
    }

    value = name;
    if (UIControlHelpers::IsControlNameValid(name))
    {
        controlNode->GetControl()->SetName(name);
    }
    else
    {
        controlNode->GetControl()->SetName("generated");
    }
}

void NameProperty::Refresh(DAVA::int32 refreshFlags)
{
    ValueProperty::Refresh(refreshFlags);

    if ((refreshFlags & REFRESH_DEFAULT_VALUE) != 0 && GetPrototypeProperty())
        ApplyValue(GetDefaultValue());
}

void NameProperty::Accept(PropertyVisitor* visitor)
{
    visitor->VisitNameProperty(this);
}

bool NameProperty::IsReadOnly() const
{
    return controlNode->GetCreationType() == ControlNode::CREATED_FROM_PROTOTYPE_CHILD || ValueProperty::IsReadOnly();
}

NameProperty::ePropertyType NameProperty::GetType() const
{
    return TYPE_VARIANT;
}

DAVA::Any NameProperty::GetValue() const
{
    return value;
}

bool NameProperty::IsOverriddenLocally() const
{
    return controlNode->GetCreationType() != ControlNode::CREATED_FROM_PROTOTYPE_CHILD;
}

ControlNode* NameProperty::GetControlNode() const
{
    return controlNode;
}

void NameProperty::ApplyValue(const DAVA::Any& newValue)
{
    using namespace DAVA;

    if (newValue.CanCast<FastName>())
    {
        value = newValue;
        FastName name = newValue.Cast<FastName>();
        if (UIControlHelpers::IsControlNameValid(name))
        {
            controlNode->GetControl()->SetName(name);
        }
    }
    else
    {
        DVASSERT(false);
    }
}
