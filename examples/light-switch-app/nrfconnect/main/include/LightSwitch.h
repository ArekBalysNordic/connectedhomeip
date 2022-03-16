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

#include <lib/core/CHIPError.h>

class LightSwitch
{
public:
    enum class Action : uint8_t
    {
        Toggle, /// Switch state on lighting-app device
        On, /// Turn on light on lighting-app device
        Off /// Turn off light on lighting-app device 
    };

    /**
     * @brief Initialize Light switch manager
     * This method should Initialize binding handlers
     * 
     * @return CHIP_ERROR 
     */
    void Init();

    /**
     * @brief Initiate Action for binded lighting-app device after Switch interaction
     * Possible actions are: toggle, on, off 
     * 
     * 
     * @param mAction one of possible action: toggle, on, off
     */
    void InitiateActionSwitch(Action mAction);

    /**
     * @brief Change brightness of binded lightning-app device
     * 
     */
    void DimmerChangeBrightness();
};
