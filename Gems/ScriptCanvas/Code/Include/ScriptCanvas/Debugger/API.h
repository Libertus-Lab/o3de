/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/EntityId.h>
#include <ScriptCanvas/Core/Core.h>
#include <AzCore/EBus/EBus.h>
#include <AzFramework/Network/IRemoteTools.h>
#include <AzCore/Outcome/Outcome.h>

/**
 * Runtime systems that provide debug information are inherently slow. Debugging such debug systems themselves makes them even slower.
 * Keep Debug debugging entries confined here, and do not enable them in source control.
 */ 
#if defined(SCRIPT_CANVAS_DEBUG_DEBUGGER)

#define SCRIPT_CANVAS_DEBUGGER_TRACE_SERVER(message, ...) AZ_TracePrintf("ScriptCanvas Debugger Server", message, __VA_ARGS__);
#define SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT(message, ...) AZ_TracePrintf("ScriptCanvas Debugger Client", message, __VA_ARGS__)

#else

#define SCRIPT_CANVAS_DEBUGGER_TRACE_SERVER(message, ...)
#define SCRIPT_CANVAS_DEBUGGER_TRACE_CLIENT(message, ...)

#endif//defined(SCRIPT_CANVAS_DEBUG_DEBUGGER)


namespace AZ
{
    class ReflectContext;
}

namespace ScriptCanvas
{
    namespace Debugger
    {
        extern const AZ::u64 k_serviceNotificationsMsgSlotId;
        extern const AZ::u64 k_clientRequestsMsgSlotId;

        void ReflectArguments(AZ::ReflectContext* context);
        void ReflectNotifications(AZ::ReflectContext* context);
        void ReflectRequests(AZ::ReflectContext* context);
    }
}
