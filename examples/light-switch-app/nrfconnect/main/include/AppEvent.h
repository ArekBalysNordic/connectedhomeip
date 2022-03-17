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

#include <inet/IPAddress.h>

#include <cstdint>

#include "LEDWidget.h"

struct AppEvent
{
    using IPAddress = chip::Inet::IPAddress;

    enum SimpleEventType : uint8_t
    {
#ifdef CONFIG_MCUMGR_SMP_BT
        StartSMPAdvertising,
#endif
        StartBLEAdvertising,
        FunctionButtonPress,
        FunctionButtonRelease,
        DimmerButtonPress,
        DimmerButtonRelease,
        DimmerChangeBrightness,
        DimmerTimer,
        SwitchToggle,
        SwitchOn,
        FunctionTimer
    };

    enum UpdateLedStateEventType : uint8_t
    {
        UpdateLedState = FunctionTimer + 1
    };

    AppEvent() = default;
    explicit AppEvent(SimpleEventType type) : Type(type) {}
    AppEvent(UpdateLedStateEventType type, LEDWidget * ledWidget) : Type(type), UpdateLedStateEvent{ ledWidget } {}

    uint8_t Type;

    struct
    {
        LEDWidget * LedWidget;
    } UpdateLedStateEvent;
};
