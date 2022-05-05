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

import sys
import json
import jsonschema
import shutil
from random import randint
import argparse
import subprocess
import logging as log

TOOLS = {}


def check_tools_exists():
    """
    Checking that spake2p tool exist in PATH environment
    """
    TOOLS['spake2p'] = shutil.which('spake2p')
    if TOOLS['spake2p'] is None:
        log.error('spake2p not found, please build and add spake2p path to PATH environment variable')
        log.error('You can find spake2p in connectedhomeip/src/tools/spake2p directory')
        sys.exit(1)


def gen_spake2p_params(passcode, it, salt):
    cmd = [
        TOOLS['spake2p'], 'gen-verifier',
        '--iteration-count', str(it),
        '--salt-len', str(len(salt)),
        '--pin-code', str(passcode),
        '--out', '-',
    ]
    output = subprocess.check_output(cmd)
    output = output.decode('utf-8').splitlines()
    return dict(zip(output[0].split(','), output[1].split(',')))


def generate_dict(input_list: list):
    output_dict = dict()
    for entry in input_list:
        output_dict[entry[0]] = entry[1]
    ret = output_dict
    return ret


class ValidatorError(Exception):
    """ Exception raised when input argument is wrong """

    def __init__(self, message="str"):
        self.message = message

    def __str__(self):
        return self.message


class FactoryDataGenerator:
    """
    Class to generate factory Data fromm given arguments and generate a Json file

    :param arguments: All input arguments parsed using ArgParse
    """

    def __init__(self, arguments) -> None:
        self.__args = arguments
        self.__factory_data = list()
        self.__user_data = dict()

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
                self.__user_data = json.loads(self.__args.user)
            except json.decoder.JSONDecodeError as e:
                raise ValidatorError("Provided wrong user data, this is not a Json format! {}".format(e))
        if self.__args.spake2_gen and (not self.__args.spake2_salt or not self.__args.spake2_it):
            raise ValidatorError("To generate spake2 verifier please enter spake2 salt and spake 2 Iteration counter")
        if (self.__args.spake2_salt or self.__args.spake2_it) and not (self.__args.spake2_gen or self.__args.spake2_verifier):
            raise ValidatorError("Spake2 Iteration counter and salt was provided but not Verifier or generate found")
        if self.__args.spake2_gen and self.__args.spake2_verifier:
            raise ValidatorError(
                "Provided spake2 verifier and spake2 generate at the same time, please choose only one of them. (--spake2_gen or --spake2_verifier)")
        if self.__args.rd_uid and self.__args.rd_uid_gen:
            raise ValidatorError(
                "Can not both provide and generate a rotating device unique ID, please choose only one of them. (--rd_uid or --rd_uid_gen)")
        if not self.__args.rd_uid and not self.__args.rd_uid_gen:
            raise ValidatorError(
                "Please Enter rotating device unique ID or request to generate it by adding --rd_uid_gen to arguments list"
            )
        if (not self.__args.spake2_salt and self.__args.spake2_it) or (self.__args.spake2_salt and not self.__args.spake2_it):
            raise ValidatorError("Provided only on of spake2 input parameters (salt or Iteration counter)")
        elif self.__args.spake2_verifier and self.__args.spake2_gen:
            raise ValidatorError("Can not both use verifier and generate it")
        if self.__args.spake2_gen and not self.__args.passcode:
            raise ValidatorError("Can not generate spake2 without passcode, please add passcode using --passcode [int value]")

    def generate_json(self):
        try:
            with open(self.__args.output + "/output.json", "w+") as json_file:
                # serialize mandatory data
                self.__add_entry("sn", self.__args.sn)
                self.__add_entry("date", self.__args.date)
                self.__add_entry("hw_ver", self.__args.hw_ver)
                self.__add_entry("hw_ver_str", self.__args.hw_ver_str)
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
                    spake_2_verifier = self.__generate_spake2_verifier()

                self.__add_entry("rd_uid", rd_uid)
                self.__add_entry("passcode", self.__args.passcode)
                self.__add_entry("spake2_it", self.__args.spake2_it)
                self.__add_entry("spake2_salt", self.__args.spake2_salt)
                self.__add_entry("spake2_verifier", spake_2_verifier)
                self.__add_entry("discriminator", self.__args.discriminator)

                factory_data_dict = generate_dict(self.__factory_data)
                # add user-specific data
                factory_data_dict["user"] = self.__create_user_entry()
                json_object = json.dumps(factory_data_dict)
                json_file.write(json_object)
                if self.__args.schema:
                    self.__validate_output_json(json_object)
                else:
                    log.warning("Json Schema file has not been provided, the output file can be wrong. Be aware of that.")
                json_file.close()
        except IOError as e:
            log.error("Error with processing file: {}".format(self.__args.output))
            json_file.close()

    def __add_entry(self, name: str, value: any):
        if value:
            log.debug("Adding entry '{}' with size {} and type {}".format(name, sys.getsizeof(value), type(value)))
            self.__factory_data.append((name, value))

    def __create_user_entry(self):
        user_data = list()
        for entry in self.__user_data:
            user_data.append((entry, self.__user_data[entry]))
        return generate_dict(user_data)

    def __generate_spake2_verifier(self):
        check_tools_exists()
        spake2_params = gen_spake2p_params(self.__args.passcode, self.__args.spake2_it, self.__args.spake2_salt)
        return spake2_params["Verifier"]

    def __generate_rotating_device_uid(self):
        rdu = bytes()
        for i in range(16):
            rdu += randint(0, 255).to_bytes(1, byteorder="big")
        return rdu.hex()

    def __validate_output_json(self, output_json: str):
        try:
            with open(self.__args.schema) as schema_file:
                schema = json.loads(schema_file.read())
                jsonschema.validate(instance=output_json, schema=schema)
        except IOError as e:
            log.error("provided Json schema file is wrong: {}".format(self.__args.schema))
            raise e

    def __process_der(self, path: str):
        log.debug("Processing der file...")
        try:
            with open(path, 'rb') as f:
                data = f.read().hex()
                f.close()
                return data
        except IOError as e:
            log.error(e)
            raise e


def main():
    parser = argparse.ArgumentParser(description="NrfConnect Factory Data NVS generator tool")

    def allow_any_int(i): return int(i, 0)

    mandatory_arguments = parser.add_argument_group("Mandatory arguments", "These arguments must be provided to generate Json file")
    optional_arguments = parser.add_argument_group(
        "Optional arguments", "These arguments are optional and they depend on the user-purpose")
    parser.add_argument("-s", "--schema", type=str,
                        help="Json schema file to validate Json output data")
    parser.add_argument("-o", "--output", type=str, required=True,
                        help="Output directory to store .json file")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Run this script with DEBUG logging level")
    # Json known-keys values
    # mandatory keys
    mandatory_arguments.add_argument("--sn", type=str, required=True,
                                     help="Provide serial number")
    mandatory_arguments.add_argument("--date", type=str, required=True,
                                     help="Provide manufacturing date in format MM.DD.YYYY_GG:MM")
    mandatory_arguments.add_argument("--hw_ver", type=allow_any_int, required=True,
                                     help="Provide hardware version in int format.")
    mandatory_arguments.add_argument("--hw_ver_str", type=str, required=True,
                                     help="Provide hardware version in string format.")
    mandatory_arguments.add_argument("--dac_cert", type=str, required=True,
                                     help="Provide the path to .der file containing DAC certificate")
    mandatory_arguments.add_argument("--dac_key", type=str, required=True,
                                     help="Provide the path to .der file containing DAC keys")
    mandatory_arguments.add_argument("--pai_cert", type=str, required=True,
                                     help="Provide the path to .der file containing PAI certificate")
    mandatory_arguments.add_argument("--cd", type=str, required=True,
                                     help="Provide the path to .der file containing Certificate Declaration")
    # optional keys
    optional_arguments.add_argument("--discriminator", type=hex,
                                    help="Provide BLE pairing discriminator")
    optional_arguments.add_argument("--rd_uid", type=str,
                                    help="Provide the rotating device unique ID. To generate the new rotate device unique ID use --rd_uid_gen")
    optional_arguments.add_argument("--rd_uid_gen", action="store_true",
                                    help="Generate and save the new Rotating Device Unique ID")
    optional_arguments.add_argument("--passcode", type=allow_any_int,
                                    help="Default PASE session passcode")
    optional_arguments.add_argument("--spake2_it", type=allow_any_int,

                                    help="Provide Spake2 Iteraction Counter. This is mandatory to generate Spake2 Verifier")
    optional_arguments.add_argument("--spake2_salt", type=str,
                                    help="Provide Spake2 Salt. This is mandatory to generate Spake2 Verifier")
    optional_arguments.add_argument("--spake2_verifier", type=str,
                                    help="Provide Spake2 Verifier without generating")
    optional_arguments.add_argument("--spake2_gen", action="store_true",
                                    help="Generate and save new Spake2 Verifier according to given Iteraction Counter and Salt")
    optional_arguments.add_argument("--user", type=str,
                                    help="Provide additional user-specific keys in Json format: {'name_1': 'value_1', 'name_2': 'value_2', ... 'name_n', 'value_n'}")
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='[%(asctime)s][%(levelname)s] %(message)s', level=log.DEBUG)
    else:
        log.basicConfig(format='[%(asctime)s] %(message)s', level=log.INFO)

    generator = FactoryDataGenerator(args)
    generator.generate_json()


if __name__ == "__main__":
    main()
