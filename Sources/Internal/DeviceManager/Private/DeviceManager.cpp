#include "DeviceManager/DeviceManager.h"

#if defined(__DAVAENGINE_COREV2__)

#include "Debug/DVAssert.h"
#include "Engine/Engine.h"
#include "Engine/Private/EngineBackend.h"
#include "Engine/Private/Dispatcher/MainDispatcherEvent.h"
#include "Input/GamepadDevice.h"
#include "Input/KeyboardInputDevice.h"
#include "Input/MouseDevice.h"

#if defined(__DAVAENGINE_QT__)
#include "DeviceManager/Private/Qt/DeviceManagerImplQt.h"
#elif defined(__DAVAENGINE_WIN32__)
#include "DeviceManager/Private/Win32/DeviceManagerImplWin32.h"
#elif defined(__DAVAENGINE_WIN_UAP__)
#include "DeviceManager/Private/Win10/DeviceManagerImplWin10.h"
#elif defined(__DAVAENGINE_MACOS__)
#include "DeviceManager/Private/Mac/DeviceManagerImplMac.h"
#elif defined(__DAVAENGINE_IPHONE__)
#include "DeviceManager/Private/Ios/DeviceManagerImplIos.h"
#elif defined(__DAVAENGINE_ANDROID__)
#include "DeviceManager/Private/Android/DeviceManagerImplAndroid.h"
#else
#error "DeviceManager: unknown platform"
#endif

namespace DAVA
{
DeviceManager::DeviceManager(Private::EngineBackend* engineBackend)
    : impl(new Private::DeviceManagerImpl(this, engineBackend->GetDispatcher()))
{
    Private::EngineBackend::Instance()->InstallEventFilter(this, MakeFunction(this, &DeviceManager::HandleEvent));
}

DeviceManager::~DeviceManager() = default;

void DeviceManager::UpdateDisplayConfig()
{
    impl->UpdateDisplayConfig();
}

bool DeviceManager::HandleEvent(const Private::MainDispatcherEvent& e)
{
    using Private::MainDispatcherEvent;

    bool isHandled = true;
    switch (e.type)
    {
    case MainDispatcherEvent::DISPLAY_CONFIG_CHANGED:
        HandleDisplayConfigChanged(e);
        break;
    case MainDispatcherEvent::GAMEPAD_MOTION:
        HandleGamepadMotion(e);
        break;
    case MainDispatcherEvent::GAMEPAD_BUTTON_DOWN:
    case MainDispatcherEvent::GAMEPAD_BUTTON_UP:
        HandleGamepadButton(e);
        break;
    case MainDispatcherEvent::GAMEPAD_ADDED:
        HandleGamepadAdded(e);
        break;
    case MainDispatcherEvent::GAMEPAD_REMOVED:
        HandleGamepadRemoved(e);
        break;
    default:
        isHandled = false;
        break;
    }
    return isHandled;
}

void DeviceManager::HandleDisplayConfigChanged(const Private::MainDispatcherEvent& e)
{
    size_t count = e.displayConfigEvent.count;
    DisplayInfo* displayInfo = e.displayConfigEvent.displayInfo;

    displays.resize(count);
    std::move(displayInfo, displayInfo + count, begin(displays));

    delete[] displayInfo;

    displayConfigChanged.Emit();
}

void DeviceManager::HandleGamepadMotion(const Private::MainDispatcherEvent& e)
{
    if (gamepad != nullptr)
    {
        gamepad->HandleGamepadMotion(e);
    }
}

void DeviceManager::HandleGamepadButton(const Private::MainDispatcherEvent& e)
{
    if (gamepad != nullptr)
    {
        gamepad->HandleGamepadButton(e);
    }
}

void DeviceManager::HandleGamepadAdded(const Private::MainDispatcherEvent& e)
{
    if (gamepad == nullptr)
    {
        gamepad = new GamepadDevice(0);
        gamepad->HandleGamepadAdded(e);
    }
}

void DeviceManager::HandleGamepadRemoved(const Private::MainDispatcherEvent& e)
{
    if (gamepad != nullptr)
    {
        gamepad->HandleGamepadRemoved(e);
        delete gamepad;
        gamepad = nullptr;
    }
}

void DeviceManager::Update(float32 /*frameDelta*/)
{
    if (gamepad != nullptr)
    {
        gamepad->Update();
    }
}

void DeviceManager::OnEngineInited()
{
    Engine::Instance()->update.Connect(this, &DeviceManager::Update);

    keyboard = new KeyboardInputDevice(1);
    inputDevices.push_back(keyboard);

#if defined(__DAVAENGINE_WINDOWS__) || defined(__DAVAENGINE_MACOS__)
    mouse = new MouseDevice(2);
    inputDevices.push_back(mouse);
#endif
}

InputDevice* DeviceManager::GetInputDevice(uint32 id)
{
    for (InputDevice* device : inputDevices)
    {
        if (device->GetId() == id)
        {
            return device;
        }
    }
    return nullptr;
}

GamepadDevice* DeviceManager::GetGamepad()
{
    return gamepad;
}

KeyboardInputDevice* DeviceManager::GetKeyboard()
{
    return keyboard;
}

MouseDevice* DeviceManager::GetMouse()
{
    return mouse;
}

} // namespace DAVA

#endif // __DAVAENGINE_COREV2__
