/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI.Reflect/Base.h>
#include <Atom/RHI/DeviceObject.h>
#include <Atom/RHI/RayTracingPipelineState.h>
#include <Atom/RHI/DeviceRayTracingShaderTable.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ::RHI
{
    class RayTracingBufferPools;
    class ShaderResourceGroup;

    //! Specifies the shader and any local root signature parameters that make up a record in the shader table
    struct RayTracingShaderTableRecord
    {
        explicit RayTracingShaderTableRecord(const Name& shaderExportName);

        //! name of the shader as described in the pipeline state
        AZ::Name m_shaderExportName;

        //! shader resource group for this shader record
        const RHI::ShaderResourceGroup* m_shaderResourceGroup = nullptr;

        static const uint32_t InvalidKey = static_cast<uint32_t>(-1);

        //! key that can be used to identify this record
        uint32_t m_key = InvalidKey;
    };
    using RayTracingShaderTableRecordList = AZStd::list<RayTracingShaderTableRecord>;

    //! RayTracingShaderTableDescriptor
    //!
    //! Describes a ray tracing shader table.
    class RayTracingShaderTableDescriptor
    {
    public:
        RayTracingShaderTableDescriptor() = default;
        ~RayTracingShaderTableDescriptor() = default;

        //! Returns the device-specific DeviceRayTracingShaderTableDescriptor for the given index
        AZStd::shared_ptr<DeviceRayTracingShaderTableDescriptor> GetDeviceRayTracingShaderTableDescriptor(int deviceIndex);

        void RemoveHitGroupRecords(uint32_t key);

        AZ::Name m_name;
        RHI::Ptr<RayTracingPipelineState> m_rayTracingPipelineState;
        //! limited to one record, but stored as a list to simplify processing
        RayTracingShaderTableRecordList m_rayGenerationRecord;
        RayTracingShaderTableRecordList m_missRecords;
        RayTracingShaderTableRecordList m_hitGroupRecords;
    };

    //! Shader Table
    //! Specifies the ray generation, miss, and hit shaders used during the ray tracing process
    class RayTracingShaderTable : public MultiDeviceObject
    {
    public:
        AZ_CLASS_ALLOCATOR(RayTracingShaderTable, AZ::SystemAllocator, 0);
        AZ_RTTI(RayTracingShaderTable, "{B448997B-A8E6-446E-A333-EFD92B486D9B}", MultiDeviceObject);
        AZ_RHI_MULTI_DEVICE_OBJECT_GETTER(RayTracingShaderTable);
        RayTracingShaderTable() = default;
        virtual ~RayTracingShaderTable() = default;

        //! Initialize all device-specific RayTracingShaderTables
        void Init(MultiDevice::DeviceMask deviceMask, const RayTracingBufferPools& rayTracingBufferPools);

        //! Queues this RayTracingShaderTable to be built by the FrameScheduler.
        //! Note that the descriptor must be heap allocated, preferably using make_shared.
        void Build(const AZStd::shared_ptr<RayTracingShaderTableDescriptor> descriptor);
    };
} // namespace AZ::RHI
