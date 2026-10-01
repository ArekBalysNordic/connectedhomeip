#
#    Copyright (c) 2020 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#
cmake_minimum_required(VERSION 3.20.0)

get_filename_component(CHIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/connectedhomeip REALPATH)
get_filename_component(NCS_MATTER_ROOT ${CHIP_ROOT}/third_party/nrfconnect/ncs-matter REALPATH)
get_filename_component(NRFCONNECT_COMMON ${CHIP_ROOT}/examples/platform/nrfconnect REALPATH)

include(${CHIP_ROOT}/config/nrfconnect/app/check-nrfconnect-version.cmake)

list(APPEND ZEPHYR_EXTRA_MODULES
    ${CHIP_ROOT}/config/nrfconnect/chip-module
    ${NCS_MATTER_ROOT}
)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})

project(chip-nrfconnect-lighting-example)

include(${CHIP_ROOT}/config/nrfconnect/app/check-sysbuild-use.cmake)
include(${CHIP_ROOT}/config/nrfconnect/app/enable-gnu-std.cmake)
include(${CHIP_ROOT}/config/nrfconnect/app/flashing.cmake)
include(${NCS_MATTER_ROOT}/cmake/sample.cmake)

# -Wmaybe-uninitialized has too many false positives, including on std::optional
# and chip::Optional.  Make it nonfatal.
#
# See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
target_compile_options(app PRIVATE -Werror -Wno-error=maybe-uninitialized)

ncs_get_zap_parent_dir(ZAP_PARENT_DIR)

target_include_directories(app PRIVATE
    src
    ${ZAP_PARENT_DIR}
)

target_sources(app PRIVATE
    src/app_task.cpp
    src/main.cpp
    src/zcl_callbacks.cpp
    ${CHIP_ROOT}/src/app/persistence/DeferredAttributePersistenceProvider.cpp
)

ncs_configure_data_model()

if(CONFIG_SHELL AND BOARD STREQUAL "nrf52840dongle_nrf52840")
    target_sources(app PRIVATE ${NRFCONNECT_COMMON}/util/DFUTrigger.cpp)
endif()
