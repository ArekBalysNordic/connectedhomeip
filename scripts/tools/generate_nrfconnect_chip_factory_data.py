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

import os
import sys
import json
import shutil
from random import randint
import argparse
import subprocess
import logging as log
from math import log as logarithm
from tkinter.font import names
from typing import Type
from intelhex import IntelHex
from dataclasses import dataclass
try:
    import cbor2 as cbor
except ImportError:
    import pip
    pip.main(['install', 'cbor2'])
    import cbor2 as cbor

OUT_DIR = os.path.dirname(os.path.realpath(__file__))
TOOLS = {}
MANDATORY_DATA = ["serial_number",
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
                  "spake2_verifier", ]


def make_dict(input_list: list):
    names = list()
    attrs = list()
    for entry in input_list:
        names.append(entry[0])
        attrs.append(entry[1])
    return dict(zip(names, attrs))


def check_tools_exists():
    TOOLS['spake2p'] = shutil.which('spake2p')
    if TOOLS['spake2p'] is None:
        log.error('spake2p not found, please build and add spake2p path to PATH environment variable')
        log.error('You can find spake2p in connectedhomeip/src/tools/spake2p directory')
        sys.exit(1)


def gen_spake2p_params(passcode):
    iter_count_max = 10000
    salt_len_max = 32

    cmd = [
        TOOLS['spake2p'], 'gen-verifier',
        '--iteration-count', str(iter_count_max),
        '--salt-len', str(salt_len_max),
        '--pin-code', str(passcode),
        '--out', '-',
    ]

    output = subprocess.check_output(cmd)
    output = output.decode('utf-8').splitlines()
    return dict(zip(output[0].split(','), output[1].split(',')))


def bytes_needed(n):
    if n == 0:
        return 1
    return int(logarithm(n, 256)) + 1


def convert_to_bytes(value: any):
    if type(value) == int:
        return value.to_bytes(bytes_needed(value), byteorder='big', signed=False)
    elif type(value) == str:
        return value.encode("utf-8")
    elif type(value) == bytes:
        return value
    else:
        return value


class PartitionCreator:
    def __init__(self, offset: int, length: int) -> None:
        self.__ih = IntelHex()
        self.__length = length
        self.__offset = offset
        self.__data_ready = False

    @staticmethod
    def encrypt(data: bytes):
        return data

    @staticmethod
    def decrypt(data: bytes):
        return data

    def create_hex(self, data: bytes):
        if len(data) > self.__length:
            log.error("generated CBOR file exceeds declared maximum partition size! {} > {}".format(len(data), self.__length))
        self.__ih.putsz(self.__offset, self.encrypt(data))
        self.__ih.write_hex_file(OUT_DIR + "/output.hex", True)
        self.__data_ready = True
        return True

    def create_bin(self):
        if not self.__data_ready:
            log.error("Please create hex file first!")
            return False
        self.__ih.tobinfile(OUT_DIR + "/output.bin")
        return True


class FactoryDataGenerator:
    def __init__(self, json_file_path: str, spake: bool) -> None:
        self.__json_file_path = json_file_path
        self.__rotating_device_unique_id = None
        self.__json_data = None
        self.__pincode = 0
        self.__factory_data = list()
        self.__generate_spake = spake

        self.__is_valid = self.__validate_json()
        if self.__is_valid:
            self.__process_json()
        else:
            sys.exit(1)

    def generate_cbor(self):
        cbor_data = cbor.dumps(make_dict(self.__factory_data))
        with open(OUT_DIR + "/output.cbor", "w+b") as cbor_output:
            cbor.dump(cbor.CBORTag(55799, cbor.loads(cbor_data)), cbor_output)
        return cbor_data

    @staticmethod
    def generate_json(cbor_file_path: str):
        if cbor_file_path:
            try:
                with open(cbor_file_path, 'rb') as fp:
                    with open(OUT_DIR + "/output.json", "w+") as json_from_cbor:
                        cbor_data = cbor.load(fp)
                        data_list = PartitionCreator.decrypt(cbor_data)
                        entries_names = list()
                        entries_values = list()
                        for entry in data_list:
                            entries_names.append(entry)
                            if entry == "dac_cert" or entry == "dac_key" or entry == "pai_cert" or entry == "cert_declaration":
                                with open(OUT_DIR + "/" + entry + ".der", "w+b") as new_der:
                                    new_der.write(data_list[entry])
                                    new_der.close()
                                entries_values.append(OUT_DIR + "/" + entry + ".der")
                            else:
                                entries_values.append(str(data_list[entry]))
                        json_data = json.dumps(dict(zip(entries_names, entries_values)))
                        json_from_cbor.write(json_data)

            except IOError:
                log.error("Wrong CBOR file: {}".format(cbor_file_path))

    def __validate_json(self):
        try:
            with open(self.__json_file_path, "r") as json_file:
                self.__json_data = json.loads(json_file.read())
                valid = True
                for entry in MANDATORY_DATA:
                    entry_found = False
                    log.debug("checking mandatory entry: {}".format(entry))
                    for json_entry in self.__json_data.keys():
                        if entry == json_entry:
                            entry_found = True
                            log.debug("OK")
                            break
                    if not entry_found:
                        if entry.find("spake2") != -1 and not self.__generate_spake:
                            log.error("Could not find spake2 entry: {}".format(entry))
                            log.error("Please run this script with argument --spake to generate spake2 parameters")
                            json_file.close()
                            return False
                        elif entry.find("spake2") != -1 and self.__generate_spake:
                            log.debug("Will be auto-created")
                        elif entry == "rotating_device_unique_id":
                            log.warning("Could not find {}, generating new 128-bit octet string...".format(entry))
                            log.warning("A new rotating device unique ID will be created")
                        else:
                            log.warning("Could not find entry: {}".format(entry))
                            json_file.close()
                            return False
        except IOError as e:
            log.error("Could not process json file: {}".format(self.__json_file_path))
            log.error(e)
            return False
        else:
            json_file.close()
            return True

    def __process_json(self):
        try:
            with open(self.__json_file_path, "r") as json_file:
                self.__json_data = json.loads(json_file.read())
                for json_entry in self.__json_data.keys():
                    log.debug("Processing {}...".format(json_entry))
                    value = self.__json_data[json_entry]

                    # save pincode to generate Spake2 data
                    if json_entry == "passcode":
                        self.__pincode = value

                    # recognize .der files and process them
                    if(type(self.__json_data[json_entry]) == str):
                        if self.__json_data[json_entry].find(".der") != -1:
                            value = self.__process_der(self.__json_data[json_entry])
                    self.__add_entry(json_entry, value)

                if self.__rotating_device_unique_id:
                    self.__add_entry("rotating_device_unique_id", self.__generate_rotating_device_uid)

                if self.__generate_spake:
                    self.__generate_spake2()

                json_file.close()
        except json.decoder.JSONDecodeError as e:
            log.error("Can not decode json file: {}".format(self.__json_file_path))
            log.error(e)
            json_file.close()
            sys.exit()
        else:
            json_file.close()

    def __add_entry(self, name: str, value: any):
        value = convert_to_bytes(value)
        self.__factory_data.append([name, value])

    def __generate_spake2(self):
        check_tools_exists()
        spake2_params = gen_spake2p_params(self.__pincode)
        self.__add_entry("spake2_iterations_counter", spake2_params["Iteration Count"])
        self.__add_entry("spake2_salt", spake2_params["Salt"])
        self.__add_entry("spake2_verifier", spake2_params["Verifier"])
        return True

    def __generate_rotating_device_uid(self):
        rdu = bytes()
        for i in range(16):
            rdu += randint(0, 255).to_bytes(1, byteorder="big")
        return rdu

    def __process_der(self, path: str):
        log.debug("Processing der file...")
        try:
            with open(path, 'rb') as f:
                return f.read()
        except IOError as e:
            raise e


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
    parser = argparse.ArgumentParser(description="NrfConnect Factory Data NVS generator tool")

    def allow_any_int(i): return int(i, 0)

    parser.add_argument("-j", "--json", type=str,
                        help="Path to Json file containing all mandatory and optional factory data")
    parser.add_argument("-c", "--cbor", type=str, help="Path to CBOR file to generate JSON")
    parser.add_argument("-p", "--partition", type=allow_any_int, help="partition offset to store factory data")
    parser.add_argument("-s", "--size", type=allow_any_int, help="factory data partition size")
    parser.add_argument("-v", "--verbose", action="store_true", help="Run script with debug logs")
    parser.add_argument("--spake", action="store_true", help="Auto generate SPAKE2 fields")
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='[%(asctime)s][%(levelname)s] %(message)s', level=log.DEBUG)
    else:
        log.basicConfig(format='[%(asctime)s] %(message)s', level=log.INFO)

    partition_creator = PartitionCreator(args.partition, args.size)

    if args.json:
        generator = FactoryDataGenerator(args.json, args.spake)
        cbor_data = generator.generate_cbor()
        if partition_creator.create_hex(cbor_data) and partition_creator.create_bin():
            print_flashing_help()
    elif args.cbor:
        FactoryDataGenerator.generate_json(args.cbor)


if __name__ == "__main__":
    main()
