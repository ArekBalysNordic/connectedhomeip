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
from attr import validate
from intelhex import IntelHex
from dataclasses import dataclass

from requests import JSONDecodeError
try:
    import cbor2 as cbor
except ImportError:
    import pip
    pip.main(['install', 'cbor2'])
    import cbor2 as cbor
try:
    from ordered_set import OrderedSet
except ImportError:
    import pip
    pip.main(['install', 'ordered-set'])
    import cbor2 as cbor
    from ordered_set import OrderedSet


OUT_DIR = os.path.dirname(os.path.realpath(__file__))
TOOLS = {}


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


class ValidatorError(Exception):
    def __init__(self, message="str"):
        self.message = message

    def __str__(self):
        return self.message


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
    def __init__(self, arguments) -> None:
        self.__args = arguments
        self.__factory_data = list()
        self.__json_data = None

        try:
            self.__validate_args()
        except ValidatorError as e:
            log.error(e)
            sys.exit(-1)

    def __validate_args(self):
        # validate mandatory
        if self.__args.output.find(".") != -1:
            raise ValidatorError("Wrong output, please use path to output directory!")
        if len(self.__args.date) != len("MM.DD.YYYY_GG:MM"):
            raise ValidatorError("Wrong date format, please use: MM.DD.YYYY_GG:MM")
        if not self.__args.hw_ver and not self.__args.hw_ver_str:
            raise ValidatorError(
                "Please provide at least one type of hardware version, use --hw_ver [int] or --hw_ver_str [string]")
        if self.__args.dac_cert.find(".der") == -1:
            raise ValidatorError("Please provide path to .der file format containing a DAC certificate")
        if self.__args.dac_key.find(".der") == -1:
            raise ValidatorError("Please provide path to .der file format containing a DAC Keys")
        if self.__args.pai_cert.find(".der") == -1:
            raise ValidatorError("Please provide path to .der file format containing a PAI certificate")
        if self.__args.cd.find(".der") == -1:
            raise ValidatorError("Please provide path to .der file format containing a Certificate Declaration")
        # validate optional
        if self.__args.user:
            try:
                json.loads(self.__args.user)
            except json.decoder.JSONDecodeError as e:
                raise ValidatorError("Provided wrong user data, this is not a Json format! {}".format(e))
        if self.__args.spake2_gen and (not self.__args.spake2_salt or not self.__args.spake2_it):
            raise ValidatorError("To generate spake2 verifier please enter spake2 salt and spake 2 interaction counter")
        if (self.__args.spake2_salt or self.__args.spake2_it) and not (self.__args.spake2_gen or self.__args.spake2_verifier):
            raise ValidatorError("Spake2 Interaction counter and salt was provided but not Verifier or generate found")
        if self.__args.spake2_gen and self.__args.spake2_verifier:
            raise ValidatorError(
                "Provided spake2 verifier and spake2 generate at the same time, please choose only one of them. (--spake2_gen or --spake2_verifier)")
        if self.__args.rd_uid and self.__args.rd_uid_gen:
            raise ValidatorError(
                "Can not both provide and generate a rotating device unique ID, please choose only one of them. (--rd_uid or --rd_uid_gen)")
        if (not self.__args.spake2_salt and self.__args.spake2_it) or (self.__args.spake2_salt and not self.__args.spake2_it):
            raise ValidatorError("Provided only on of spake2 input parameters (salt or interaction counter)")
        elif self.__args.spake2_verifier and self.__args.spake2_gen:
            raise ValidatorError("Can not both use verifier and generate it")
        if self.__args.spake2_gen and not self.__args.passcode:
            raise ValidatorError("Can not generate spake2 without passcode, please add passcode using --passcode [int value]")
        if self.__args.generate and (not self.__args.offset or not self.__args.size):
            raise ValidatorError("Enter partition offset (--offset) and partition size (--size) to generate a hex file")

    def generate_cbor(self):
        if self.__json_data:
            cbor_data = cbor.dumps(self.__json_data)
            with open(self.__args.output + "/output.cbor", "w+b") as cbor_output:
                cbor.dump(cbor.CBORTag(55799, cbor.loads(cbor_data)), cbor_output)
            return cbor_data

    def generate_json(self):
        try:
            with open(self.__args.output + "/output.json", "w+") as json_file:
                # serialize mandatory data
                self.__add_entry("sn", self.__args.sn)
                self.__add_entry("date", self.__args.date)
                hw_version = self.__args.hw_ver
                if self.__args.hw_ver_str:
                    hw_version = self.__args.hw_ver_str
                self.__add_entry("hw_ver", hw_version)
                self.__add_entry("dac_cert", self.__process_der(self.__args.dac_cert))
                self.__add_entry("dac_key", self.__process_der(self.__args.dac_key))
                self.__add_entry("pai_cert", self.__process_der(self.__args.pai_cert))
                self.__add_entry("cd", self.__process_der(self.__args.cd))
                # try to add optional data
                rd_uid = self.__args.rd_uid
                if self.__args.rd_uid_gen:
                    rd_uid = self.__generate_rotating_device_uid()
                spake_2_verifier = self.__args.spake2_verifier
                if self.__args.spake2_gen:
                    spake_2_verifier = self.__generate_spake2_verifier(
                        self.__args.passcode, self.__args.spake2_it, self.__args.spake2_salt)

                self.__add_entry("rd_uid", rd_uid)
                self.__add_entry("cd", self.__args.pincode)
                self.__add_entry("passcode", self.__args.passcode)
                self.__add_entry("spake2_it", self.__args.spake2_it)
                self.__add_entry("spake2_salt", self.__args.spake2_it)
                self.__add_entry("spake2_verifier", spake_2_verifier)
                self.__add_entry("pincode", self.__args.pincode)
                self.__add_entry("discriminator", self.__args.discriminator)

                factory_data_dict = self.__generate_dict()
                self.__json_data = json.dumps(factory_data_dict)
                json_file.write(self.__json_data)

        except IOError as e:
            log.error("Error with processing file: {}".format(self.__args.output))
            json_file.close()

    def __add_entry(self, name: str, value: any):
        if value:
            log.info("{} {}".format(name, type(value)))
            self.__factory_data.append((name, value))

    def __generate_spake2(self):
        check_tools_exists()
        spake2_params = gen_spake2p_params(self.__pincode)
        self.__add_entry("spake2_iterations_counter", spake2_params["Iteration Count"])
        self.__add_entry("spake2_salt", spake2_params["Salt"])
        self.__add_entry("spake2_verifier", spake2_params["Verifier"])
        return True

    def __generate_dict(self):
        factory_data_names = list()
        factory_data_values = OrderedSet()
        for entry in self.__factory_data:
            factory_data_names.append(entry[0])
            factory_data_values.add(entry[1])
        return dict(zip(factory_data_names, factory_data_values))

    def __generate_rotating_device_uid(self):
        rdu = bytes()
        for i in range(16):
            rdu += randint(0, 255).to_bytes(1, byteorder="big")
        return rdu

    def __process_der(self, path: str):
        log.debug("Processing der file...")
        try:
            with open(path, 'rb') as f:
                data = f.read()
                f.close()
                return str(data)
        except IOError as e:
            log.error(e)
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

    mandatory_arguments = parser.add_argument_group("Mandatory arguments", "These arguments must be provided to generate Json file")
    optional_arguments = parser.add_argument_group(
        "Optional arguments", "These arguments are optional and they depend on the user-purpose")

    parser.add_argument("-o", "--output", type=str, help="Output path to store .json file", required=True)
    parser.add_argument("-g", "--generate", action="store_true",
                        help="Genrate CBOR output file, Hex file and raw Bin data")
    parser.add_argument("--offset", type=allow_any_int, help="Provide partition offset")
    parser.add_argument("--size", type=allow_any_int, help="Provide maximum partition size")
    parser.add_argument("-v", "--verbose", action="store_true", help="Run this script with DEBUG logging level")
    # Json known-keys values
    # mandatory keys
    mandatory_arguments.add_argument("--sn", type=str, help="Provide serial number", required=True)
    mandatory_arguments.add_argument(
        "--date", type=str, help="Provide manufacturing date in format MM.DD.YYYY_GG:MM", required=True)
    mandatory_arguments.add_argument(
        "--hw_ver", type=allow_any_int, help="Provide hardware version in int format. Alternatively you can use --hw_ver_str to store string format of hardware version")
    mandatory_arguments.add_argument(
        "--hw_ver_str", type=str, help="Provide hardware version in string format. Alternatively you can use --hw_ver to store int format of hardware version")
    mandatory_arguments.add_argument(
        "--dac_cert", type=str, help="Provide the path to .der file containing DAC certificate", required=True)
    mandatory_arguments.add_argument("--dac_key", type=str, help="Provide the path to .der file containing DAC keys", required=True)
    mandatory_arguments.add_argument(
        "--pai_cert", type=str, help="Provide the path to .der file containing PAI certificate", required=True)
    parser.add_argument("--cd", type=str, help="Provide the path to .der file containing Certificate Declaration", required=True)
    # optional keys
    optional_arguments.add_argument("--pincode", type=allow_any_int, help="Provide BLE Pairing Pincode")
    optional_arguments.add_argument("--discriminator", type=hex, help="Provide BLE pairing discriminator")
    optional_arguments.add_argument("--rd_uid", type=str,
                                    help="Provide the rotating device unique ID. To generate the new rotate device unique ID use --rd_uid_gen")
    optional_arguments.add_argument("--rd_uid_gen", action="store_true", help="Generate and save the new Rotating Device Unique ID")
    optional_arguments.add_argument("--passcode", type=allow_any_int, help="Passcode to generate Spake2 verifier")
    optional_arguments.add_argument("--spake2_it", type=allow_any_int,
                                    help="Provide Spake2 Interaction Counter. This is mandatory to generate Spake2 Verifier")
    optional_arguments.add_argument("--spake2_salt", type=str,
                                    help="Provide Spake2 Salt. This is mandatory to generate Spake2 Verifier")
    optional_arguments.add_argument("--spake2_verifier", type=str, help="Provide Spake2 Verifier without generating")
    optional_arguments.add_argument("--spake2_gen", action="store_true",
                                    help="Generate and save new Spake2 Verifier according to given Iteraction Counter and Salt")
    optional_arguments.add_argument(
        "--user", type=str, help="Provide additional user-specific keys in Json format: {'name_1': 'value_1', 'name_2': 'value_2', ... 'name_n', 'value_n'}")
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='[%(asctime)s][%(levelname)s] %(message)s', level=log.DEBUG)
    else:
        log.basicConfig(format='[%(asctime)s] %(message)s', level=log.INFO)

    generator = FactoryDataGenerator(args)
    generator.generate_json()
    if args.generate:
        partition_creator = PartitionCreator(args.offset, args.size)
        cbor_data = generator.generate_cbor()
        if partition_creator.create_hex(cbor_data) and partition_creator.create_bin():
            print_flashing_help()


if __name__ == "__main__":
    main()
