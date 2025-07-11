/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/Fence.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RHI/RHISystemInterface.h>

#include <AzCore/Debug/Profiler.h>

namespace AZ::RHI
{
    bool Fence::ValidateIsInitialized() const
    {
        if (Validation::IsEnabled())
        {
            if (!IsInitialized())
            {
                AZ_Error("Fence", false, "Fence is not initialized!");
                return false;
            }
        }

        return true;
    }

    ResultCode Fence::Init(
        MultiDevice::DeviceMask deviceMask, FenceState initialState, bool usedForWaitingOnDevice, AZStd::optional<int> ownerDeviceIndex)
    {
        if (Validation::IsEnabled())
        {
            if (IsInitialized())
            {
                AZ_Error("Fence", false, "Fence is already initialized!");
                return ResultCode::InvalidOperation;
            }
        }

        MultiDeviceObject::Init(deviceMask);

        ResultCode resultCode = ResultCode::Success;

        auto flags = FenceFlags::None;
        if (usedForWaitingOnDevice)
        {
            flags |= FenceFlags::WaitOnDevice;
        }
        if (ownerDeviceIndex)
        {
            auto* device = RHISystemInterface::Get()->GetDevice(ownerDeviceIndex.value());
            m_deviceObjects[ownerDeviceIndex.value()] = Factory::Get().CreateFence();
            flags |= FenceFlags::CrossDevice;
            resultCode = GetDeviceFence(ownerDeviceIndex.value())->Init(*device, initialState, flags);
        }
        m_ownerDeviceIndex = ownerDeviceIndex;

        IterateDevices(
            [this, initialState, ownerDeviceIndex, flags, &resultCode](int deviceIndex)
            {
                auto* device = RHISystemInterface::Get()->GetDevice(deviceIndex);

                if (ownerDeviceIndex)
                {
                    if (deviceIndex != ownerDeviceIndex.value())
                    {
                        m_deviceObjects[deviceIndex] = Factory::Get().CreateFence();
                        resultCode = GetDeviceFence(deviceIndex)->InitCrossDevice(*device, GetDeviceFence(ownerDeviceIndex.value()));
                    }
                }
                else
                {
                    m_deviceObjects[deviceIndex] = Factory::Get().CreateFence();

                    resultCode = GetDeviceFence(deviceIndex)->Init(*device, initialState, flags);
                }

                return resultCode == ResultCode::Success;
            });

        if (resultCode != ResultCode::Success)
        {
            // Reset already initialized device-specific Fences and set deviceMask to 0
            m_deviceObjects.clear();
            MultiDeviceObject::Init(static_cast<MultiDevice::DeviceMask>(0u));
        }

        if (const auto& name = GetName(); !name.IsEmpty())
        {
            SetName(name);
        }

        return resultCode;
    }

    void Fence::Shutdown()
    {
        MultiDeviceObject::Shutdown();
    }

    ResultCode Fence::SignalOnCpu()
    {
        if (!ValidateIsInitialized())
        {
            return ResultCode::InvalidOperation;
        }

        return IterateObjects<DeviceFence>([]([[maybe_unused]] auto deviceIndex, auto deviceFence)
        {
            return deviceFence->SignalOnCpu();
        });
    }

    ResultCode Fence::Reset()
    {
        if (!ValidateIsInitialized())
        {
            return ResultCode::InvalidOperation;
        }

        if (m_ownerDeviceIndex)
        {
            // Only the owner device is reset here
            // All the other device fences share the state of the owner device and thus resetting them does not make sense
            return GetDeviceFence(m_ownerDeviceIndex.value())->Reset();
        }
        else
        {
            return IterateObjects<DeviceFence>(
                []([[maybe_unused]] auto deviceIndex, auto deviceFence)
                {
                    return deviceFence->Reset();
                });
        }
    }
} // namespace AZ::RHI
