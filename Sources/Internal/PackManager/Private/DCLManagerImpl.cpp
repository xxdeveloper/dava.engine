#include "PackManager/Private/DCLManagerImpl.h"
#include "FileSystem/FileList.h"
#include "FileSystem/Private/PackArchive.h"
#include "FileSystem/Private/PackMetaData.h"
#include "DLC/Downloader/DownloadManager.h"
#include "Utils/CRC32.h"
#include "Utils/StringUtils.h"
#include "Utils/StringFormat.h"
#include "DLC/DLC.h"
#include "Logger/Logger.h"
#include "Base/Exception.h"
#include "Concurrency/Mutex.h"
#include "Concurrency/LockGuard.h"

#include <algorithm>

namespace DAVA
{
IDLCManager::~IDLCManager() = default;
IDLCManager::IRequest::~IRequest() = default;

const String& DCLManagerImpl::ToString(DCLManagerImpl::InitState state)
{
    static Vector<String> states{
        "Starting",
        "LoadingRequestAskFooter",
        "LoadingRequestGetFooter",
        "LoadingRequestAskFileTable",
        "LoadingRequestGetFileTable",
        "CalculateLocalDBHashAndCompare",
        "LoadingRequestAskMeta",
        "LoadingRequestGetMeta",
        "UnpakingDB",
        "DeleteDownloadedPacksIfNotMatchHash",
        "LoadingPacksDataFromLocalMeta",
        "MountingDownloadedPacks",
        "Ready",
        "Offline"
    };
    DVASSERT(states.size() == 14);
    return states.at(static_cast<size_t>(state));
}

const String& DCLManagerImpl::ToString(DCLManagerImpl::InitError state)
{
    static Vector<String> states{
        "AllGood",
        "CantCopyLocalDB",
        "CantMountLocalPacks",
        "LoadingRequestFailed",
        "UnpackingDBFailed",
        "DeleteDownloadedPackFailed",
        "LoadingPacksDataFailed",
        "MountingDownloadedPackFailed"
    };
    DVASSERT(states.size() == 8);
    return states.at(static_cast<size_t>(state));
}

static void WriteBufferToFile(const Vector<uint8>& outDB, const FilePath& path)
{
    ScopedPtr<File> f(File::Create(path, File::WRITE | File::CREATE));
    if (!f)
    {
        DAVA_THROW(DAVA::Exception, "can't create file for local DB: " + path.GetStringValue());
    }

    uint32 written = f->Write(outDB.data(), static_cast<uint32>(outDB.size()));
    if (written != outDB.size())
    {
        DAVA_THROW(DAVA::Exception, "can't write file for local DB: " + path.GetStringValue());
    }
}

#ifdef __DAVAENGINE_COREV2__
DCLManagerImpl::DCLManagerImpl(Engine* engine_)
    : engine(*engine_)
{
    DVASSERT(Thread::IsMainThread());
    sigConnectionUpdate = engine.update.Connect(this, &DCLManagerImpl::Update);
}

DCLManagerImpl::~DCLManagerImpl()
{
    DVASSERT(Thread::IsMainThread());
    engine.update.Disconnect(sigConnectionUpdate);
}
#endif

void DCLManagerImpl::Initialize(const FilePath& dirToDownloadPacks_,
                                const String& urlToServerSuperpack_,
                                const Hints& hints_)
{
    DVASSERT(Thread::IsMainThread());
    // TODO check if signal asyncConnectStateChanged has any subscriber

    if (!IsInitialized())
    {
        dirToDownloadedPacks = dirToDownloadPacks_;

        FileSystem* fs = FileSystem::Instance();
        if (FileSystem::DIRECTORY_CANT_CREATE == fs->CreateDirectory(dirToDownloadedPacks, true))
        {
            DAVA_THROW(DAVA::Exception, "can't create directory for packs: " + dirToDownloadedPacks.GetStringValue());
        }

        urlToSuperPack = urlToServerSuperpack_;
        hints = hints_;
    }

    // if Initialize called second time
    fullSizeServerData = 0;
    if (0 != downloadTaskId)
    {
        DownloadManager::Instance()->Cancel(downloadTaskId);
        downloadTaskId = 0;
    }

    initError = InitError::AllGood;
    initState = InitState::LoadingRequestAskFooter;
}

bool DCLManagerImpl::IsInitialized() const
{
    // current inputState can be in differect states becouse of
    // offline mode
    LockGuard<Mutex> lock(protectPM);

    bool requestManagerCreated = requestManager != nullptr;

    return requestManagerCreated;
}

// start ISync //////////////////////////////////////
DCLManagerImpl::InitState DCLManagerImpl::GetInitState() const
{
    DVASSERT(Thread::IsMainThread());
    return initState;
}

DCLManagerImpl::InitError DCLManagerImpl::GetInitError() const
{
    DVASSERT(Thread::IsMainThread());
    return initError;
}

const String& DCLManagerImpl::GetLastErrorMessage() const
{
    DVASSERT(Thread::IsMainThread());
    return initErrorMsg;
}

void DCLManagerImpl::RetryInit()
{
    DVASSERT(Thread::IsMainThread());

    // clear error state
    Initialize(dirToDownloadedPacks, urlToSuperPack, hints);

    // wait and then try again
    timeWaitingNextInitializationAttempt = hints.retryConnectMilliseconds / 1000.f; // to seconds
    retryCount++;
    initState = InitState::Offline;
}

// end Initialization ////////////////////////////////////////

void DCLManagerImpl::Update(float frameDelta)
{
    DVASSERT(Thread::IsMainThread());

    try
    {
        if (InitState::Starting != initState)
        {
            if (initState != InitState::Ready)
            {
                ContinueInitialization(frameDelta);
            }
            else if (isProcessingEnabled)
            {
                if (requestManager)
                {
                    requestManager->Update();
                }
            }
        }
    }
    catch (std::exception& ex)
    {
        Logger::Error("PackManager error: %s", ex.what());
        throw; // crush or let parent code decide
    }
}

void DCLManagerImpl::ContinueInitialization(float frameDelta)
{
    if (timeWaitingNextInitializationAttempt > 0.f)
    {
        timeWaitingNextInitializationAttempt -= frameDelta;
        if (timeWaitingNextInitializationAttempt <= 0.f)
        {
            timeWaitingNextInitializationAttempt = 0.f;
            initState = InitState::LoadingRequestAskFooter;
        }
        else
        {
            return;
        }
    }

    const InitState beforeState = initState;

    if (InitState::Starting == initState)
    {
        initState = InitState::LoadingRequestAskFooter;
    }
    else if (InitState::LoadingRequestAskFooter == initState)
    {
        AskFooter();
    }
    else if (InitState::LoadingRequestGetFooter == initState)
    {
        GetFooter();
    }
    else if (InitState::LoadingRequestAskFileTable == initState)
    {
        AskFileTable();
    }
    else if (InitState::LoadingRequestGetFileTable == initState)
    {
        GetFileTable();
    }
    else if (InitState::CalculateLocalDBHashAndCompare == initState)
    {
        CompareLocalMetaWitnRemoteHash();
    }
    else if (InitState::LoadingRequestAskMeta == initState)
    {
        AskMeta();
    }
    else if (InitState::LoadingRequestGetMeta == initState)
    {
        GetMeta();
    }
    else if (InitState::UnpakingDB == initState)
    {
        ParseMeta();
    }
    else if (InitState::DeleteDownloadedPacksIfNotMatchHash == initState)
    {
        DeleteOldPacks();
    }
    else if (InitState::LoadingPacksDataFromLocalMeta == initState)
    {
        LoadPacksDataFromDB();
    }
    else if (InitState::MountingDownloadedPacks == initState)
    {
        MountDownloadedPacks();
    }
    else if (InitState::Ready == initState)
    {
        // happy end
    }

    const InitState newState = initState;

    if (newState != beforeState || initError != InitError::AllGood)
    {
        if (initError != InitError::AllGood)
        {
            networkReady.Emit(false);
            RetryInit();
        }
        else
        {
            networkReady.Emit(true);
        }
    }
}

void DCLManagerImpl::AskFooter()
{
    //Logger::FrameworkDebug("pack manager ask_footer");

    DownloadManager* dm = DownloadManager::Instance();

    DVASSERT(0 == fullSizeServerData);

    if (0 == downloadTaskId)
    {
        downloadTaskId = dm->Download(urlToSuperPack, "", GET_SIZE);
    }
    else
    {
        DownloadStatus status = DL_UNKNOWN;
        if (dm->GetStatus(downloadTaskId, status))
        {
            if (DL_FINISHED == status)
            {
                DownloadError error = DLE_NO_ERROR;
                dm->GetError(downloadTaskId, error);
                if (DLE_NO_ERROR == error)
                {
                    if (!dm->GetTotal(downloadTaskId, fullSizeServerData))
                    {
                        DAVA_THROW(DAVA::Exception, "can't get size of file on server side");
                    }

                    if (fullSizeServerData < sizeof(PackFormat::PackFile))
                    {
                        DAVA_THROW(DAVA::Exception, "too small superpack on server");
                    }
                    // start downloading footer from server superpack
                    uint64 downloadOffset = fullSizeServerData - sizeof(initFooterOnServer);
                    uint32 sizeofFooter = static_cast<uint32>(sizeof(initFooterOnServer));
                    downloadTaskId = dm->DownloadIntoBuffer(urlToSuperPack, &initFooterOnServer, sizeofFooter, downloadOffset, sizeofFooter);
                    initState = InitState::LoadingRequestGetFooter;
                }
                else
                {
                    initError = InitError::LoadingRequestFailed;
                    initErrorMsg = "failed get superpack size on server, download error: " + DLC::ToString(error) + " " + std::to_string(retryCount);
                    Logger::Error("%s", initErrorMsg.c_str());
                }
            }
        }
    }
}

void DCLManagerImpl::GetFooter()
{
    //Logger::FrameworkDebug("pack manager get_footer");

    DownloadManager* dm = DownloadManager::Instance();
    DownloadStatus status = DL_UNKNOWN;
    if (dm->GetStatus(downloadTaskId, status))
    {
        if (DL_FINISHED == status)
        {
            DownloadError error = DLE_NO_ERROR;
            dm->GetError(downloadTaskId, error);
            if (DLE_NO_ERROR == error)
            {
                uint32 crc32 = CRC32::ForBuffer(reinterpret_cast<char*>(&initFooterOnServer.info), sizeof(initFooterOnServer.info));
                if (crc32 != initFooterOnServer.infoCrc32)
                {
                    DAVA_THROW(DAVA::Exception, "on server bad superpack!!! Footer not match crc32");
                }
                usedPackFile.footer = initFooterOnServer;
                initState = InitState::LoadingRequestAskFileTable;
            }
            else
            {
                initError = InitError::LoadingRequestFailed;
                initErrorMsg = "failed get footer from server, download error: " + DLC::ToString(error) + " " + std::to_string(retryCount);
            }
        }
    }
    else
    {
        DAVA_THROW(DAVA::Exception, "can't get status for download task");
    }
}

void DCLManagerImpl::AskFileTable()
{
    //Logger::FrameworkDebug("pack manager ask_file_table");

    DownloadManager* dm = DownloadManager::Instance();
    buffer.resize(initFooterOnServer.info.filesTableSize);

    uint64 downloadOffset = fullSizeServerData - (sizeof(initFooterOnServer) + initFooterOnServer.info.filesTableSize);

    downloadTaskId = dm->DownloadIntoBuffer(urlToSuperPack, buffer.data(), static_cast<uint32>(buffer.size()), downloadOffset, buffer.size());
    if (0 == downloadTaskId)
    {
        DAVA_THROW(DAVA::Exception, "can't start downloading into buffer");
    }
    initState = InitState::LoadingRequestGetFileTable;
}

void DCLManagerImpl::GetFileTable()
{
    //Logger::FrameworkDebug("pack manager get_file_table");

    DownloadManager* dm = DownloadManager::Instance();
    DownloadStatus status = DL_UNKNOWN;
    if (dm->GetStatus(downloadTaskId, status))
    {
        if (DL_FINISHED == status)
        {
            DownloadError error = DLE_NO_ERROR;
            dm->GetError(downloadTaskId, error);
            if (DLE_NO_ERROR == error)
            {
                uint32 crc32 = CRC32::ForBuffer(buffer.data(), buffer.size());
                if (crc32 != initFooterOnServer.info.filesTableCrc32)
                {
                    DAVA_THROW(DAVA::Exception, "on server bad superpack!!! FileTable not match crc32");
                }

                String fileNames;
                PackArchive::ExtractFileTableData(initFooterOnServer, buffer, fileNames, usedPackFile.filesTable);
                initFileData.clear(); // in case of second initialize
                initfilesInfo.clear(); // in case of second initialize
                PackArchive::FillFilesInfo(usedPackFile, fileNames, initFileData, initfilesInfo);

                initState = InitState::CalculateLocalDBHashAndCompare;
            }
            else
            {
                initError = InitError::LoadingRequestFailed;
                initErrorMsg = "failed get fileTable from server, download error: " + DLC::ToString(error) + " " + std::to_string(retryCount);
            }
        }
    }
    else
    {
        DAVA_THROW(DAVA::Exception, "can't get status for download task");
    }
}

void DCLManagerImpl::CompareLocalMetaWitnRemoteHash()
{
    //Logger::FrameworkDebug("pack manager calc_local_db_with_remote_crc32");

    FileSystem* fs = FileSystem::Instance();

    if (fs->IsFile(metaLocalCache))
    {
        const uint32 localCrc32 = CRC32::ForFile(metaLocalCache);
        if (localCrc32 != initFooterOnServer.metaDataCrc32)
        {
            DeleteLocalDBFiles();
            // we have to download new localDB file from server!
            initState = InitState::LoadingRequestAskMeta;
        }
        else
        {
            // all good go to
            initState = InitState::LoadingPacksDataFromLocalMeta;
        }
    }
    else
    {
        DeleteLocalDBFiles();

        initState = InitState::LoadingRequestAskMeta;
    }
}

void DCLManagerImpl::AskMeta()
{
    //Logger::FrameworkDebug("pack manager ask_db");

    DownloadManager* dm = DownloadManager::Instance();

    uint64 internalDataSize = initFooterOnServer.metaDataSize +
    initFooterOnServer.info.filesTableSize +
    initFooterOnServer.info.namesSizeCompressed +
    sizeof(initFooterOnServer);

    uint64 downloadOffset = fullSizeServerData - internalDataSize;
    uint64 downloadSize = initFooterOnServer.metaDataSize;

    buffer.resize(static_cast<size_t>(downloadSize));

    downloadTaskId = dm->DownloadIntoBuffer(urlToSuperPack, buffer.data(), static_cast<uint32>(buffer.size()), downloadOffset, downloadSize);
    DVASSERT(0 != downloadTaskId);

    initState = InitState::LoadingRequestGetMeta;
}

void DCLManagerImpl::GetMeta()
{
    //Logger::FrameworkDebug("pack manager get_db");

    DownloadManager* dm = DownloadManager::Instance();
    DownloadStatus status = DL_UNKNOWN;
    if (dm->GetStatus(downloadTaskId, status))
    {
        if (DL_FINISHED == status)
        {
            DownloadError error = DLE_NO_ERROR;
            dm->GetError(downloadTaskId, error);
            if (DLE_NO_ERROR == error)
            {
                initState = InitState::UnpakingDB;
            }
            else
            {
                initError = InitError::LoadingRequestFailed;
                initErrorMsg = "failed get meta from server, download error: " + DLC::ToString(error) + " " + std::to_string(retryCount);
            }
        }
    }
    else
    {
        DAVA_THROW(DAVA::Exception, "can't get status for download task");
    }
}

void DCLManagerImpl::ParseMeta()
{
    //Logger::FrameworkDebug("pack manager unpacking_db");

    uint32 buffCrc32 = CRC32::ForBuffer(reinterpret_cast<char*>(buffer.data()), static_cast<uint32>(buffer.size()));

    if (buffCrc32 != initFooterOnServer.metaDataCrc32)
    {
        DAVA_THROW(DAVA::Exception, "on server bad superpack!!! Footer meta not match crc32");
    }

    WriteBufferToFile(buffer, metaLocalCache);

    buffer.clear();
    buffer.shrink_to_fit();

    initState = InitState::DeleteDownloadedPacksIfNotMatchHash;
}

void DCLManagerImpl::StoreAllMountedPackNames()
{
}

void DCLManagerImpl::DeleteOldPacks()
{
    //Logger::FrameworkDebug("pack manager delete_old_packs");

    StoreAllMountedPackNames();

    initState = InitState::LoadingPacksDataFromLocalMeta;
}

void DCLManagerImpl::ReloadState()
{
    ScopedPtr<File> metaFile(new DAVA::File::Create(metaLocalCache, File::OPEN | File::READ));
    std::unique_ptr<PackMetaData> tmpDb(new PackMetaData(metaLocalCache));
    Vector<Pack> tmpPacks;

    InitializePacksFromDB(*tmpDb, tmpPacks);

    UnorderedMap<String, uint32> tmpIndex;
    BuildPackIndex(tmpIndex, tmpPacks);

    std::unique_ptr<RequestManager> tmpRequestManager(new RequestManager(*this));

    // if no exceptions switch to new objects

    db.swap(tmpDb);
    packs.swap(tmpPacks);
    packsIndex.swap(tmpIndex);
    requestManager.swap(tmpRequestManager);

    // now request all previouslly mounted pack
    for (const String& packName : tmpOldMountedPackNames)
    {
        const Pack& p = RequestPack(packName);
        if (p.state == Pack::Status::Requested)
        {
            // move in begin of queue
            SetRequestOrder(packName, 0.f);
        }
    }
    tmpOldMountedPackNames.clear();
    tmpOldMountedPackNames.shrink_to_fit();

    // next move old requests to new manager
    const Vector<PackRequest>& requests = tmpRequestManager->GetRequests();

    for (const PackRequest& request : requests)
    {
        const String& packName = request.GetRootPack().name;
        float32 priority = request.GetPriority();
        requestManager->Push(packName, priority);
    }
}

void DCLManagerImpl::LoadPacksDataFromDB()
{
    //Logger::FrameworkDebug("pack manager load_packs_data_from_db");

    if (IsInitialized())
    {
        // 1. create new objects for db, packs, packsIndex, requestManager
        // 2. transit all pack request from old requestManager
        // 3. reset db, packs, packsIndex, requestManager to new objects
        try
        {
            ReloadState();
        }
        catch (std::exception& ex)
        {
            Logger::Error("can't reinitialize new DB during runtime: %s", ex.what());
            throw;
        }
    }
    else
    {
        // now build all packs from localDB, later after request to server
        // we can delete localDB and replace with new from server if needed
        bool dbInMemory = true;
        db.reset(new PacksDB(metaLocalCache, dbInMemory));

        InitializePacksFromDB(*db, packs);

        BuildPackIndex(packsIndex, packs);

        // now user can do requests for local packs
        requestManager.reset(new RequestManager(*this));
    }

    initState = InitState::MountingDownloadedPacks;
}

void DCLManagerImpl::MountDownloadedPacks()
{
    //Logger::FrameworkDebug("pack manager mount_downloaded_packs");

    FileSystem* fs = FileSystem::Instance();

    // now mount all downloaded packs
    // we have to mount all packs togather it's a client requerement
    ScopedPtr<FileList> packFiles(new FileList(dirToDownloadedPacks, false));
    for (unsigned i = 0; i < packFiles->GetCount(); ++i)
    {
        if (packFiles->IsDirectory(i))
        {
            continue;
        }
        const FilePath& packPath = packFiles->GetPathname(i);
        if (packPath.GetExtension() == RequestManager::packPostfix)
        {
            try
            {
                Pack& pack = GetPack(packPath.GetBasename());
                if (!fs->IsMounted(packPath))
                {
                    fs->Mount(packPath, "Data/");
                }
                // do not do! pack.state = Pack::Status::Mounted;
                // client code should request pack first
                // we need it for consistensy with pack dependency
            }
            catch (std::exception& ex)
            {
                Logger::Error("can't auto mount pack on init: %s, cause: %s, so delete it", packPath.GetAbsolutePathname().c_str(), ex.what());
                fs->DeleteFile(packPath);
            }
        }
    }

    initState = InitState::Ready;
}

void DCLManagerImpl::DeleteLocalDBFiles()
{
    FileSystem* fs = FileSystem::Instance();
    fs->DeleteFile(metaLocalCache);
    fs->DeleteFile(dbLocalNameZipped);
}

void DCLManagerImpl::UnmountAllPacks()
{
    for (auto& pack : packs)
    {
        if (pack.state == Pack::Status::Mounted)
        {
            FileSystem::Instance()->Unmount(pack.name);
        }
    }
}

static void CheckPackCrc32(const FilePath& path, const uint32 hashFromDB)
{
    uint32 crc32ForFile = CRC32::ForFile(path);
    if (crc32ForFile != hashFromDB)
    {
        const char* str = path.GetStringValue().c_str();

        String msg = Format(
        "crc32 not match for pack %s, crc32 from DB 0x%X crc32 from file 0x%X",
        str, hashFromDB, crc32ForFile);

        DAVA_THROW(DAVA::Exception, msg.c_str());
    }
}

void DCLManagerImpl::MountPackWithDependencies(Pack& pack, const FilePath& path)
{
    FileSystem* fs = FileSystem::Instance();
    // first check all dependencies already mounted and mount if not
    // 1. collect dependencies
    Vector<Pack*> dependencies;
    dependencies.reserve(64);
    CollectDownloadableDependency(*this, pack.name, dependencies);
    // 2. check every dependency
    Vector<Pack*> notMounted;
    notMounted.reserve(dependencies.size());
    for (Pack* packItem : dependencies)
    {
        if (packItem->state != Pack::Status::Mounted)
        {
            notMounted.push_back(packItem);
        }
    }
    // 3. mount packs
    const FilePath packsDir = path.GetDirectory();
    for (Pack* packItem : notMounted)
    {
        try
        {
            const FilePath packPath = packsDir.GetStringValue() + packItem->name + RequestManager::packPostfix;

            CheckPackCrc32(packPath, packItem->hashFromDB);

            fs->Mount(packPath, "Data/");
            packItem->state = Pack::Status::Mounted;
        }
        catch (std::exception& ex)
        {
            Logger::Error("can't mount dependent pack: %s cause: %s", packItem->name.c_str(), ex.what());
            throw;
        }
    }

    try
    {
        CheckPackCrc32(path, pack.hashFromDB);
    }
    catch (std::exception& ex)
    {
        fs->Unmount(path);
        if (!fs->DeleteFile(path))
        {
            DAVA_THROW(DAVA::Exception, "can't delete old mounted pack: " + path.GetStringValue() + " " + ex.what());
        }

        throw;
    }

    fs->Mount(path, "Data/");
    pack.state = Pack::Status::Mounted;
}

void DCLManagerImpl::CollectDownloadableDependency(DCLManagerImpl& pm, const String& packName, Vector<Pack*>& dependency)
{
    const Pack& packState = pm.FindPack(packName);
    for (const String& dependName : packState.dependency)
    {
        Pack* dependPack = nullptr;
        try
        {
            dependPack = &pm.GetPack(dependName);
        }
        catch (std::exception& ex)
        {
            Logger::Error("pack \"%s\" has dependency to base pack \"%s\", error: %s", packName.c_str(), dependName.c_str(), ex.what());
            continue;
        }

        if (dependPack->state != Pack::Status::Mounted)
        {
            if (find(begin(dependency), end(dependency), dependPack) == end(dependency))
            {
                dependency.push_back(dependPack);
            }

            CollectDownloadableDependency(pm, dependName, dependency);
        }
    }
}

const IPackManager::Pack& DCLManagerImpl::RequestPack(const String& packName)
{
    DVASSERT(Thread::IsMainThread());

    if (!IsInitialized())
    {
        static Pack p;
        if (p.otherErrorMsg.empty())
        {
            p.state = Pack::Status::OtherError;
            p.otherErrorMsg = "initialization not finished";
        }
        return p;
    }

    if (requestManager)
    {
        Pack& pack = GetPack(packName);
        if (pack.state == Pack::Status::NotRequested)
        {
            // first try mount pack in it exist on local dounload dir
            FilePath path = dirToDownloadedPacks + "/" + packName + RequestManager::packPostfix;
            FileSystem* fs = FileSystem::Instance();
            if (fs->Exists(path))
            {
                try
                {
                    MountPackWithDependencies(pack, path);
                }
                catch (std::exception& ex)
                {
                    Logger::Error("%s", ex.what());
                    requestManager->Push(packName, 1.0f); // 1.0f last order by default
                }
            }
            else
            {
                requestManager->Push(packName, 1.0f); // 1.0f last order by default
            }
        }
        else if (pack.state == Pack::Status::Mounted)
        {
            // pass
        }
        else if (pack.state == Pack::Status::Downloading)
        {
            // pass
        }
        else if (pack.state == Pack::Status::ErrorLoading)
        {
            requestManager->Push(packName, 1.0f);
        }
        else if (pack.state == Pack::Status::OtherError)
        {
            requestManager->Push(packName, 1.0f);
        }
        else if (pack.state == Pack::Status::Requested)
        {
            // pass
        }
        return pack;
    }
    DAVA_THROW(DAVA::Exception, "can't process request initialization not finished");
}

void DCLManagerImpl::ListFilesInPacks(const FilePath& relativePathDir, const Function<void(const FilePath&, const String&)>& fn)
{
    DVASSERT(Thread::IsMainThread());
    DVASSERT(IsInitialized());

    if (!relativePathDir.IsDirectoryPathname())
    {
        Logger::Error("can't list not directory path: %s", relativePathDir.GetStringValue().c_str());
        return;
    }

    Set<String> addedDirectory;

    const String relative = relativePathDir.GetRelativePathname("~res:/");

    auto filterMountedPacks = [&](const String& path, const String& pack)
    {
        try
        {
            const Pack& p = FindPack(pack);
            if (p.state == Pack::Status::Mounted)
            {
                size_type index = path.find_first_of("/", relative.size());
                if (String::npos != index)
                {
                    String directoryName = path.substr(relative.size(), index - relative.size());
                    if (addedDirectory.find(directoryName) == end(addedDirectory))
                    {
                        addedDirectory.insert(directoryName);
                        fn("~res:/" + relative + "/" + directoryName + "/", pack);
                    }
                }
                else
                {
                    fn("~res:/" + path, pack);
                }
            }
        }
        catch (std::exception& ex)
        {
            Logger::Error("error while list files in pack: %s", ex.what());
        }
    };

    db->ListFiles(relative, filterMountedPacks);
}

const IPackManager::IRequest* DCLManagerImpl::FindRequest(const String& packName) const
{
    DVASSERT(Thread::IsMainThread());
    try
    {
        return &requestManager->Find(packName);
    }
    catch (std::exception&)
    {
        return nullptr;
    }
}

void DCLManagerImpl::SetRequestOrder(const String& packName, float newPriority)
{
    DVASSERT(Thread::IsMainThread());
    if (requestManager->IsInQueue(packName))
    {
        requestManager->UpdatePriority(packName, newPriority);
    }
}

void DCLManagerImpl::MountOnePack(const FilePath& filePath)
{
    String fileName = filePath.GetBasename();
    auto it = packsIndex.find(fileName);
    if (it == end(packsIndex))
    {
        DAVA_THROW(DAVA::Exception, "can't find pack: " + fileName + " in packIndex");
    }

    Pack& pack = packs.at(it->second);

    try
    {
        FileSystem* fs = FileSystem::Instance();
        fs->Mount(filePath, "Data/");
        pack.state = Pack::Status::Mounted;
    }
    catch (std::exception& ex)
    {
        Logger::Error("%s", ex.what());
    }
}

void DCLManagerImpl::MountPacks(const Set<FilePath>& basePacks)
{
    for_each(begin(basePacks), end(basePacks), [this](const FilePath& filePath)
             {
                 MountOnePack(filePath);
             });
}

void DCLManagerImpl::DeletePack(const String& packName)
{
    DVASSERT(Thread::IsMainThread());

    auto& pack = GetPack(packName);

    // first modify pack
    pack.state = Pack::Status::NotRequested;
    pack.priority = 0.0f;
    pack.downloadProgress = 0.f;
    pack.downloadError = DLE_NO_ERROR;

    // now remove archive from filesystem
    FilePath archivePath = dirToDownloadedPacks + packName + RequestManager::packPostfix;
    FileSystem* fs = FileSystem::Instance();
    fs->Unmount(archivePath);
    fs->DeleteFile(archivePath);

    // now we in inconsistent state! some packs may depends on it
    // and it's state may be `Pack::Status::Mounted`
    // so just insure to find out all dependent packs and set state to it
    // `Pack::Status::NotRequested`
    for (auto& p : packs)
    {
        db->ListDependentPacks(p.name, [&](const String& depName)
                               {
                                   GetPack(depName).state = Pack::Status::NotRequested;
                               });
    }
}

uint32_t DCLManagerImpl::DownloadPack(const String& packName, const FilePath& packPath)
{
    Pack& pack = GetPack(packName);
    String serverRelativePackFileName = packName + RequestManager::packPostfix;

    if (pack.isGPU)
    {
        serverRelativePackFileName = architecture + "/" + serverRelativePackFileName;
    }
    else
    {
        serverRelativePackFileName = "common/" + serverRelativePackFileName;
    }

    auto it = initFileData.find(serverRelativePackFileName);

    if (it == end(initFileData))
    {
        DAVA_THROW(DAVA::Exception, "can't find pack file: " + serverRelativePackFileName);
    }

    uint64 downloadOffset = it->second->startPosition;
    uint64 downloadSize = it->second->originalSize;

    DownloadManager* dm = DownloadManager::Instance();
    const String& url = GetSuperPackUrl();
    uint32 result = dm->DownloadRange(url, packPath, downloadOffset, downloadSize);
    return result;
}

bool DCLManagerImpl::IsRequestingEnabled() const
{
    DVASSERT(Thread::IsMainThread());
    return isProcessingEnabled;
}

void DCLManagerImpl::EnableRequesting()
{
    DVASSERT(Thread::IsMainThread());

    LockGuard<Mutex> lock(protectPM);

    if (!isProcessingEnabled)
    {
        isProcessingEnabled = true;
        if (requestManager)
        {
            requestManager->Start();
        }
    }
}

void DCLManagerImpl::DisableRequesting()
{
    DVASSERT(Thread::IsMainThread());

    LockGuard<Mutex> lock(protectPM);

    if (isProcessingEnabled)
    {
        isProcessingEnabled = false;
        if (requestManager)
        {
            requestManager->Stop();
        }
    }
}

const String& DCLManagerImpl::FindPackName(const FilePath& relativePathInPack) const
{
    LockGuard<Mutex> lock(protectPM);
    const String& result = db->FindPack(relativePathInPack);
    return result;
}

uint32 DCLManagerImpl::GetPackIndex(const String& packName) const
{
    DVASSERT(Thread::IsMainThread());

    auto it = packsIndex.find(packName);
    if (it != end(packsIndex))
    {
        return it->second;
    }
    DAVA_THROW(DAVA::Exception, "can't find pack with name: " + packName);
}

IPackManager::Pack& DCLManagerImpl::GetPack(const String& packName)
{
    DVASSERT(Thread::IsMainThread());
    DVASSERT(IsInitialized());

    uint32 index = GetPackIndex(packName);
    return packs.at(index);
}

const IPackManager::Pack& DCLManagerImpl::FindPack(const String& packName) const
{
    DVASSERT(Thread::IsMainThread());
    DVASSERT(IsInitialized());

    uint32 index = GetPackIndex(packName);
    return packs.at(index);
}

const Vector<IPackManager::Pack>& DCLManagerImpl::GetPacks() const
{
    DVASSERT(Thread::IsMainThread());
    return packs;
}

const FilePath& DCLManagerImpl::GetLocalPacksDirectory() const
{
    DVASSERT(Thread::IsMainThread());
    return dirToDownloadedPacks;
}

const String& DCLManagerImpl::GetSuperPackUrl() const
{
    DVASSERT(Thread::IsMainThread());
    return urlToSuperPack;
}

} // end namespace DAVA
