/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppTask.h"
#include "LEDWidget.h"
#include "LightSwitch.h"
#include "ThreadUtil.h"

#include <platform/CHIPDeviceLayer.h>

#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/SafeInt.h>

#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>

#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

#ifdef CONFIG_CHIP_OTA_REQUESTOR
#include <app/clusters/ota-requestor/BDXDownloader.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorStorage.h>
#include <app/clusters/ota-requestor/GenericOTARequestorDriver.h>
#include <app/clusters/ota-requestor/OTARequestor.h>
#include <platform/nrfconnect/OTAImageProcessorImpl.h>
#endif

#include <dk_buttons_and_leds.h>
#include <logging/log.h>
#include <system/SystemError.h>
#include <zephyr.h>

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

LOG_MODULE_DECLARE(app);
namespace {
constexpr EndpointId kLightSwitchEndpointId    = 1;
constexpr uint32_t kFactoryResetTriggerTimeout = 3000;
constexpr uint32_t kFactoryResetCancelWindow   = 3000;
constexpr uint32_t kDimmerTriggeredTimeout     = 500;
constexpr uint32_t kDimmerInterval             = 300;
constexpr size_t kAppEventQueueSize            = 10;

K_MSGQ_DEFINE(sAppEventQueue, sizeof(AppEvent), kAppEventQueueSize, alignof(AppEvent));

LightSwitch sLightSwitch;

LEDWidget sStatusLED;
LEDWidget sDiscoveryLED;
LEDWidget sBleLED;
LEDWidget sUnusedLED;

bool sIsThreadProvisioned    = false;
bool sIsThreadEnabled        = false;
bool sIsThreadBLEAdvertising = false;
bool sIsSMPAdvertising       = false;
bool sHaveBLEConnections     = false;
bool sIsDiscoveryEnabled     = false;
bool sWasDimmerTriggered     = false;

k_timer sFunctionTimer;
k_timer sDimmerPressKeyTimer;
k_timer sDimmerTimer;

#ifdef CONFIG_CHIP_OTA_REQUESTOR
DefaultOTARequestorStorage sRequestorStorage;
GenericOTARequestorDriver sOTARequestorDriver;
OTAImageProcessorImpl sOTAImageProcessor;
chip::BDXDownloader sBDXDownloader;
chip::OTARequestor sOTARequestor;
#endif

} /* namespace */

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::Init()
{
    // Initialize CHIP
    LOG_INF("Init CHIP stack");

    CHIP_ERROR err = chip::Platform::MemoryInit();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("Platform::MemoryInit() failed");
        return err;
    }

    err = PlatformMgr().InitChipStack();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("PlatformMgr().InitChipStack() failed");
        return err;
    }

    err = ThreadStackMgr().InitThreadStack();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("ThreadStackMgr().InitThreadStack() failed: %s", chip::ErrorStr(err));
        return err;
    }

#ifdef CONFIG_OPENTHREAD_MTD_SED
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice);
#elif CONFIG_OPENTHREAD_MTD
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice);
#else
    err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_FullEndDevice);
#endif
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("ConnectivityMgr().SetThreadDeviceType() failed: %s", chip::ErrorStr(err));
        return err;
    }

    sLightSwitch.Init();

    // Initialize UI components
    LEDWidget::InitGpio();
    LEDWidget::SetStateUpdateCallback(LEDStateUpdateHandler);
    sStatusLED.Init(DK_LED1);
    sBleLED.Init(DK_LED2);
    sDiscoveryLED.Init(DK_LED3);
    sUnusedLED.Init(DK_LED4);
    UpdateStatusLED();

    int ret = dk_buttons_init(ButtonEventHandler);

    if (ret)
    {
        LOG_ERR("dk_buttons_init() failed");
        return chip::System::MapErrorZephyr(ret);
    }

    // Initialize Timers
    k_timer_init(&sFunctionTimer, AppTask::TimerEventHandler, nullptr);
    k_timer_init(&sDimmerPressKeyTimer, AppTask::TimerEventHandler, nullptr);
    k_timer_init(&sDimmerTimer, AppTask::TimerEventHandler, nullptr);
    k_timer_user_data_set(&sDimmerTimer, this);
    k_timer_user_data_set(&sDimmerPressKeyTimer, this);
    k_timer_user_data_set(&sFunctionTimer, this);

    // Initialize DFU
#ifdef CONFIG_MCUMGR_SMP_BT
    GetDFUOverSMP().Init(RequestSMPAdvertisingStart);
    GetDFUOverSMP().ConfirmNewImage();
#endif

    // Print initial configs
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
    InitOTARequestor();
    ReturnErrorOnFailure(chip::Server::GetInstance().Init());
    ConfigurationMgr().LogDeviceConfig();
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    // Add CHIP aEvent handler and start CHIP thread.
    // Note that all the initialization code should happen prior to this point
    // to avoid data races between the main and the CHIP threads.
    PlatformMgr().AddEventHandler(ChipEventHandler, 0);

    err = PlatformMgr().StartEventLoopTask();
    if (err != CHIP_NO_ERROR)
    {
        LOG_ERR("PlatformMgr().StartEventLoopTask() failed");
        return err;
    }

    return err;
}

void AppTask::InitOTARequestor()
{
#ifdef CONFIG_CHIP_OTA_REQUESTOR
    sOTAImageProcessor.SetOTADownloader(&sBDXDownloader);
    sBDXDownloader.SetImageProcessorDelegate(&sOTAImageProcessor);
    sOTARequestorDriver.Init(&sOTARequestor, &sOTAImageProcessor);
    sRequestorStorage.Init(Server::GetInstance().GetPersistentStorage());
    sOTARequestor.Init(Server::GetInstance(), sRequestorStorage, sOTARequestorDriver, sBDXDownloader);
    chip::SetRequestorInstance(&sOTARequestor);
#endif
}

CHIP_ERROR AppTask::StartApp()
{
    ReturnErrorOnFailure(Init());

    AppEvent aEvent{};

    while (true)
    {
        k_msgq_get(&sAppEventQueue, &aEvent, K_FOREVER);
        DispatchEvent(aEvent);
    }

    return CHIP_NO_ERROR;
}

void AppTask::PostEvent(const AppEvent & aEvent)
{
    if (k_msgq_put(&sAppEventQueue, &aEvent, K_NO_WAIT))
    {
        LOG_INF("Failed to post aEvent to app task aEvent queue");
    }
}

void AppTask::DispatchEvent(const AppEvent & aEvent)
{
    switch (aEvent.Type)
    {
    case AppEvent::FunctionButtonPress:
        ButtonPressHandler(Button::Function);
        break;
    case AppEvent::FunctionButtonRelease:
        ButtonReleaseHandler(Button::Function);
        break;
    case AppEvent::DimmerButtonPress:
        ButtonPressHandler(Button::Dimmer);
        break;
    case AppEvent::DimmerButtonRelease:
        ButtonReleaseHandler(Button::Dimmer);
        break;
    case AppEvent::DiscoverButtonPress:
        ButtonPressHandler(Button::Discovery);
        break;
    case AppEvent::SwitchToggle:
        sLightSwitch.InitiateActionSwitch(LightSwitch::Action::Toggle);
        break;
    case AppEvent::SwitchOn:
        sLightSwitch.InitiateActionSwitch(LightSwitch::Action::On);
        break;
    case AppEvent::FunctionTimer:
        FunctionTimerEventHandler();
        break;
    case AppEvent::DimmerTimer:
        DimmerTimerEventHandler();
        break;
    case AppEvent::StartBLEAdvertising:
        StartBLEAdvertisingHandler();
        break;
    case AppEvent::UpdateLedState:
        aEvent.UpdateLedStateEvent.LedWidget->UpdateState();
        break;
    case AppEvent::DimmerChangeBrightness:
        sLightSwitch.DimmerChangeBrightness();
        break;
#ifdef CONFIG_MCUMGR_SMP_BT
    case AppEvent::StartSMPAdvertising:
        GetDFUOverSMP().StartBLEAdvertising();
        break;
#endif
    default:
        LOG_INF("Unknown aEvent received");
        break;
    }
}

void AppTask::ButtonPressHandler(Button aButton)
{
    switch (aButton)
    {
    case Button::Function:
        sAppTask.StartTimer(Timer::Function, kFactoryResetTriggerTimeout);
        sAppTask.mFunction = TimerFunction::SoftwareUpdate;
        break;
    case Button::Dimmer:
        LOG_INF("Press this button for at least 500 ms to change light sensitivity of binded lighting devices.");
        sAppTask.StartTimer(Timer::DimmerTrigger, kDimmerTriggeredTimeout);
        break;
    case Button::Discovery:
        UpdateStatusLED();
        break;
    default:
        break;
    }
}

void AppTask::ButtonReleaseHandler(Button aButton)
{
    switch (aButton)
    {
    case Button::Function:
        if (sAppTask.mFunction == TimerFunction::SoftwareUpdate)
        {
            sAppTask.CancelTimer(Timer::Function);
            sAppTask.mFunction = TimerFunction::NoneSelected;

#ifdef CONFIG_MCUMGR_SMP_BT
            GetDFUOverSMP().StartServer();
            sIsSMPAdvertising = true;
            UpdateStatusLED();
#else
            LOG_INF("Software update is disabled");
#endif
        }
        else if (sAppTask.mFunction == TimerFunction::FactoryReset)
        {
            UpdateStatusLED();

            sAppTask.CancelTimer(Timer::Function);
            sAppTask.mFunction = TimerFunction::NoneSelected;
            LOG_INF("Factory Reset has been canceled");
        }
        break;
    case Button::Dimmer:
        if (!sWasDimmerTriggered)
        {
            sAppTask.PostEvent(AppEvent{ AppEvent::SwitchToggle });
        }
        sAppTask.CancelTimer(Timer::Dimmer);
        sAppTask.CancelTimer(Timer::DimmerTrigger);
        sWasDimmerTriggered = false;
        break;
    default:
        break;
    }
}

void AppTask::FunctionTimerEventHandler()
{
    if (sAppTask.mFunction == TimerFunction::SoftwareUpdate)
    {
        LOG_INF("Factory Reset has been triggered. Release button within %u ms to cancel.", kFactoryResetCancelWindow);
        sAppTask.StartTimer(Timer::Function, kFactoryResetCancelWindow);
        sAppTask.mFunction = TimerFunction::FactoryReset;

        // reset all LEDs to synchronize factory reset blinking
        sStatusLED.Set(false);
        sDiscoveryLED.Set(false);
        sBleLED.Set(false);
        sUnusedLED.Set(false);

        sStatusLED.Blink(500);
        sDiscoveryLED.Blink(500);
        sBleLED.Blink(500);
        sUnusedLED.Blink(500);
    }
    else if (sAppTask.mFunction == TimerFunction::FactoryReset)
    {
        sAppTask.mFunction = TimerFunction::NoneSelected;
        LOG_INF("Factory Reset triggered");
        ConfigurationMgr().InitiateFactoryReset();
    }
}

void AppTask::DimmerTimerEventHandler()
{
    LOG_INF("Dimming started...");
    sWasDimmerTriggered = true;
    sAppTask.PostEvent(AppEvent{ AppEvent::SwitchOn });
    sAppTask.StartTimer(Timer::Dimmer, kDimmerInterval);
    sAppTask.CancelTimer(Timer::DimmerTrigger);
}

void AppTask::StartBLEAdvertisingHandler()
{
    /// Don't allow on starting Matter service BLE advertising after Thread provisioning.
    if (Server::GetInstance().GetFabricTable().FabricCount() != 0)
    {
        LOG_INF("NFC Tag emulation and Matter service BLE advertising not started - device is commissioned to a Thread network.");
        LOG_INF("Matter service BLE advertising not started - device is already commissioned");
        return;
    }

    if (ConnectivityMgr().IsBLEAdvertisingEnabled())
    {
        LOG_INF("BLE advertising is already enabled");
        return;
    }

    LOG_INF("Enabling BLE advertising...");
    if (chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow() != CHIP_NO_ERROR)
    {
        LOG_ERR("OpenBasicCommissioningWindow() failed");
    }
}

void AppTask::ChipEventHandler(const ChipDeviceEvent * aEvent, intptr_t /* arg */)
{
    switch (aEvent->Type)
    {
    case DeviceEventType::kCHIPoBLEAdvertisingChange:
        sIsThreadBLEAdvertising = true;
        UpdateStatusLED();
#ifdef CONFIG_CHIP_NFC_COMMISSIONING
        if (aEvent->CHIPoBLEAdvertisingChange.Result == kActivity_Started)
        {
            if (NFCMgr().IsTagEmulationStarted())
            {
                LOG_INF("NFC Tag emulation is already started");
            }
            else
            {
                ShareQRCodeOverNFC(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
            }
        }
        else if (aEvent->CHIPoBLEAdvertisingChange.Result == kActivity_Stopped)
        {
            NFCMgr().StopTagEmulation();
        }
#endif
        sHaveBLEConnections = ConnectivityMgr().NumBLEConnections() != 0;
        UpdateStatusLED();
        break;
    case DeviceEventType::kThreadStateChange:
        sIsThreadProvisioned = ConnectivityMgr().IsThreadProvisioned();
        sIsThreadEnabled     = ConnectivityMgr().IsThreadEnabled();
        UpdateStatusLED();
        break;
    default:
        if ((ConnectivityMgr().NumBLEConnections() == 0) && (!sIsThreadProvisioned || !sIsThreadEnabled))
        {
            LOG_ERR("Commissioning with a Thread network has not been done. An error occurred...");
            sIsThreadBLEAdvertising = false;
            sHaveBLEConnections     = false;
            UpdateStatusLED();
        }
        break;
    }
}

void AppTask::UpdateStatusLED()
{
    sUnusedLED.Set(false);

    // Status LED indicates:
    // - blinking 1 s - advertising, ready to commission
    // - blinking 200 ms - commissioning in progress
    // - constant lightning means commissioned with Thread network
    if (sIsThreadBLEAdvertising && !sHaveBLEConnections)
    {
        sStatusLED.Blink(50, 950);
    }
    else if (sIsThreadProvisioned && sIsThreadEnabled)
    {
        sStatusLED.Set(true);
    }
    else if (sHaveBLEConnections)
    {
        sStatusLED.Blink(30, 170);
    }
    else
    {
        sStatusLED.Set(false);
    }

    // Ble LED indicates BLE connectivity:
    //- blinking 200 ms means BLE advertising
    if (sIsSMPAdvertising)
    {
        sBleLED.Blink(30, 170);
    }
    else
    {
        sBleLED.Set(false);
    }

    // Binded LED indicates connection with light-bulb:
    // - constant lighting means at least one light bulb is connected
    // - blinking means looking for light bulb publishing
    if (sIsDiscoveryEnabled)
    {
        sDiscoveryLED.Blink(30, 170);
    }
    else
    {
        sDiscoveryLED.Set(false);
    }
}

void AppTask::ButtonEventHandler(uint32_t aButtonState, uint32_t aHasChanged)
{
    if (DK_BTN1_MSK & aButtonState & aHasChanged)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::FunctionButtonPress });
    }
    else if (DK_BTN1_MSK & aHasChanged)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::FunctionButtonRelease });
    }

    if (DK_BTN2_MSK & aButtonState & aHasChanged)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::DimmerButtonPress });
    }
    else if (DK_BTN2_MSK & aHasChanged)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::DimmerButtonRelease });
    }

    if (DK_BTN3_MSK & aButtonState & aHasChanged)
    {
        GetAppTask().PostEvent(AppEvent(AppEvent::DiscoverButtonPress));
    }

    if (DK_BTN4_MSK & aHasChanged & aButtonState)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::StartBLEAdvertising });
    }
}

void AppTask::StartTimer(Timer aTimer, uint32_t aTimeoutMs)
{
    switch (aTimer)
    {
    case Timer::Function:
        k_timer_start(&sFunctionTimer, K_MSEC(aTimeoutMs), K_NO_WAIT);
        break;
    case Timer::DimmerTrigger:
        k_timer_start(&sDimmerPressKeyTimer, K_MSEC(aTimeoutMs), K_NO_WAIT);
        break;
    case Timer::Dimmer:
        k_timer_start(&sDimmerTimer, K_MSEC(aTimeoutMs), K_MSEC(aTimeoutMs));
        break;
    default:
        break;
    }
}

void AppTask::CancelTimer(Timer aTimer)
{
    switch (aTimer)
    {
    case Timer::Function:
        k_timer_stop(&sFunctionTimer);
        break;
    case Timer::DimmerTrigger:
        k_timer_stop(&sDimmerPressKeyTimer);
        break;
    case Timer::Dimmer:
        k_timer_stop(&sDimmerTimer);
        break;
    default:
        break;
    }
}

void AppTask::LEDStateUpdateHandler(LEDWidget & aLedWidget)
{
    sAppTask.PostEvent(AppEvent{ AppEvent::UpdateLedState, &aLedWidget });
}

void AppTask::TimerEventHandler(k_timer * aTimer)
{
    if (aTimer == &sFunctionTimer)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::FunctionTimer });
    }
    if (aTimer == &sDimmerPressKeyTimer)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::DimmerTimer });
    }
    if (aTimer == &sDimmerTimer)
    {
        GetAppTask().PostEvent(AppEvent{ AppEvent::DimmerChangeBrightness });
    }
}

#ifdef CONFIG_MCUMGR_SMP_BT
void AppTask::RequestSMPAdvertisingStart(void)
{
    sAppTask.PostEvent(AppEvent{ AppEvent::StartSMPAdvertising });
}
#endif
