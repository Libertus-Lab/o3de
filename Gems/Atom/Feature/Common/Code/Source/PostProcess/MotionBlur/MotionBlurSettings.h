/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/RTTI/ReflectContext.h>

#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>

#include <Atom/Feature/PostProcess/MotionBlur/MotionBlurSettingsInterface.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <PostProcess/PostProcessBase.h>

#include <Atom/RPI.Public/View.h>

namespace AZ
{
    namespace Render
    {
        class PostProcessSettings;

        class MotionBlurSettings final
            : public MotionBlurSettingsInterface
            , public PostProcessBase
        {
            friend class PostProcessSettings;
            friend class PostProcessFeatureProcessor;

        public:
            AZ_RTTI(
                AZ::Render::MotionBlurSettings,
                "{262E7D7E-4B2D-4022-A67D-8EF4D07FB147}",
                AZ::Render::MotionBlurSettingsInterface,
                AZ::Render::PostProcessBase);
            AZ_CLASS_ALLOCATOR(MotionBlurSettings, SystemAllocator, 0);

            MotionBlurSettings(PostProcessFeatureProcessor* featureProcessor);
            ~MotionBlurSettings() = default;

            // BloomSettingsInterface overrides...
            void OnConfigChanged() override;

            // Applies settings from this onto target using override settings and passed alpha value for blending
            void ApplySettingsTo(MotionBlurSettings* target, float alpha) const;

            // Generate getters and setters.
#include <Atom/Feature/ParamMacros/StartParamFunctionsOverrideImpl.inl>
#include <Atom/Feature/PostProcess/MotionBlur/MotionBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

        private:
            // Generate members...
#include <Atom/Feature/ParamMacros/StartParamMembers.inl>
#include <Atom/Feature/PostProcess/MotionBlur/MotionBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>


            void Simulate(float deltaTime);

            PostProcessSettings* m_parentSettings = nullptr;

            float m_deltaTime = 0.0f;
        };

    } // namespace Render
} // namespace AZ
