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

#include "LightSwitch.h"
#include "AppEvent.h"
#include "BindingHandler.h"

#include <app/server/Server.h>
#include <app/util/binding-table.h>
#include <controller/InvokeInteraction.h>

using namespace chip;
using namespace chip::app;

void LightSwitch::Init()
{
    BindingHandler::Init();
}

void LightSwitch::InitiateActionSwitch(Action mAction)
{
    BindingHandler::BindingData * data = Platform::New<BindingHandler::BindingData>();
    data->clusterId                    = chip::app::Clusters::OnOff::Id;
    switch (mAction)
    {
    case Action::Toggle:
        data->commandId = chip::app::Clusters::OnOff::Commands::Toggle::Id;
        break;
    case Action::On:
        data->commandId = chip::app::Clusters::OnOff::Commands::On::Id;
        break;
    case Action::Off:
        data->commandId = chip::app::Clusters::OnOff::Commands::Off::Id;
        break;
    default:
        Platform::Delete(data);
        return;
    }
    DeviceLayer::PlatformMgr().ScheduleWork(BindingHandler::SwitchWorkerHandler, reinterpret_cast<intptr_t>(data));
}

void LightSwitch::DimmerChangeBrightness()
{
    static uint8_t brightness;
    BindingHandler::BindingData * data = Platform::New<BindingHandler::BindingData>();
    data->commandId                    = chip::app::Clusters::LevelControl::Commands::MoveToLevel::Id;
    data->clusterId                    = chip::app::Clusters::LevelControl::Id;
    data->value                        = brightness++;
    if (brightness == 255)
    {
        brightness = 0;
    }
    DeviceLayer::PlatformMgr().ScheduleWork(BindingHandler::SwitchWorkerHandler, reinterpret_cast<intptr_t>(data));
}
