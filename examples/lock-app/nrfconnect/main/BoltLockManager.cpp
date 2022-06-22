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

#include "BoltLockManager.h"

#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

BoltLockManager BoltLockManager::sLock;

static constexpr auto sDoorLockMaxUsers           = 10;
static constexpr auto sDoorLockCredentialsPerUser = 10;

void BoltLockManager::Init(StateChangeCallback callback)
{
    VerifyOrDie(LockMgr().Init(callback, sDoorLockMaxUsers, sDoorLockCredentialsPerUser, LockManager::State::kLockingCompleted));

    k_timer_init(&mActuatorTimer, &BoltLockManager::ActuatorTimerEventHandler, nullptr);
    k_timer_user_data_set(&mActuatorTimer, &LockMgr());
}

void BoltLockManager::Lock(OperationSource source)
{
    VerifyOrReturn(GetState() != LockManager::State::kLockingCompleted);

    LockMgr().Unlock(LockManager::State::kLockingInitiated);

    k_timer_start(&mActuatorTimer, K_MSEC(kActuatorMovementTimeMs), K_NO_WAIT);
}

void BoltLockManager::Unlock(OperationSource source)
{
    VerifyOrReturn(mState != State::kUnlockingCompleted);

    LockMgr().Unlock(source);

    k_timer_start(&mActuatorTimer, K_MSEC(kActuatorMovementTimeMs), K_NO_WAIT);
}

bool BoltLockManager::Lock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin,
                           DlOperationError & err)
{
    VerifyOrReturn(GetState() != LockManager::State::kLockingCompleted);

    bool ret = LockMgr().Lock(source, endpointId, pin, err);

    k_timer_start(&mActuatorTimer, K_MSEC(kActuatorMovementTimeMs), K_NO_WAIT);
    return ret;
}

bool BoltLockManager::Unlock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin,
                             DlOperationError & err)
{
    VerifyOrReturn(mState != State::kUnlockingCompleted);
    SetState(State::kUnlockingInitiated, source);

    bool ret = LockMgr().Unlock(source, endpointId, pin, err);

    k_timer_start(&mActuatorTimer, K_MSEC(kActuatorMovementTimeMs), K_NO_WAIT);
    return ret;
}

void BoltLockManager::ActuatorTimerEventHandler(k_timer * timer)
{
    // The timer event handler is called in the context of the system clock ISR.
    // Post an event to the application task queue to process the event in the
    // context of the application thread.

    AppEvent event;
    event.Type               = AppEvent::kEventType_Timer;
    event.TimerEvent.Context = static_cast<LockManager *>(k_timer_user_data_get(timer));
    event.Handler            = BoltLockManager::ActuatorAppEventHandler;
    GetAppTask().PostEvent(&event);
}

void BoltLockManager::ActuatorAppEventHandler(AppEvent * aEvent)
{
    LockManager * lock = static_cast<LockManager *>(aEvent->TimerEvent.Context);

    switch (lock->mState)
    {
    case State::kLockingInitiated:
        lock->CompleteChangingState(LockManager::State::kLockingCompleted, lock->mOperationSource);
        break;
    case State::kUnlockingInitiated:
        lock->CompleteChangingState(LockManager::State::kUnlockingCompleted, lock->mOperationSource);
        break;
    default:
        break;
    }
}
