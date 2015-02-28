#include <QMessageBox>

#include <unordered_map>

#include <Utils/UTF8Utils.h>

#include "Base/FunctionTraits.h"

#include "MemProfWidget.h"
#include "MemProfController.h"

using namespace DAVA;
using namespace DAVA::Net;

MemProfController::MemProfController(const DAVA::Net::PeerDescription& peerDescr, QWidget *_parentWidget, QObject* parent)
    : QObject(parent)
    , parentWidget(_parentWidget)
    , peer(peerDescr)
{
    ShowView();
    netClient.SetCallbacks(MakeFunction(this, &MemProfController::ChannelOpen),
                           MakeFunction(this, &MemProfController::ChannelClosed),
                           MakeFunction(this, &MemProfController::CurrentStat),
                           MakeFunction(this, &MemProfController::Dump),
                           MakeFunction(this, &MemProfController::DumpDone));
}

MemProfController::~MemProfController() {}

void MemProfController::OnDumpPressed()
{
    netClient.RequestDump();
}

void MemProfController::ShowView()
{
    if (NULL == view)
    {
        const QString title = QString("%1 (%2 %3)")
            .arg(peer.GetName().c_str())
            .arg(peer.GetPlatformString().c_str())
            .arg(peer.GetVersion().c_str());

        view = new MemProfWidget(parentWidget);
        view->setWindowFlags(Qt::Window);
        view->setWindowTitle(title);

        connect(view, SIGNAL(OnDumpButton()), this, SLOT(OnDumpPressed()));
        connect(this, &QObject::destroyed, view, &QObject::deleteLater);
    }
    view->showNormal();
    view->activateWindow();
    view->raise();
}

void MemProfController::ChannelOpen(const DAVA::MMStatConfig* config)
{
    view->ChangeStatus("connected", nullptr);
    view->ClearStat();

    Logger::Debug("MemProfController::ChannelOpen");
    if (config)
    {
        Logger::Debug("   maxTags=%u, ntags=%u", config->maxTagCount, config->tagCount);
        for (uint32 i = 0;i < config->tagCount;++i)
            Logger::Debug("      %d, %s", i, config->names[i].name);
        Logger::Debug("   maxPools=%u, npools=%u", config->maxAllocPoolCount, config->allocPoolCount);
        for (uint32 i = 0;i < config->allocPoolCount;++i)
            Logger::Debug("      %d, %s", i, config->names[i + config->tagCount].name);
    }
    view->SetStatConfig(config);
    
}

void MemProfController::ChannelClosed(const char8* message)
{
    view->ChangeStatus("disconnected", message);
}

void MemProfController::CurrentStat(const DAVA::MMStat* stat)
{
    view->UpdateStat(stat);
}

void MemProfController::Dump(size_t total, size_t recv)
{
    view->UpdateProgress(total, recv);
}

template<typename T>
inline T* Offset(void* ptr, size_t byteOffset)
{
    return reinterpret_cast<T*>(static_cast<uint8*>(ptr)+byteOffset);
}

template<typename T>
inline const T* Offset(const void* ptr, size_t byteOffset)
{
    return reinterpret_cast<const T*>(static_cast<const uint8*>(ptr)+byteOffset);
}

void MemProfController::DumpDone(const DAVA::MMDump* dump)
{
    using namespace DAVA;

    static int dumpIndex = 1;

    view->UpdateProgress(100, 100);

    std::unordered_map<uint64, String> symbolMap;

    const MMBlock* blocks = Offset<MMBlock>(dump, sizeof(MMDump));
    const MMBacktrace* bt = Offset<MMBacktrace>(blocks, sizeof(MMBlock) * dump->blockCount);
    const MMSymbol* symbols = Offset<MMSymbol>(bt, sizeof(MMBacktrace) * dump->backtraceCount);

    for (size_t i = 0, n = dump->symbolCount;i < n;++i)
        symbolMap.emplace(std::make_pair(symbols[i].addr, symbols[i].name));

    char fname[100];
    const char* prefix = 
#if defined(__DAVAENGINE_WIN32__)
        ""
#elif defined(__DAVAENGINE_MACOS__)
        "/Users/max/"
#endif
        ;
    {
        Snprintf(fname, COUNT_OF(fname), "%sdump_%d.bin", prefix, dumpIndex);
        FILE* f = fopen(fname, "wb");
        if (f)
        {
            size_t dumpSize = sizeof(MMDump) 
                + sizeof(MMBlock) * dump->blockCount 
                + sizeof(MMBacktrace) * dump->backtraceCount
                + sizeof(MMSymbol) * dump->symbolCount;
            fwrite(dump, 1, dumpSize, f);
            fclose(f);
        }
    }
    Snprintf(fname, COUNT_OF(fname), "%sdump_%d.log", prefix, dumpIndex++);
    FILE* f = fopen(fname, "wb");
    if (f)
    {
        fprintf(f, "General info\n");
        fprintf(f, "  blockCount=%u\n", dump->blockCount);
        fprintf(f, "  backtraceCount=%u\n", dump->backtraceCount);
        fprintf(f, "  nameCount=%u\n", dump->symbolCount);
        fprintf(f, "  blockBegin=%u\n", dump->blockBegin);
        fprintf(f, "  blockEnd=%u\n", dump->blockEnd);

        fprintf(f, "Blocks\n");
        for (uint32 i = 0;i < dump->blockCount;++i)
        {
            fprintf(f, "%4d: addr=%08llX, allocByApp=%u, allocTotal=%u, orderNo=%u, pool=%u, hash=%u\n", i + 1,
                    blocks[i].addr,
                    blocks[i].allocByApp, blocks[i].allocTotal, blocks[i].orderNo, blocks[i].pool,
                    blocks[i].backtraceHash);
            /*for (size_t j = 0;j < 16;++j)
            {
                uint64 addr = dump->blocks[i].backtrace.frames[j];
                const char* s = "";
                const MMSymbol* n = std::find_if(sym, sym + dump->symbolCount, [addr](const MMSymbol& mms) -> bool {
                    return mms.addr == addr;
                });
                if (n != sym + dump->symbolCount)
                    s = n->name;
                fprintf(f, "        %08llX    %s\n", dump->blocks[i].backtrace.frames[j], s);
            }*/
        }

        fprintf(f, "Symbols\n");
        int isym = 0;
        for (auto& x : symbolMap)
        {
            fprintf(f, "  %4d: %08llX; %s\n", isym + 1, x.first, x.second.c_str());
        }
        fclose(f);
    }
}

void MemProfController::Output(const String& msg)
{
    view->AppendText(msg.c_str());
}
