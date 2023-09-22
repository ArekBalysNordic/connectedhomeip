/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include "AppTask.h"

#include <app/server/Server.h>
#include <app/util/attribute-storage.h>

namespace chip {

class AppFabricTableDelegate : public FabricTable::Delegate
{
public:
    ~AppFabricTableDelegate() { Server::GetInstance().GetFabricTable().RemoveFabricDelegate(this); }

    static void Init()
    {
        static AppFabricTableDelegate sAppFabricDelegate;
        Server::GetInstance().GetFabricTable().AddFabricDelegate(&sAppFabricDelegate);
    }

private:
    void OnFabricRemoved(const FabricTable & fabricTable, FabricIndex fabricIndex)
    {
        if (Server::GetInstance().GetFabricTable().FabricCount() == 0)
        {
#ifdef CONFIG_CHIP_LAST_FABRIC_REMOVED_ERASE_AND_REBOOT
            Server::GetInstance().ScheduleFactoryReset();
#elif defined(CONFIG_CHIP_LAST_FABRIC_REMOVED_ERASE_ONLY) || defined(CONFIG_CHIP_LAST_FABRIC_REMOVED_ERASE_AND_PAIRING_START)
            DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) {
                // Delete all fabrics and emit Leave event.
                Server::GetInstance().GetFabricTable().DeleteAllFabrics();
                /* Erase Matter data */
                DeviceLayer::PersistedStorage::KeyValueStoreMgrImpl().DoFactoryReset();
                /* Erase Thread credentials and disconnect */
                DeviceLayer::ConnectivityMgr().ErasePersistentInfo();
#ifdef CONFIG_CHIP_LAST_FABRIC_REMOVED_ERASE_AND_PAIRING_START
                /* Start the New BLE advertising */
                AppEvent event;
                event.Handler = AppTask::StartBLEAdvertisementHandler;
                AppTask::Instance().PostEvent(event);
#endif
            });
#endif
        }
    }
};
} // namespace chip
