#!/usr/bin/env python3
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

import codecs
import binascii
from intelhex import IntelHex
import argparse
import json
import logging as log
import sys
import cbor2 as cbor
from ordered_set import OrderedSet


class PartitionCreator:
    def __init__(self, offset: int, length: int, input: str, output: str) -> None:
        self.__ih = IntelHex()
        self.__length = length
        self.__offset = offset
        self.__data_ready = False
        self.__output = output
        self.__input = input

        self.__data_to_save = self.__convert_to_dict(self.__load_json())

    @staticmethod
    def encrypt(data: bytes):
        return data

    @staticmethod
    def decrypt(data: bytes):
        return data

    def generate_cbor(self):
        if self.__data_to_save:
            # prepare raw data from Json
            cbor_data = cbor.dumps(self.__data_to_save)
            with open(self.__output + "/output.cbor", "w+b") as cbor_output:
                cbor.dump(cbor.CBORTag(55799, cbor.loads(cbor_data)), cbor_output)
            return cbor_data

    def create_hex(self, data: bytes):
        if len(data) > self.__length:
            log.warning("generated CBOR file exceeds declared maximum partition size! {} > {}".format(len(data), self.__length))
        self.__ih.putsz(self.__offset, self.encrypt(data))
        self.__ih.write_hex_file(self.__output + "/output.hex", True)
        self.__data_ready = True
        return True

    def create_bin(self):
        if not self.__data_ready:
            log.error("Please create hex file first!")
            return False
        self.__ih.tobinfile(self.__output + "/output.bin")
        return True

    @staticmethod
    def __convert_to_dict(data):
        data_names = list()
        data_values = OrderedSet()
        for entry in data:
            log.debug("Processing entry {}".format(entry))
            data_names.append(entry)
            if isinstance(data[entry], str):
                try:
                    data_values.add(codecs.decode(data[entry], "hex"))
                    log.debug(codecs.decode(data[entry], "hex"))
                except binascii.Error:
                    data_values.add(data[entry].encode("utf-8"))

            else:
                data_values.add(data[entry])
        return dict(zip(data_names, data_values))

    def __load_json(self):
        try:
            with open(self.__input, "rb") as json_file:
                return json.loads(json_file.read())
        except IOError:
            log.error("Can not read Json file {}".format(self.__input))


def print_flashing_help():
    print("\nTo flash the generated hex containing factory data, run the following command:")
    print("For nrf52:")
    print("-------------------------------------------------------------------------------")
    print("nrfjprog -f nrf52 --program HEXFILE_PATH --sectorerase")
    print("-------------------------------------------------------------------------------")
    print("For nrf53:")
    print("-------------------------------------------------------------------------------")
    print("nrfjprog -f nrf53 --program HEXFILE_PATH --sectorerase")
    print("-------------------------------------------------------------------------------")


def main():

    def allow_any_int(i): return int(i, 0)

    parser = argparse.ArgumentParser(description="NrfConnect Factory Data NVS partition generator tool")
    parser.add_argument("-i", "--input", type=str, help="Path to input json file", required=True)
    parser.add_argument("-o", "--output", type=str, help="Output path to store .json file", required=True)
    parser.add_argument("--offset", type=allow_any_int, help="Provide partition offset", required=True)
    parser.add_argument("--size", type=allow_any_int, help="Provide maximum partition size", required=True)
    parser.add_argument("-v", "--verbose", action="store_true", help="Run this script with DEBUG logging level")
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='[%(asctime)s][%(levelname)s] %(message)s', level=log.DEBUG)
    else:
        log.basicConfig(format='[%(asctime)s] %(message)s', level=log.INFO)

    partition_creator = PartitionCreator(args.offset, args.size, args.input, args.output)
    cbor_data = partition_creator.generate_cbor()
    if partition_creator.create_hex(cbor_data) and partition_creator.create_bin():
        print_flashing_help()


if __name__ == "__main__":
    main()
