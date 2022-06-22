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
#include <app/clusters/door-lock-server/door-lock-server.h>
#include <lib/core/CHIPError.h>

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/zephyr.h>

class LockManager
{
public:
    enum class State : uint8_t
    {
        kLockingInitiated = 0,
        kLockingCompleted,
        kUnlockingInitiated,
        kUnlockingCompleted,
    };

    using OperationSource     = chip::app::Clusters::DoorLock::DlOperationSource;
    using StateChangeCallback = void (*)(State, OperationSource);

    static constexpr auto sMaxUsers                  = 10;
    static constexpr auto sMaxCredentialSize         = 8;
    static constexpr auto sMinUserIndex              = 1;
    static constexpr auto sMaxCredentialsPerUser     = 10;
    static constexpr auto sMaxCredentials            = 50;
    static constexpr auto sMaxCredentialInfoDataSize = 20;

    bool Init(StateChangeCallback callback, uint16_t maxNumberOfUsers, uint8_t maxNumberOfCredentialsPerUser, State initialState);

    bool Lock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin, DlOperationError & err);
    bool Unlock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin, DlOperationError & err);
    void Lock(OperationSource source)
    {
        SetState(State::kLockingInitiated, source);
        mOperationSource = source;
    }
    void Unlock(OperationSource source)
    {
        SetState(State::kUnlockingInitiated, source);
        mOperationSource = source;
    }

    bool GetCredential(chip::EndpointId endpointId, uint16_t credentialIndex, DlCredentialType credentialType,
                       EmberAfPluginDoorLockCredentialInfo & credential) const;
    bool SetCredential(chip::EndpointId endpointId, uint16_t credentialIndex, chip::FabricIndex creator, chip::FabricIndex modifier,
                       DlCredentialStatus credentialStatus, DlCredentialType credentialType, const chip::ByteSpan & credentialData);

    bool GetUser(chip::EndpointId endpointId, uint16_t userIndex, EmberAfPluginDoorLockUserInfo & user) const;
    bool SetUser(chip::EndpointId endpointId, uint16_t userIndex, chip::FabricIndex creator, chip::FabricIndex modifier,
                 const chip::CharSpan & userName, uint32_t uniqueId, DlUserStatus userStatus, DlUserType usertype,
                 DlCredentialRule credentialRule, const DlCredential * credentials, size_t totalCredentials);

    void CompleteChangingState(State state, OperationSource source) { SetState(state, source); }
    State GetState() const { return mState; }

private:
    friend LockManager & LockMgr();

    OperationSource mOperationSource         = OperationSource::kButton;
    StateChangeCallback mStateChangeCallback = nullptr;

    State mState = State::kLockingCompleted;
    static LockManager sLock;

    EmberAfPluginDoorLockUserInfo mUsers[sMaxUsers];
    EmberAfPluginDoorLockCredentialInfo mCredentialInfos[sMaxCredentials];

    // Maximum amount of users and credentials can be set in Init method,
    // but it can not exceed sMaxUsers and sMaxCredentialsPerUser.
    uint8_t mCurrentMaxUsers               = 0;
    uint16_t mCurrentMaxCredentialsPerUser = 0;

    char mUserNames[ArraySize(mUsers)][DOOR_LOCK_MAX_USER_NAME_SIZE];
    uint8_t mCredentialData[sMaxCredentials][sMaxCredentialSize];
    chip::Platform::ScopedMemoryBuffer<DlCredential> mCredentials[sMaxCredentialsPerUser];

    bool CheckCredentials(chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin, DlOperationError & err);
    void SetState(State state, OperationSource source);
    void ReadConfigs();
};

inline LockManager & LockMgr()
{
    return LockManager::sLock;
}