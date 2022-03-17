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

#pragma once

#include "AppEvent.h"
#include "LEDWidget.h"

#include <core/CHIPError.h>

#ifdef CONFIG_MCUMGR_SMP_BT
#include "DFUOverSMP.h"
#endif

#include <cstdint>

struct k_timer;

class AppTask
{
public:
    CHIP_ERROR StartApp();
    void PostEvent(const AppEvent & event);
    void UpdateClusterState();

private:
    enum class Timer : uint8_t
    {
        Function,
        DimmerTrigger,
        Dimmer
    };
    enum class TimerFunction : uint8_t
    {
        NoneSelected = 0,
        SoftwareUpdate,
        FactoryReset
    };
    TimerFunction mFunction = TimerFunction::NoneSelected;

    enum class Button : uint8_t
    {
        Function,
        Dimmer,
    };

    friend AppTask & GetAppTask();
    static AppTask sAppTask;

    CHIP_ERROR Init();

    void DispatchEvent(const AppEvent & event);
    void InitOTARequestor();

    static void ButtonPressHandler(Button);
    static void ButtonReleaseHandler(Button);
    static void FunctionTimerEventHandler();
    static void DimmerTimerEventHandler();
    static void StartBLEAdvertisingHandler();

    static void ChipEventHandler(const chip::DeviceLayer::ChipDeviceEvent *, intptr_t);
    static void UpdateStatusLED();
    static void ButtonEventHandler(uint32_t, uint32_t);
    static void LEDStateUpdateHandler(LEDWidget &);

    static void StartTimer(Timer, uint32_t);
    static void CancelTimer(Timer);
    static void TimerEventHandler(k_timer *);

#ifdef CONFIG_MCUMGR_SMP_BT
    static void RequestSMPAdvertisingStart(void);
#endif
};

inline AppTask & GetAppTask()
{
    return AppTask::sAppTask;
}
