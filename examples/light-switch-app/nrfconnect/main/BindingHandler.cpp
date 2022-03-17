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

#include "BindingHandler.h"
#ifdef CONFIG_CHIP_LIB_SHELL
#include "ShellCommands.h"
#endif

#include <logging/log.h>
LOG_MODULE_DECLARE(app);

using namespace chip;
using namespace chip::app;

void BindingHandler::Init()
{
#ifdef CONFIG_CHIP_LIB_SHELL
    SwitchCommands::RegisterSwitchCommands();
#endif
    DeviceLayer::PlatformMgr().ScheduleWork(InitInternal);
}

void BindingHandler::OnOffProcessCommandUnicast(CommandId commandId, const EmberBindingTableEntry & binding, DeviceProxy * device,
                                                void * context)
{
    CHIP_ERROR ret = CHIP_NO_ERROR;

    auto onSuccess = [](const ConcreteCommandPath & commandPath, const StatusIB & status, const auto & dataResponse) {
        LOG_DBG("Binding command applied successfully!");
    };

    auto onFailure = [](CHIP_ERROR error) {
        LOG_INF("Binding command was not applied! Reason: %" CHIP_ERROR_FORMAT, error.Format());
    };

    switch (commandId)
    {
    case Clusters::OnOff::Commands::Toggle::Id:
        Clusters::OnOff::Commands::Toggle::Type toggleCommand;
        ret = Controller::InvokeCommandRequest(device->GetExchangeManager(), device->GetSecureSession().Value(), binding.remote,
                                               toggleCommand, onSuccess, onFailure);
        break;

    case Clusters::OnOff::Commands::On::Id:
        Clusters::OnOff::Commands::On::Type onCommand;
        ret = Controller::InvokeCommandRequest(device->GetExchangeManager(), device->GetSecureSession().Value(), binding.remote,
                                               onCommand, onSuccess, onFailure);
        break;

    case Clusters::OnOff::Commands::Off::Id:
        Clusters::OnOff::Commands::Off::Type offCommand;
        ret = Controller::InvokeCommandRequest(device->GetExchangeManager(), device->GetSecureSession().Value(), binding.remote,
                                               offCommand, onSuccess, onFailure);
        break;
    default:
        LOG_DBG("Invalid binding command data - commandId is not supported");
        break;
    }
    if (CHIP_NO_ERROR != ret)
    {
        LOG_INF("Invoke Unicast Command Request ERROR: %s", ErrorStr(ret));
    }
}

void BindingHandler::LevelControlProcessCommandUnicast(CommandId commandId, const EmberBindingTableEntry & binding,
                                                       DeviceProxy * device, void * context)
{
    auto onSuccess = [](const ConcreteCommandPath & commandPath, const StatusIB & status, const auto & dataResponse) {
        LOG_DBG("Binding command applied successfully!");
    };

    auto onFailure = [](CHIP_ERROR error) {
        LOG_INF("Binding command was not applied! Reason: %" CHIP_ERROR_FORMAT, error.Format());
    };

    CHIP_ERROR ret = CHIP_NO_ERROR;

    switch (commandId)
    {
    case Clusters::LevelControl::Commands::MoveToLevel::Id: {
        Clusters::LevelControl::Commands::MoveToLevel::Type moveToLevelCommand;
        BindingData * data       = reinterpret_cast<BindingData *>(context);
        moveToLevelCommand.level = data->value;
        ret = Controller::InvokeCommandRequest(device->GetExchangeManager(), device->GetSecureSession().Value(), binding.remote,
                                               moveToLevelCommand, onSuccess, onFailure);
    }
    break;
    default:
        LOG_DBG("Invalid binding command data - commandId is not supported");
        break;
    }
    if (CHIP_NO_ERROR != ret)
    {
        LOG_INF("Invoke Group Command Request ERROR: %s", ErrorStr(ret));
    }
}

void BindingHandler::LightSwitchChangedHandler(const EmberBindingTableEntry & binding, DeviceProxy * deviceProxy, void * context)
{
    VerifyOrReturn(context != nullptr, LOG_ERR("Invalid context for Light switch handler"););
    BindingData * data = static_cast<BindingData *>(context);

    if (binding.type == EMBER_UNICAST_BINDING)
    {
        switch (data->clusterId)
        {
        case Clusters::OnOff::Id:
            OnOffProcessCommandUnicast(data->commandId, binding, deviceProxy, context);
            break;
        case Clusters::LevelControl::Id:
            LevelControlProcessCommandUnicast(data->commandId, binding, deviceProxy, context);
            break;
        default:
            LOG_DBG("Invalid binding unicast command data");
            break;
        }
    }
}

void BindingHandler::BindingAddeddHandler(const EmberBindingTableEntry & binding)
{
    switch (binding.type)
    {
    case EMBER_UNICAST_BINDING:
        LOG_INF("Bound new unicast entry:\n \
                FabricId: % d\n \
                LocalEndpointId: % d\n \
                ClusterId: % d\n \
                RemoteEndpointId: % d\n \
                NodeId: %d ",
                (int) binding.fabricIndex, (int) binding.local, (int) binding.clusterId.Value(), (int) binding.remote,
                (int) binding.nodeId);
        break;
    case EMBER_MULTICAST_BINDING:

        LOG_INF("Bound new multicast entry\n \
                FabricId: % d\n \
                LocalEndpointId: %d \n \
                RemoteEndpointId: % d\n \
                GroupId: %d ",
                (int) binding.fabricIndex, (int) binding.local, (int) binding.remote, (int) binding.groupId);
        break;
    default:
        break;
    }
}

void BindingHandler::InitInternal(intptr_t arg)
{
    LOG_INF("Initialize binding Handler");
    auto & server = Server::GetInstance();
    if (CHIP_NO_ERROR !=
        BindingManager::GetInstance().Init(
            { &server.GetFabricTable(), server.GetCASESessionManager(), &server.GetPersistentStorage() }))
    {
        LOG_ERR("BindingHandler::InitInternal failed");
    }

    BindingManager::GetInstance().RegisterBoundDeviceChangedHandler(LightSwitchChangedHandler);
    if (CHIP_NO_ERROR != BindingManager::GetInstance().RegisterBindingAddedHandler(BindingAddeddHandler))
    {
        LOG_ERR("BindingHandler::RegisterBindingAddedHandler failed");
    }
    PrintBindingTable();
}

void BindingHandler::PrintBindingTable()
{
    BindingTable & bindingTable = BindingTable::GetInstance();

    LOG_INF("Binding Table [%d]:", bindingTable.Size());
    uint8_t i = 0;
    for (auto & entry : bindingTable)
    {
        switch (entry.type)
        {
        case EMBER_UNICAST_BINDING:
            LOG_INF("[%d] UNICAST:", i++);
            LOG_INF("\t\t+ Fabric: %d\n \
            \t+ LocalEndpoint %d \n \
            \t+ ClusterId %d \n \
            \t+ RemoteEndpointId %d \n \
            \t+ NodeId %d",
                    (int) entry.fabricIndex, (int) entry.local, (int) entry.clusterId.Value(), (int) entry.remote,
                    (int) entry.nodeId);
            break;
        case EMBER_MULTICAST_BINDING:
            LOG_INF("[%d] GROUP:", i++);
            LOG_INF("\t\t+ Fabric: %d\n \
            \t+ LocalEndpoint %d \n \
            \t+ RemoteEndpointId %d \n \
            \t+ GroupId %d",
                    (int) entry.fabricIndex, (int) entry.local, (int) entry.remote, (int) entry.groupId);
            break;
        case EMBER_UNUSED_BINDING:
            LOG_INF("[%d] UNUSED", i++);
            break;
        case EMBER_MANY_TO_ONE_BINDING:
            LOG_INF("[%d] MANY TO ONE", i++);
            break;
        default:
            break;
        }
    }
}

void BindingHandler::SwitchWorkerHandler(intptr_t context)
{
    VerifyOrReturn(context != 0, LOG_ERR("Invalid Swich data"));

    BindingData * data = reinterpret_cast<BindingData *>(context);
    LOG_INF("Notify Bounded Cluster | endpoint: %d cluster: %d", data->endpointId, data->clusterId);
    BindingManager::GetInstance().NotifyBoundClusterChanged(data->endpointId, data->clusterId, static_cast<void *>(data));

    Platform::Delete(data);
}
