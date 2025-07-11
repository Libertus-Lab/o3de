/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Atom/RHI/DeviceQuery.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI/RayTracingCompactionQueryPool.h>

namespace AZ::RHI
{
    ResultCode RayTracingCompactionQueryPool::Init(RayTracingCompactionQueryPoolDescriptor desc)
    {
        MultiDeviceObject::Init(desc.m_deviceMask);

        auto resultCode{ ResultCode::Success };
        IterateDevices(
            [&](int deviceIndex)
            {
                auto* device = RHISystemInterface::Get()->GetDevice(deviceIndex);
                m_deviceObjects[deviceIndex] = Factory::Get().CreateRayTracingCompactionQueryPool();
                resultCode = GetDeviceRayTracingCompactionQueryPool(deviceIndex)->Init(*device, desc);
                return resultCode == ResultCode::Success;
            });

        if (resultCode != ResultCode::Success)
        {
            // Reset already initialized device-specific objects and set deviceMask to 0
            m_deviceObjects.clear();
            MultiDeviceObject::Init(static_cast<MultiDevice::DeviceMask>(0u));
        }

        return resultCode;
    }

    ResultCode RayTracingCompactionQueryPool::InitQuery(MultiDevice::DeviceMask deviceMask, RayTracingCompactionQuery* query)
    {
        AZ_Assert(query, "Null query");
        ResultCode resultCode{ ResultCode::Success };
        IterateDevices(
            deviceMask,
            [&](auto deviceIndex) -> bool
            {
                resultCode = AddDeviceToQuery(deviceIndex, query);
                return resultCode == ResultCode::Success;
            });

        return resultCode;
    }

    ResultCode RayTracingCompactionQueryPool::AddDeviceToQuery(int deviceIndex, RayTracingCompactionQuery* query)
    {
        query->Init(SetBit(query->GetDeviceMask(), deviceIndex));
        const auto& deviceQueryPool = GetDeviceRayTracingCompactionQueryPool(deviceIndex);
        RHI::Ptr<DeviceRayTracingCompactionQuery> deviceQuery;
        deviceQuery = RHI::Factory::Get().CreateRayTracingCompactionQuery();
        auto resultCode = deviceQuery->Init(deviceQueryPool->GetDevice(), deviceQueryPool.get());
        if (resultCode != ResultCode::Success)
        {
            return resultCode;
        }

        query->m_deviceObjects[deviceIndex] = deviceQuery;
        query->m_deviceObjects[deviceIndex]->SetName(query->GetName());

        return resultCode;
    }

    void RayTracingCompactionQueryPool::RemoveDeviceFromQuery(int deviceIndex, RayTracingCompactionQuery* query)
    {
        query->Init(ResetBit(query->GetDeviceMask(), deviceIndex));
        query->m_deviceObjects.erase(deviceIndex);
    }

    void RayTracingCompactionQueryPool::BeginFrame(int frame)
    {
        IterateObjects<DeviceRayTracingCompactionQueryPool>(
            [&]([[maybe_unused]] auto deviceIndex, auto deviceQueryPool)
            {
                deviceQueryPool->BeginFrame(frame);
            });
    }
} // namespace AZ::RHI
