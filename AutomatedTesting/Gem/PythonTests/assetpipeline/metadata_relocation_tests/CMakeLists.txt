#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

if(PAL_TRAIT_BUILD_TESTS_SUPPORTED AND PAL_TRAIT_BUILD_HOST_TOOLS)
    # Metadata Relocation Tests only currently contain one periodic test (see TestSuite.py).
    ly_add_pytest(
        NAME AutomatedTesting::MetadataRelocation.Periodic
        TEST_SUITE periodic
        PYTEST_MARKS "SUITE_periodic"
        TEST_SERIAL
        PATH ${CMAKE_CURRENT_LIST_DIR}/TestSuite.py
        RUNTIME_DEPENDENCIES
            Legacy::Editor
            AZ::AssetProcessor
            AutomatedTesting.Assets
        COMPONENT
            MetadataRelocation
    )
  
endif()