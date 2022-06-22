/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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

#include <lib/core/ClusterEnums.h>

#include "LockManager.h"
#include <zephyr/zephyr.h>

#include <cstdint>

class AppEvent;

class BoltLockManager
{
public:
    using OperationSource = LockManager::OperationSource;
    using State           = LockManager::State;

    static constexpr uint32_t kActuatorMovementTimeMs = 2000;

    CHIP_ERROR Init(LockManager::StateChangeCallback callback);

    State GetState() const { return LockMgr().GetState(); }
    bool IsLocked() const { return LockMgr().GetState() == LockManager::State::kLockingCompleted; }

    bool Lock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin, DlOperationError & err);
    bool Unlock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin, DlOperationError & err);
    void Lock(OperationSource source);
    void Unlock(OperationSource source);

private:
    static void ActuatorTimerEventHandler(k_timer * timer);
    static void ActuatorAppEventHandler(AppEvent * aEvent);
    friend BoltLockManager & BoltLockMgr();

    k_timer mActuatorTimer = {};

    static BoltLockManager sLock;
};

inline BoltLockManager & BoltLockMgr()
{
    return BoltLockManager::sLock;
}
