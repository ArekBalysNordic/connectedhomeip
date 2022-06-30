#
# Copyright (c) 2022 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

function(nrfconnect_generate_certs)
    find_program(CHIP_CERT "chip-cert")

    if(NOT CHIP_CERT)
        message(FATAL_ERROR "chip-cert executable was not found. Please compile it and add to PATH.")
    endif()

    # generate Certificate Declaration
    set(cd_args)
    string(APPEND cd_args "--key \"${CHIP_ROOT}/credentials/test/certification-declaration/Chip-Test-CD-Signing-Key.pem\"\n")
endfunction()
