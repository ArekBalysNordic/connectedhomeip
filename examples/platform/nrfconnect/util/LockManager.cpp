
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

#include "LockManager.h"

LockManager LockManager::sLock;

bool LockManager::Init(StateChangeCallback callback, uint16_t maxNumberOfUsers, uint8_t maxNumberOfCredentialsPerUser,
                       State initialState)
{

    if (maxNumberOfCredentialsPerUser > sMaxCredentialsPerUser)
    {
        ChipLogError(Zcl, "LockManager: Init: max number of credentials per user to big.");
        return false;
    }

    if (maxNumberOfUsers > sMaxUsers)
    {
        ChipLogError(Zcl, "LockManager: Init: max number of users to big.");
        return false;
    }

    for (uint8_t i = 0; i < ArraySize(mUsers); i++)
    {
        if (!mCredentials[i].Alloc(maxNumberOfCredentialsPerUser))
        {
            ChipLogError(Zcl, "LockManager: Init: Failed to allocate credentials for user");
            return false;
        }
    }

    mCurrentMaxUsers              = maxNumberOfUsers;
    mCurrentMaxCredentialsPerUser = maxNumberOfCredentialsPerUser;
    mState                        = initialState;

    if (callback)
    {
        mStateChangeCallback = callback;
    }

    return true;
}

bool LockManager::Lock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin,
                       DlOperationError & err)
{
    if (!CheckCredentials(endpointId, pin, err))
    {
        return false;
    }

    SetState(State::kLockingInitiated, source);
    mOperationSource = source;

    return true;
}

bool LockManager::Unlock(OperationSource source, chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin,
                         DlOperationError & err)
{
    if (!CheckCredentials(endpointId, pin, err))
    {
        return false;
    }

    SetState(State::kUnlockingInitiated, source);
    mOperationSource = source;

    return true;
}

bool LockManager::CheckCredentials(chip::EndpointId endpointId, const Optional<chip::ByteSpan> & pin, DlOperationError & err)
{

    // check the RequirePINforRemoteOperation attribute
    bool requirePin = false;

    // chip::app::Clusters::DoorLock::Attributes::RequirePINforRemoteOperation::Get(endpointId, &requirePin);

    if (!requirePin)
    {
        ChipLogDetail(Zcl, "Lock Manager: Pin is not required.");
        return true;
    }
    else if (!pin.HasValue() && requirePin)
    {
        ChipLogError(Zcl, "Lock Manager:  PIN code is not specified but it is required.");
        return false;
    }

    // Check the PIN code
    for (uint8_t i = 0; i < mCurrentMaxCredentialsPerUser; i++)
    {
        if (mCredentialInfos[i].credentialType != DlCredentialType::kPin ||
            mCredentialInfos[i].status == DlCredentialStatus::kAvailable)
        {
            continue;
        }

        if (mCredentialInfos[i].credentialData.data_equal(pin.Value()))
        {
            ChipLogDetail(Zcl, "Lock Manager: specified PIN code was found in the database");
            return true;
        }
    }

    err = DlOperationError::kInvalidCredential;
    return false;
}

bool LockManager::GetUser(chip::EndpointId endpointId, uint16_t userIndex, EmberAfPluginDoorLockUserInfo & user) const
{
    // In the Matter userIndex starts from 1 so it must be adjusted
    uint16_t actualUserIndex = userIndex - 1;
    const auto & storedUser  = mUsers[actualUserIndex];

    ChipLogProgress(Zcl, "LockManager: GetUser [endpoint=%d,userIndex=%hu]", endpointId, actualUserIndex);

    user.userStatus = storedUser.userStatus;
    if (DlUserStatus::kAvailable == user.userStatus)
    {
        ChipLogDetail(Zcl, "LockManager: Found unoccupied user [endpoint=%d]", endpointId);
        return true;
    }

    user.userName       = chip::CharSpan(storedUser.userName.data(), storedUser.userName.size());
    user.credentials    = chip::Span<const DlCredential>(mCredentials[actualUserIndex].Get(), storedUser.credentials.size());
    user.userUniqueId   = storedUser.userUniqueId;
    user.userType       = storedUser.userType;
    user.credentialRule = storedUser.credentialRule;

    // Set creation source and modification source to the "Matter" one due to
    // there is no way to create credentials outside Matter so far.
    user.creationSource     = DlAssetSource::kMatterIM;
    user.modificationSource = DlAssetSource::kMatterIM;
    user.createdBy          = storedUser.createdBy;
    user.lastModifiedBy     = storedUser.lastModifiedBy;

    ChipLogDetail(Zcl,
                  "LockManager: Found occupied user [endpoint=%d,name=\"%.*s\",credentialsCount=%u,"
                  "uniqueId=%u,type=%u,credentialRule=%u, createdBy=%d,lastModifiedBy=%d]",
                  endpointId, static_cast<int>(user.userName.size()), user.userName.data(), user.credentials.size(),
                  user.userUniqueId, chip::to_underlying(user.userType), chip::to_underlying(user.credentialRule), user.createdBy,
                  user.lastModifiedBy);

    return true;
}

bool LockManager::SetUser(chip::EndpointId endpointId, uint16_t userIndex, chip::FabricIndex creator, chip::FabricIndex modifier,
                          const chip::CharSpan & userName, uint32_t uniqueId, DlUserStatus userStatus, DlUserType usertype,
                          DlCredentialRule credentialRule, const DlCredential * credentials, size_t totalCredentials)
{
    ChipLogProgress(Zcl,
                    "LockManager:SetUser "
                    "[endpoint=%d,userIndex=%d,creator=%d,modifier=%d,userName=%s,uniqueId=%d "
                    "userStatus=%u,userType=%u,credentialRule=%u,credentials=%p,totalCredentials=%u]",
                    endpointId, userIndex, creator, modifier, userName.data(), uniqueId, chip::to_underlying(userStatus),
                    chip::to_underlying(usertype), chip::to_underlying(credentialRule), credentials, totalCredentials);

    // In the Matter userIndex starts from 1 so it must be adjusted
    uint16_t actualUserIndex = userIndex - 1;
    auto & storedUser        = mUsers[actualUserIndex];

    if (userName.size() > DOOR_LOCK_MAX_USER_NAME_SIZE)
    {
        ChipLogError(Zcl, "LockManager: User name is too long [endpoint=%d,index=%d]", endpointId, actualUserIndex);
        return false;
    }

    if (totalCredentials > mCurrentMaxCredentialsPerUser)
    {
        ChipLogError(Zcl, "LockManager: Total number of credentials is too big [endpoint=%d,index=%d,totalCredentials=%u]",
                     endpointId, actualUserIndex, totalCredentials);
        return false;
    }

    chip::Platform::CopyString(mUserNames[actualUserIndex], userName);
    storedUser.userName       = chip::CharSpan(mUserNames[actualUserIndex], userName.size());
    storedUser.userUniqueId   = uniqueId;
    storedUser.userStatus     = userStatus;
    storedUser.userType       = usertype;
    storedUser.credentialRule = credentialRule;
    storedUser.lastModifiedBy = modifier;
    storedUser.createdBy      = creator;

    for (size_t i = 0; i < totalCredentials; ++i)
    {
        mCredentials[actualUserIndex][i]                 = credentials[i];
        mCredentials[actualUserIndex][i].CredentialType  = 1;
        mCredentials[actualUserIndex][i].CredentialIndex = i + 1;

        storedUser.credentials = chip::Span<const DlCredential>(mCredentials[actualUserIndex].Get(), totalCredentials);
    }

    ChipLogProgress(Zcl, "LockManager: Successfully set the user [mEndpointId=%d,index=%d]", endpointId, actualUserIndex);

    return true;
}

bool LockManager::GetCredential(chip::EndpointId endpointId, uint16_t credentialIndex, DlCredentialType credentialType,
                                EmberAfPluginDoorLockCredentialInfo & credential) const
{
    // In the Matter credentialIndex starts from 1 so it must be adjusted
    uint16_t actualCredentialIndex = credentialIndex - 1;

    ChipLogProgress(Zcl, "Lock Manager: GetCredentials [credentialType:%u] [credentialIndex:%d]",
                    chip::to_underlying(credentialType), actualCredentialIndex);

    const auto & storedCredential = mCredentialInfos[actualCredentialIndex];

    credential.status = storedCredential.status;
    if (DlCredentialStatus::kAvailable == credential.status)
    {
        ChipLogDetail(Zcl, "Lock Manager: Found unoccupied credential.");
        return false;
    }
    credential.credentialType = storedCredential.credentialType;
    credential.credentialData = storedCredential.credentialData;
    credential.createdBy      = storedCredential.createdBy;
    credential.lastModifiedBy = storedCredential.lastModifiedBy;

    // Set creation source and modification source to the "Matter" one due to
    // there is no way to create credentials outside Matter so far.
    credential.creationSource     = DlAssetSource::kMatterIM;
    credential.modificationSource = DlAssetSource::kMatterIM;
    ChipLogDetail(Zcl, "Lock Manager: Found credential: [type%u, size:%zu]", chip::to_underlying(credential.credentialType),
                  credential.credentialData.size());
    return true;
}

bool LockManager::SetCredential(chip::EndpointId endpointId, uint16_t credentialIndex, chip::FabricIndex creator,
                                chip::FabricIndex modifier, DlCredentialStatus credentialStatus, DlCredentialType credentialType,
                                const chip::ByteSpan & credentialData)
{
    // ChipLogProgress(Zcl,
    //                 "Lock Manager: SetCredentials [credentialStatus = % u, credentialType = % u, credentialDataSize = % u,
    //                 creator "
    //                 "= % d, modifier = % d]",
    //                 chip::to_underlying(credentialStatus), chip::to_underlying(credentialType), credentialData.size(), creator,
    //                 modifier);

    // In the Matter credentialIndex starts from 1 so it must be adjusted
    uint16_t actualCredentialIndex = credentialIndex - 1;
    auto & storedCredential        = mCredentialInfos[actualCredentialIndex];

    if (credentialData.size() > sMaxCredentialInfoDataSize)
    {
        ChipLogError(Zcl, "Lock Manager: SetCredential: credential data size is too big.");
        return false;
    }

    storedCredential.status         = credentialStatus;
    storedCredential.credentialType = credentialType;
    storedCredential.createdBy      = creator;
    storedCredential.lastModifiedBy = modifier;

    memcpy(mCredentialData[actualCredentialIndex], credentialData.data(), credentialData.size());
    storedCredential.credentialData = chip::ByteSpan{ mCredentialData[actualCredentialIndex], credentialData.size() };

    ChipLogProgress(Zcl, "Lock Manager: Successfully set the credential [credentialType=%u]", chip::to_underlying(credentialType));
    return true;
}

void LockManager::SetState(State state, OperationSource source)
{
    mState = state;

    if (mStateChangeCallback != nullptr)
    {
        mStateChangeCallback(state, source);
    }
}