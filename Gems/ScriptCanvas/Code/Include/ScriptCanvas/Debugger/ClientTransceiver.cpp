/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "ClientTransceiver.h"

#include "Messages/Request.h"
#include "Messages/Notify.h"

#include <AzFramework/Script/ScriptRemoteDebuggingConstants.h>

using namespace AzFramework;

namespace ScriptCanvas
{
    namespace Debugger
    {
        ClientTransceiver::ClientTransceiver()
        {
            ClientRequestsBus::Handler::BusConnect();
            ClientUIRequestBus::Handler::BusConnect();
            AZ::SystemTickBus::Handler::BusConnect();

            m_remoteToolsEndpointJoinedHandler = AzFramework::RemoteToolsEndpointStatusEvent::Handler(
                [this]([[maybe_unused]] AzFramework::RemoteToolsEndpointInfo info)
                {
                    OnRemoteToolsEndpointListChanged();
                });

            m_remoteToolsEndpointLeftHandler = AzFramework::RemoteToolsEndpointStatusEvent::Handler(
                [this]([[maybe_unused]] AzFramework::RemoteToolsEndpointInfo info)
                {
                    OnRemoteToolsEndpointListChanged();
                });

            if (auto* remoteToolsInterface = AzFramework::RemoteToolsInterface::Get())
            {
                remoteToolsInterface->RegisterRemoteToolsEndpointJoinedHandler(
                    AzFramework::ScriptCanvasToolsKey, m_remoteToolsEndpointJoinedHandler);
                remoteToolsInterface->RegisterRemoteToolsEndpointLeftHandler(AzFramework::ScriptCanvasToolsKey, m_remoteToolsEndpointLeftHandler);
            }

            OnRemoteToolsEndpointListChanged();

            m_addCache.m_logExecution = true;
            m_removeCache.m_logExecution = false;
        }

        ClientTransceiver::~ClientTransceiver()
        {
            AZ::SystemTickBus::Handler::BusDisconnect();
            ClientUIRequestBus::Handler::BusDisconnect();
            ClientRequestsBus::Handler::BusDisconnect();
        }

        void ClientTransceiver::AddBreakpoint(const Breakpoint& breakpoint)
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX sending AddBreakpoint Request %s", breakpoint.ToString().data());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::AddBreakpointRequest(breakpoint));
            }
        }

        void ClientTransceiver::AddVariableChangeBreakpoint(const VariableChangeBreakpoint&)
        {

        }

        void ClientTransceiver::Break()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX Sending Break Request %s", target.GetDisplayName());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::BreakRequest());
            }
        }

        void ClientTransceiver::OnRemoteToolsEndpointListChanged()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (!target.IsValid())
            {
                ConnectToFirstTargetIfNotConnected();
            }
        }

        void ClientTransceiver::ConnectToFirstTargetIfNotConnected()
        {
            auto* remoteToolsInterface = AzFramework::RemoteToolsInterface::Get();
            if (!remoteToolsInterface)
                return;

            if (remoteToolsInterface->GetDesiredEndpoint(AzFramework::ScriptCanvasToolsKey).IsValid())
                return; // If we are already connected to a target, we don't do anything

            AzFramework::RemoteToolsEndpointContainer targets;
            remoteToolsInterface->EnumTargetInfos(AzFramework::ScriptCanvasToolsKey, targets);
            for (const auto& [_, info] : targets)
            {
                if (!info.IsSelf() && info.IsOnline())
                {
                    SetNetworkTarget(info);
                    return;
                }
            }

            // No valid target found
            SetNetworkTarget(AzFramework::RemoteToolsEndpointInfo());
        }

        void ClientTransceiver::BreakpointAdded(const Breakpoint& breakpoint)
        {
            Lock lock(m_mutex);
            auto inactiveIter = m_breakpointsInactive.find(breakpoint);

            if (inactiveIter != m_breakpointsInactive.end())
            {
                m_breakpointsInactive.erase(inactiveIter);
            }

            if (m_breakpointsActive.find(breakpoint) == m_breakpointsActive.end())
            {
                m_breakpointsActive.insert(breakpoint);
                ServiceNotificationsBus::Broadcast(&ServiceNotifications::BreakPointAdded, breakpoint);
            }
        }

        void ClientTransceiver::ClearMessages()
        {
            Lock guard(m_msgMutex);
            m_msgQueue.clear();
        }

        void ClientTransceiver::Continue()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX Sending Continue Request %s", target.GetDisplayName());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::ContinueRequest());
            }
        }
        
        AzFramework::RemoteToolsEndpointContainer ClientTransceiver::EnumerateAvailableNetworkTargets() const
        {
            AzFramework::RemoteToolsEndpointContainer targets;
            if (AzFramework::IRemoteTools* remoteTools = RemoteToolsInterface::Get())
            {
                remoteTools->EnumTargetInfos(AzFramework::ScriptCanvasToolsKey, targets);
            }

            AzFramework::RemoteToolsEndpointContainer connectableTargets;

            for (auto& idAndInfo : targets)
            {
                const auto& targetInfo = idAndInfo.second;
                if (targetInfo.IsValid() && targetInfo.IsOnline())
                {
                    connectableTargets[idAndInfo.first] = idAndInfo.second;
                    SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("Debugger TRX can connect to %s", targetInfo.GetDisplayName());
                }
                else
                {
                    SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT(
                        "Debugger TRX can't connect to %s because: %s", targetInfo.GetDisplayName(), isConnectable.GetError().c_str());
                }
            }

            return connectableTargets;
        }

        void ClientTransceiver::SetNetworkTarget(AzFramework::RemoteToolsEndpointInfo target)
        {
            AzFramework::RemoteToolsEndpointInfo previousTarget = GetNetworkTarget();
            if (previousTarget.IsValid())
            {
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(previousTarget, Message::DisconnectRequest());
            }

            ClearMessages();
            if (target.IsValid())
            {
                RemoteToolsInterface::Get()->SetDesiredEndpoint(AzFramework::ScriptCanvasToolsKey, target.GetPersistentId());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::ConnectRequest(m_connectionState));
            }

            ClientUINotificationBus::Broadcast(&ClientUINotifications::OnCurrentTargetChanged);
        }

        AzFramework::RemoteToolsEndpointInfo ClientTransceiver::GetNetworkTarget() const
        {
            AzFramework::RemoteToolsEndpointInfo targetInfo;
            if (AzFramework::IRemoteTools* remoteTools = RemoteToolsInterface::Get())
            {
                targetInfo = remoteTools->GetDesiredEndpoint(AzFramework::ScriptCanvasToolsKey);
            }

            if (!targetInfo.GetPersistentId())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("Debugger TRX The user has not chosen a target to connect to.\n");
                return AzFramework::RemoteToolsEndpointInfo();
            }

            if (!targetInfo.IsValid() || !targetInfo.IsOnline())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("The target is currently not in a state that would allow debugging code (offline or not debuggable)");
                return AzFramework::RemoteToolsEndpointInfo();
            }

            return targetInfo;
        }
        
        void ClientTransceiver::GetAvailableScriptTargets()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX sending GetAvailableScriptTargets Request %s", target.GetDisplayName());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::GetAvailableScriptTargets());
            }
        }
        
        void ClientTransceiver::GetActiveEntities()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX sending GetActiveEntities Request %s", target.GetDisplayName());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::GetActiveEntitiesRequest());
            }
        }

        void ClientTransceiver::GetActiveGraphs()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX sending GetActiveGraphs Request %s", target.GetDisplayName());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::GetActiveGraphsRequest());
            }
        }
        
        void ClientTransceiver::GetVariableValue()
        {

        }

        void ClientTransceiver::OnReceivedMsg(AzFramework::RemoteToolsMessagePointer msg)
        {
            {
                Lock lock(m_msgMutex);
                if (msg)
                {
                    m_msgQueue.push_back(msg);
                }
                else
                {
                    AZ_Error("ScriptCanvas Debugger", msg, "We received a NULL message in the trx message queue!");
                }
            }

            ProcessMessages();
        }
        
        void ClientTransceiver::ProcessMessages()
        {
            AzFramework::RemoteToolsMessageQueue messages;
            
            while (true)
            {
                {
                    Lock lock(m_msgMutex);
                    
                    if (m_msgQueue.empty())
                    {
                        return;
                    }
                    
                    AZStd::swap(messages, m_msgQueue);
                }
                
                while (!messages.empty())
                {
                    AzFramework::RemoteToolsMessagePointer msg = *messages.begin();
                    messages.pop_front();

                    if (auto request = azrtti_cast<Message::Notification*>(msg.get()))
                    {
                        request->Visit(*this);
                    }
                }
            }
        }

        void ClientTransceiver::RemoveBreakpoint(const Breakpoint&)
        {

        }

        void ClientTransceiver::RemoveVariableChangeBreakpoint(const VariableChangeBreakpoint&)
        {

        }

        void ClientTransceiver::SetVariableValue()
        {

        }

        void ClientTransceiver::StepOver()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("TRX Sending StepOver Request %s", target.GetDisplayName());
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::StepOverRequest());
            }
        }

        void ClientTransceiver::Visit(Message::AvailableScriptTargetsResult& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received AvailableScriptTargetsResult!");
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::GetAvailableScriptTargetResult, notification.m_payload);
        }
        
        void ClientTransceiver::Visit(Message::ActiveEntitiesResult& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received ActiveEntitiesResult!");
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::GetActiveEntitiesResult, notification.m_payload);
        }
        
        void ClientTransceiver::Visit(Message::ActiveGraphsResult& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received ActiveGraphsResult!");
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::GetActiveGraphsResult, notification.m_payload);
        }

        void ClientTransceiver::Visit(Message::AnnotateNode& notification)
        {
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::AnnotateNode, notification.m_payload);
        }
        
        void ClientTransceiver::Visit(Message::BreakpointAdded& notification)
        {
            BreakpointAdded(notification.m_breakpoint);
        }
        
        void ClientTransceiver::Visit(Message::BreakpointHit& notification)
        {
            BreakpointAdded(notification.m_breakpoint);
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::BreakPointHit, notification.m_breakpoint);
        }

        void ClientTransceiver::Visit(Message::Connected& notification)
        {
            if (notification.m_target.m_info.IsIdentityEqualTo(GetNetworkTarget()))
            {
                SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("Neat. we're connected");
                ServiceNotificationsBus::Broadcast(&ServiceNotifications::Connected, notification.m_target);
            }
            else
            {
                AZ_Warning("ScriptCanvas Debugger", false, "Received connection notification, but targets did not match");
            }
        }

        void ClientTransceiver::Visit([[maybe_unused]] Message::Disconnected& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("Disconnect Notification Received");
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::Disconnected);
            ClearMessages();
        }

        void ClientTransceiver::Visit([[maybe_unused]] Message::Continued& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received continue notification!");
            Target connectedTarget;
            connectedTarget.m_info = GetNetworkTarget();

            ServiceNotificationsBus::Broadcast(&ServiceNotifications::Continued, connectedTarget);
        }

        void ClientTransceiver::Visit(Message::GraphActivated& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received GraphActivated! %s", notification.m_payload.ToString().data());
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::GraphActivated, notification.m_payload);
        }

        void ClientTransceiver::Visit(Message::GraphDeactivated& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received GraphDeactivated! %s", notification.m_payload.ToString().data());
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::GraphDeactivated, notification.m_payload);
        }

        void ClientTransceiver::Visit(Message::SignaledInput& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received input signal notification! %s", notification.m_signal.ToString().data());
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::SignaledInput, notification.m_signal);
        }

        void ClientTransceiver::Visit(Message::SignaledOutput& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received output signal notification! %s", notification.m_signal.ToString().data());
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::SignaledOutput, notification.m_signal);
        }

        void ClientTransceiver::Visit(Message::VariableChanged& notification)
        {
            SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT("received variable change notification! %s", notification.m_variableChange.ToString().data());
            ServiceNotificationsBus::Broadcast(&ServiceNotifications::VariableChanged, notification.m_variableChange);
        }

        void ClientTransceiver::OnSystemTick()
        {
            AzFramework::IRemoteTools* remoteTools = RemoteToolsInterface::Get();
            if (!remoteTools)
                return;

            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                if (!m_addCache.m_entities.empty())
                {
                    remoteTools->SendRemoteToolsMessage(target, Message::AddTargetsRequest(m_addCache));
                    m_connectionState.Merge(m_addCache);
                    m_addCache.Clear();
                }

                if (!m_removeCache.m_entities.empty())
                {
                    remoteTools->SendRemoteToolsMessage(target, Message::RemoveTargetsRequest(m_removeCache));
                    m_connectionState.Remove(m_removeCache);
                    m_removeCache.Clear();
                }
            }

            const AzFramework::ReceivedRemoteToolsMessages* messages = remoteTools->GetReceivedMessages(AzFramework::ScriptCanvasToolsKey);
            if (messages)
            {
                for (const AzFramework::RemoteToolsMessagePointer& msg : *messages)
                {
                    OnReceivedMsg(msg);
                }
                remoteTools->ClearReceivedMessagesForNextTick(AzFramework::ScriptCanvasToolsKey);
            }
        }

        void ClientTransceiver::StartLogging(ScriptTarget& initialTargets)
        {
            m_connectionState.m_logExecution = true;
            m_connectionState.Clear();
            m_connectionState.Merge(initialTargets);

            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::StartLoggingRequest(initialTargets));
            }
        }

        void ClientTransceiver::StopLogging()
        {
            AzFramework::RemoteToolsEndpointInfo target = GetNetworkTarget();
            if (target.IsValid())
            {
                RemoteToolsInterface::Get()->SendRemoteToolsMessage(target, Message::StopLoggingRequest());
            }

            m_connectionState.m_logExecution = false;
            m_connectionState.Clear();
        }

        void ClientTransceiver::AddEntityLoggingTarget(const AZ::EntityId& entityId, const ScriptCanvas::GraphIdentifier& graphIdentifier)
        {
            auto insertResult = m_addCache.m_entities.insert(entityId);
            auto addCacheIter = insertResult.first;

            addCacheIter->second.insert(graphIdentifier);

            auto removeCacheIter = m_removeCache.m_entities.find(entityId);

            if (removeCacheIter != m_removeCache.m_entities.end())
            {
                removeCacheIter->second.erase(graphIdentifier);
            }
        }

        void ClientTransceiver::RemoveEntityLoggingTarget(const AZ::EntityId& entityId, const ScriptCanvas::GraphIdentifier& graphIdentifier)
        {
            auto insertResult = m_removeCache.m_entities.insert(entityId);
            auto removeCacheIter = insertResult.first;

            removeCacheIter->second.insert(graphIdentifier);

            auto addCacheIter = m_addCache.m_entities.find(entityId);

            if (addCacheIter != m_addCache.m_entities.end())
            {
                addCacheIter->second.erase(graphIdentifier);
            }
        }

        void ClientTransceiver::AddGraphLoggingTarget(const AZ::Data::AssetId& assetId)
        {
            m_addCache.m_graphs.insert(assetId);
            m_removeCache.m_graphs.erase(assetId);
        }

        void ClientTransceiver::RemoveGraphLoggingTarget(const AZ::Data::AssetId& assetId)
        {
            m_addCache.m_graphs.erase(assetId);
            m_removeCache.m_graphs.insert(assetId);
        }
    }
}
