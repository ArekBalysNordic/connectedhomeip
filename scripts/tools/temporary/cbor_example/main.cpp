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

extern "C" {
#include "cbor.h"
}
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>

using namespace std;

static uint8_t sReadCborRawData[4096];

static uint8_t pValidMapEncoded[] = {
    0xa3, 0x6d, 0x66, 0x69, 0x72, 0x73, 0x74, 0x20, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x72, 0x18, 0x2a, 0x77, 0x61,
    0x6e, 0x20, 0x61, 0x72, 0x72, 0x61, 0x79, 0x20, 0x6f, 0x66, 0x20, 0x74, 0x77, 0x6f, 0x20, 0x73, 0x74, 0x72, 0x69,
    0x6e, 0x67, 0x73, 0x82, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
    0x32, 0x6c, 0x6d, 0x61, 0x70, 0x20, 0x69, 0x6e, 0x20, 0x61, 0x20, 0x6d, 0x61, 0x70, 0xa4, 0x67, 0x62, 0x79, 0x74,
    0x65, 0x73, 0x20, 0x31, 0x44, 0x78, 0x78, 0x78, 0x78, 0x67, 0x62, 0x79, 0x74, 0x65, 0x73, 0x20, 0x32, 0x44, 0x79,
    0x79, 0x79, 0x79, 0x6b, 0x61, 0x6e, 0x6f, 0x74, 0x68, 0x65, 0x72, 0x20, 0x69, 0x6e, 0x74, 0x18, 0x62, 0x66, 0x74,
    0x65, 0x78, 0x74, 0x20, 0x32, 0x78, 0x1e, 0x6c, 0x69, 0x65, 0x73, 0x2c, 0x20, 0x64, 0x61, 0x6d, 0x6e, 0x20, 0x6c,
    0x69, 0x65, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x73, 0x74, 0x61, 0x74, 0x69, 0x73, 0x74, 0x69, 0x63, 0x73
};

int main(int argc, char * argv[])
{

    ifstream cborFile(argv[1], ios::binary | ios::out);

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
    CborParser parser;
    CborValue it;
    CborError err = cbor_parser_init(cborBuffer, cborFilelength, 0, &parser, &it);
    if (err != CborNoError)
    {
        cout << "CBOR error!" << err;
    }

    cout << "value type:" << int(it.type) << endl;
    CborTag result;
    cbor_value_get_tag(&it, &result);
    cout << "TAG:" << result << endl;
    cbor_value_skip_tag(&it);

    size_t mapLen = 0;
    cbor_value_get_map_length(&it, &mapLen);
    cout << "map len " << mapLen << endl;

    CborValue element;
    cbor_value_map_find_value(&it, "serial_number", &element);

    cout << int(element.type) << endl;
    size_t str_len = 0;
    cbor_value_get_string_length(&element, &str_len);
    cout << "strlen " << str_len << endl;

    cborFile.close();
    delete (cborBuffer);

    return 0;
}