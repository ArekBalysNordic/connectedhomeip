#
#    Copyright (c) 2022 Project CHIP Authors
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
cmake_minimum_required(VERSION 3.13.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

get_filename_component(CHIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/connectedhomeip REALPATH)
get_filename_component(NRFCONNECT_COMMON ${CHIP_ROOT}/examples/platform/nrfconnect REALPATH)

project(chip-nrfconnect_cbor_example)

add_executable(app main.cpp
                    zcbor/zcbor/src/cbor_decode.c
                    zcbor/zcbor/src/cbor_encode.c
                    )

target_include_directories(app PUBLIC . zcbor/zcbor/include)
