#pragma once

#include <Command/Command.h>
#include <Base/RefPtr.h>

class PackageNode;

class QEPackageCommand : public DAVA::Command
{
public:
    QEPackageCommand(PackageNode* package, DAVA::int32 commandID, const DAVA::String& description = "");

protected:
    DAVA::RefPtr<PackageNode> package;
};
