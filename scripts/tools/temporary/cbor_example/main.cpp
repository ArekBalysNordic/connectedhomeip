/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "cbor_decode.h"
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>

using namespace std;

static const char * known_keys[] = { "serial_number",
                                     "manufacturing_date",
                                     "passcode",
                                     "discriminator",
                                     "hardware_version",
                                     "hardware_version_string",
                                     "dac_cert",
                                     "dac_key",
                                     "pai_cert",
                                     "cert_declaration",
                                     "rotating_device_unique_id",
                                     "spake2_iterations_counter",
                                     "spake2_salt",
                                     "spake2_verifier" };

int main(int argc, char * argv[])
{

    ifstream cborFile(argv[1], ios::binary | ios::out);
    bool res;

    if (!cborFile)
    {
        cout << "Can not read provided file" << endl;
        return -1;
    }
    cout << "Reading given file..." << endl;
    cborFile.seekg(0, cborFile.end);
    int cborFilelength = cborFile.tellg();
    cborFile.seekg(0, cborFile.beg);
    uint8_t * cborBuffer = new uint8_t[cborFilelength];
    cborFile.read(reinterpret_cast<char *>(cborBuffer), cborFilelength);

    cout << "CBOR encoding..." << endl;
    zcbor_state_t states[14];
    zcbor_new_state(states, ARRAY_SIZE(states), cborBuffer, cborFilelength, 1);
    res = zcbor_map_start_decode(states);

    cborFile.close();
    delete (cborBuffer);

    return 0;
}