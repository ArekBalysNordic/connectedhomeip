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
from wsgiref.validate import validator
import jsonschema
import shutil
from random import randint
import argparse
import subprocess
import logging as log

TOOLS = {}
HEX_PREFIX = "hex:"


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
    """ Generate spake2 params using external spake2p script"""
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
    """ Creates simple dictionary from given list of tuples ("key", "value") """

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
        if self.__args.user:
            try:
                self.__user_data = json.loads(self.__args.user)
            except json.decoder.JSONDecodeError as e:
                raise ValidatorError("Provided wrong user data, this is not a Json format! {}".format(e))
        if (self.__args.spake2_salt or self.__args.spake2_it) and not (self.__args.spake2_verifier or self.__args.passcode):
            raise ValidatorError(
                "Provided spake2 salt or spake2 iteration counter, but no verifier nor passcode to generate a new verifier")
        if not self.__args.spake2_verifier and not self.__args.passcode:
            raise ValidatorError("Can not find spake2 verifier, to generate a new one please enter passcode (--passcode)")
        if not self.__args.rd_uid:
            log.warning("Can not find rotating device UID in provided arguments list. A new one will be generated.")

    def generate_json(self):
        """
        This function generates JSON data, .json file and validate it

        To validate generated JSON data a scheme must be provided within script's arguments

        - In the first part, if the rotating device's unique id has been not provided 
            as an argument, it will be created.
        - If user provided passcode and spake2 verifier have been not provided 
            as an argument, it will be created using an external script
        - Passcode is not stored in JSON by default. To store it for debugging purposes, add --include_passcode argument.
        - Validating output JSON is not mandatory, but highly recommended.

        """
        # generate missing data if needed
        if not self.__args.rd_uid:
            rd_uid = self.__generate_rotating_device_uid()
        else:
            rd_uid = self.__args.rd_uid
        if not self.__args.spake2_verifier:
            spake_2_verifier = self.__generate_spake2_verifier()
        else:
            spake_2_verifier = self.__args.spake2_verifier

        try:
            with open(self.__args.output + "/output.json", "w+") as json_file:
                # serialize mandatory data
                self.__add_entry("sn", self.__args.sn)
                self.__add_entry("date", self.__args.date)
                self.__add_entry("hw_ver", self.__args.hw_ver)
                self.__add_entry("hw_ver_str", self.__args.hw_ver_str)
                self.__add_entry("rd_uid", rd_uid)
                self.__add_entry("dac_cert", self.__process_der(self.__args.dac_cert))
                self.__add_entry("dac_key", self.__process_der(self.__args.dac_key))
                self.__add_entry("pai_cert", self.__process_der(self.__args.pai_cert))
                self.__add_entry("cd", self.__process_der(self.__args.cd))
                if self.__args.include_passcode:
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
        """ Add single entry to list of tuples ("key", "value") """
        if value:
            log.debug("Adding entry '{}' with size {} and type {}".format(name, sys.getsizeof(value), type(value)))
            self.__factory_data.append((name, value))

    def __create_user_entry(self):
        """ Collect all user entries provided as Json-type object"""
        user_data = list()
        for entry in self.__user_data:
            user_data.append((entry, self.__user_data[entry]))
        return generate_dict(user_data)

    def __generate_spake2_verifier(self):
        """ If verifier has not been provided in arguments list it should be generated via external script """
        check_tools_exists()
        spake2_params = gen_spake2p_params(self.__args.passcode, self.__args.spake2_it, self.__args.spake2_salt)
        return spake2_params["Verifier"]

    def __generate_rotating_device_uid(self):
        """ If rotating device unique ID has not been provided it should be generated """
        rdu = bytes()
        for i in range(16):
            rdu += randint(0, 255).to_bytes(1, byteorder="big")
        log.info("\n\nThe new rotate device UID: {}\n".format(rdu.hex()))
        return rdu.hex()

    def __validate_output_json(self, output_json: str):
        """ 
        Validate output JSON data with provided .scheme file 
        This function will raise error if JSON does not match schema.

        """
        try:
            with open(self.__args.schema) as schema_file:
                log.info("Validating Json with schema...")
                schema = json.loads(schema_file.read())
                validator = jsonschema.Draft202012Validator(schema=schema)
                validator.validate(instance=json.loads(output_json))
        except IOError as e:
            log.error("provided Json schema file is wrong: {}".format(self.__args.schema))
            raise e
        else:
            log.info("Validate OK")

    def __process_der(self, path: str):
        log.debug("Processing der file...")
        try:
            with open(path, 'rb') as f:
                data = HEX_PREFIX + f.read().hex()
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
    parser.add_argument("--include_passcode", action="store_true",
                        help="passcode is used only for generating Spake2 Verifier to include it in factory data add this argument")
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
    optional_arguments.add_argument("--passcode", type=allow_any_int,
                                    help="Default PASE session passcode")
    optional_arguments.add_argument("--spake2_it", type=allow_any_int,
                                    help="Provide Spake2 Iteraction Counter. This is mandatory to generate Spake2 Verifier")
    optional_arguments.add_argument("--spake2_salt", type=str,
                                    help="Provide Spake2 Salt. This is mandatory to generate Spake2 Verifier")
    optional_arguments.add_argument("--spake2_verifier", type=str,
                                    help="Provide Spake2 Verifier without generating")
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
