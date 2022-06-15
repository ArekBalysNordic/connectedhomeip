# Configuring factory data for nRF Connect examples

Factory data is a set of device's parameters written to a non-volatile memory during the manufacturing process. All of them are immutable i.e. they are protected against modifications by the software. These parameters meant to be the same during the whole lifetime of a device and preserves factory resets. Factory data can be replaced only by reprogramming the entire device using a programmer.  

<p align="center">
  <img src="../../examples/platform/nrfconnect/doc/images/Logo_RGB_H-small.png" alt="Nordic Semiconductor logo"/>
  <img src="../../examples/platform/nrfconnect/doc/images/nRF52840-DK-small.png" alt="nRF52840 DK">
</p>

<hr>
This document is based on [Matter](https://github.com/project-chip/connectedhomeip), and Nordic
Semiconductor's nRF Connect SDK, and describes the process of creating and programming factory data.

-   [Overview](#overview)
    -   [Factory data components](#factory-data-components)
    -   [Factory data format](#factory-data-format)
-   [Using own factory data implementation](#using-own-factory-data-implementation)
-   [Generating factory data](#generating-factory-data)
    -   [Creating factory data JSON file](#creating-factory-data-json-file)
    -   [Verifying using a JSON schema](#verifying-using-a-json-schema)
    -   [Preparing factory data partition on a device](#preparing-factory-data-partition-on-a-device)
    -   [Generating factory data partition](#generating-factory-data-partition)
-   [Building an example with factory data](#building-an-example-with-factory-data)
    -   [Providing factory data parameters as a build argument list](#providing-factory-data-parameters-as-a-build-argument-list)
    - [Setting factory data parameters using interactive Kconfig interfaces](#setting-factory-data-parameters-using-interactive-kconfig-interfaces)
-   [Programming factory data](#programming-factory-data)

<hr>

<a name="overview"></a>

## Overview

The factory data parameter set includes information such as a manufacturer name, date of manufacturing, product certifications, and PCB versioning. 

For the nRF Connect platform, factory data is stored by default in the internal Flash memory in a separate partition. This helps to keep factory partition secure by applying hardware protection. Parameters are read at the boot time of a device, and then they can be used in the Matter stack and user application.

The factory data set described in [Factory data components](#factory-data-components) can be implemented in various ways as long as the final hex file will contain all mandatory components defined in the factory data components table. In this document, the [Generating factory data](#generating-factory-data) and the [Building an example with factory data](#building-an-example-with-factory-data) sections describe one of the implementations of the factory data set created by the nRF Connect platform's maintainers. As a result, a hex file is generated and it contains a factory data partition in the CBOR format. 

The default implementation of the Factory Data Accessor assumes that factory data stored in the device's Flash memory is provided in CBOR format. However, it is possible to generate factory data set without using nRF Connect scripts and implement another parser and Factory Data Accessor. This is possible if the newly provided implementation is consistent with the [Factor Data Provider](../../src/platform/nrfconnect/FactoryDataProvider.h). More information about preparing a factory data accessor was explained in [Using own factory data implementation](#using-own-factory-data-implementation) section.


> Note: Encryption and security of the factory data partition is not provided yet for this feature.

### Factory data components

A factory data set consists of following parameters:

##### A table represents all factory data components
|Key name|Full name|Length|Format|Conformance|Description|
|:------:|:-------:|:----:|:----:|:---------:|:---------:|
|**`sn`**|`serial number`|<1, 32> B|*ASCII string*|mandatory|A serial number parameter defines an unique number of manufactured device. Maximum length of serial number is 32 characters.|
|**`vendor_id`**|`vendor ID`|2 B|*uint16*|mandatory|A MATTER-assigned id for the organization responsible for producing the device.|
|**`product_id`**|`product ID`|2 B|*uint16*|mandatory|A unique id assigned by the device vendor to identify the product or device type. Defaults to a MATTER-assigned id designating a non-production or test "product".|
|**`vendor_name`**|`vendor name`|<1, 32> B|*ASCII string*|mandatory|A human-readable vendor name which provides a simple string containing identification of device's vendor for the application and Matter stack purposes.|
|**`product_name`**|`product name`|<1, 32> B|*ASCII string*|mandatory|A human-readable product name which provides a simple string containing identification of the product for the  the application and Matter stack purposes.|
|**`date`**|`manufacturing date`|<8, 10> B|*ISO 8601*|mandatory|A manufacturing date specifies the date that the device was manufactured. The format used for providing a manufacturing date is ISO 8601 e.g. YYYY-MM-DD.|
|**`hw_ver`**|`hardware version`|2 B|*uint16*|mandatory|A hardware version number specifies the version number of the hardware of the device. The meaning of its value, and the versioning scheme, are vendor defined.|
|**`hw_ver_str`**|`hardware version string`|<1, 64> B|*uint16*|mandatory|A hardware version string parameter specifies the version of the hardware of the device as a more user-friendly value than that presented by the hardware version integer value. The meaning of its value, and the versioning scheme, are vendor defined.|
|**`rd_uid`**|`rotating device ID unique ID`|<20, 36> B|*hex string*|mandatory|The unique ID for rotating device ID consists of a randomly-generated 128-bit or longer octet string. This parameter should be protected against reading or writing over the air after initial introduction into the device, and stay fixed during the lifetime of the device.|
|**`dac_cert`**|`(DAC) Device Attestation Certificate`|<1, 1204> B|*hex string*|mandatory|The Device Attestation Certificate (DAC) and corresponding private key, are unique to each Matter Device. The DAC is used for the Device Attestation process, and to perform commissioning into a Fabric. The DAC is a DER-encoded X.509v3-compliant certificate as defined in RFC 5280.|
|**`dac_key`**|`DAC private key`|68 B|*hex string*|mandatory|The Private key associated with the Device Attestation Certificate (DAC). This key should be encrypted, and maximum security should be guaranteed while generating and providing it to factory data.|
|**`pai_cert`**|`Product Attestation Intermediate`|<1, 1204> B|*hex string*|mandatory|An intermediate certificate is an X.509 certificate, which has been signed by the root certificate. The last intermediate certificate in a chain is used to sign the leaf (the Matter device) certificate.|
|**`spake2_it`**|`SPAKE2 Iteration Counter`|4 B|*uint32*|mandatory|The Spake2 iteration count is associated with the ephemeral PAKE passcode verifier to be used for the commissioning. The iteration count is used as a crypto parameter to process spake2 verifier.|
|**`spake2_salt`**|`SPAKE2 Salt`| <36, 68> B|*hex string*|mandatory|The spake2 salt is random data that is used as an additional input  to a one-way function that “hashes” data. A new salt should be randomly generated for each password.|
|**`spake2_verifier`**|`SPAKE2 Verifier`| 97 B|*hex string*|mandatory|The spake 2 verifier generated using default SPAKE2 salt, iteration count and passcode. This value can be used for development or testing purposes.|
|**`discriminator`**|`Discriminator`| 2 B|*uint16*|mandatory|A 12-bit value matching the field of the same name in the setup code. Discriminator is used during a discovery process.|
|**`passcode`**|`SPAKE passcode`|4 B|*uint32*|optional|A pairing passcode is a 27-bit unsigned integer which serves as a proof of possession during commissioning. Its value shall be restricted to the values 0x0000001 to 0x5F5E0FE (00000001 to 99999998 in decimal), excluding the invalid Passcode values: 00000000, 11111111, 22222222, 33333333, 44444444, 55555555, 66666666, 77777777, 88888888, 99999999, 12345678, 87654321|
|**`user`**|`User data`| variable|*JSON string*|max 1024 B|The user data is provided in JSON format. This parameter is optional and depends on user/manufacturer purpose. It is getting as a string from persistent storage and then should be parsed in a user application. This data is not used in the Matter stack.|

### Factory data format

The Factory data set is represented by JSON format that is regulated by [JSON Schema](https://github.com/project-chip/connectedhomeip/blob/master/scripts/tools/nrfconnect/nrfconnect_factory_data.schema) file. All parameters are divided between `mandatory `and `optional.` 
- `Mandatory` parameters must be provided as they are required e.g. to perform commissioning to the Matter network. 
- `Optional` parameters may be used for development and testing purposes. A `user` data parameter can consist of all data needed by a specific manufacturer that is not included in mandatory parameters.

In the factory data set, the following formats are used:

- `uint16` and `uint32` are the numeric formats representing respectively two-bytes length unsigned integer and four-bytes length unsigned integer.
- `hex string` meaning the bytes in hexadecimal representation given in a string. An prefix `hex:` is added to parameter to mark that is a `hex string`. E.g. an ASCII string *abba* is represented as *hex:61626261*, and its length is two times greater than ASCII string plus length of prefix. 
- `ASCII string` is a string representation in ASCII encoding without null-terminating.
- `ISO 8601` format is a [date format](https://www.iso.org/iso-8601-date-and-time-format.html) which represents a date given as YYYY-MM-DD or YYYYMMDD.
- All certificates stored in factory data are given in [X.509](https://www.itu.int/rec/T-REC-X.509-201910-I/en) format.

<hr>
<a name="Using own factory data"></a>

## Using own factory data implementation



<hr>
<a name="Generating factory data"></a>

## Generating factory data

### Creating factory data JSON file

### Verifying using a JSON schema

Ready JSON file containing factory data can be verified using [JSON Schema tool](https://github.com/project-chip/connectedhomeip/blob/master/scripts/tools/nrfconnect/nrfconnect_factory_data.schema). JSON Schema is a tool for validating the structure and content of JSON data. User should validate factory data JSON file.

To check JSON file using JSON Schema manually on Linux machine install *php-json-schema* package:
```
$ sudo apt install php-json-schema
```

And use following command:
```
$ validate-json <path_to_JSON_file> <path_to_schema_file>
```

The tool returns empty output in case of success.

JSON file can be checked automatically by the python script during its generation by given a path to JSON schema file as an additional argument:
```
$ python generate_nrfconnect_chip_factory_data.py --schema <path_to_schema>
```

> Note: To learn more about JSON scheme, check this [website](https://json-schema.org/understanding-json-schema/). 

### Preparing factory data partition on a device

The factory data partition is an area in a device's persistent storage where a factory data set is stored. This area is configured using the [Partition Manager](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/scripts/partition_manager/partition_manager.html), within which all partitions are declared in the `pm_static.yml` file. 

To prepare an example that support a factory data, a *pm_static* file has to contain a partition named `factory_data`, and its size should be equal to at least 4 kB and be multiple of one Flash page (for nRF52 and nRF53 boards, a single page is 4 kB size).

Se below example to learn how to create a factory data partition in `pm_static.yml` file according to `pm_static.yml` from [lock app example](../../examples/lock-app/nrfconnect/configuration/nrf52840dk_nrf52840/pm_static_dfu.yml) and nRF52840DK board:

```
...
mcuboot_primary_app:
    orig_span: &id002
        - app
    span: *id002
    address: 0x7200
    size: 0xf3e00

factory_data:
    address: 0xfb00
    size: 0x1000
    region: flash_primary

settings_storage:
    address: 0xfc000
    size: 0x4000
    region: flash_primary
...
```
In this example, a `factory_data` partition has been placed after the application partition(mcuboot_primary_app is a container to store application firmware which can be replaced in the Direct Firmware Update process), and its size has been set to 4 kB (0x1000) which corresponds to one page of nRF52840 flash memory.

Check a partition allocation using the `west` build system to ensure that the partition was appropriately created. To use the partition manager report tool, go to an example's directory and run the following command:

```
$ west build -t partition_manager_report
```

The output should be similar to:

```

  external_flash (0x800000 - 8192kB): 
+---------------------------------------------+
| 0x0: mcuboot_secondary (0xf4000 - 976kB)    |
| 0xf4000: external_flash (0x70c000 - 7216kB) |
+---------------------------------------------+

  flash_primary (0x100000 - 1024kB): 
+-------------------------------------------------+
| 0x0: mcuboot (0x7000 - 28kB)                    |
+---0x7000: mcuboot_primary (0xf4000 - 976kB)-----+
| 0x7000: mcuboot_pad (0x200 - 512B)              |
+---0x7200: mcuboot_primary_app (0xf3e00 - 975kB)-+
| 0x7200: app (0xf3e00 - 975kB)                   |
+-------------------------------------------------+
| 0xfb000: factory_data (0x1000 - 4kB)            |
| 0xfc000: settings_storage (0x4000 - 16kB)       |
+-------------------------------------------------+

  sram_primary (0x40000 - 256kB): 
+--------------------------------------------+
| 0x20000000: sram_primary (0x40000 - 256kB) |
+--------------------------------------------+

```

### Generating factory data partition

To store factory data set into device's persistent storage the data from JSON file should be converted to its binary representation in CBOR format. To do that in the simplest way, use this python [script](../../scripts/tools/nrfconnect/nrfconnect_generate_partition.py). 

To generate factory data partition using [python script](../../scripts/tools/nrfconnect/nrfconnect_generate_partition.py) use the following command pattern:

```
$ python nrfconnect_generate_partition.py -i <path_to_JSON_file> -o <path_to_output> --offset <partition_address_in_memory> --size <partition_size> 
```
In this command:

- <path_to_JSON_file> is a path to JSON file containing appropriate factory data.
- <path_to_output> is a path to an output file without any prefix. E.g. By providing here */build/output* as an argument, as a result */build/output.hex* and */build/output.bin* files will be created.
- <partition_address_in_memory> is an address in the device's persistent storage area where a partition data set should be stored.
- <partition_size> is a size of partition in the device's persistent storage area. This value will be checked according to JSON data to check if new data fits this size.

To see optional arguments for the script use the following command:
``` 
$ python nrfconnect_generate_partition.py -h
```

**Example of command for nRF52840DK**:
```
$ python nrfconnect_generate_partition.py -i build/zephyr/factory_data.json -o build/zephyr/factory_data --offset 0xfb000 --size 0x1000
```

As a result a *factory_data.hex* and *factory_data.bin* files will be created in */build/zephyr/* directory. The first contains memory offset; therefore, it can be programmed directly to the device using a programmer (E.g. nrfjprog).


<hr>
<a name="Building an example with factory data"></a>

## Building an example with factory data

Generating factory data set can be done manually using instructions described in [Generating factory data](#generating-factory-data) topic. But there is another way to generate factory data. Inside the nRF Connect build system, there is a script that generates factory data automatically using kConfigs and merges it to the firmware .hex file.

To enable generating factory data set automatically go to example's directory and build an example with the following option:

```
$ west build -b nrf52840dk_nrf52840 -- -DCONFIG_CHIP_FACTORY_DATA_BUILD=y
```
or add `CONFIG_CHIP_FACTORY_DATA_BUILD=y` config to a ***prj.conf*** file.

Each factory data parameter has default value and they are described in [Kconfig file](../../config/nrfconnect/chip-module/Kconfig). Setting a new value for factory data parameter can be done in two ways - by providing it as a build argument list or by using interactive Kconfig interfaces.

### Providing factory data parameters as a build argument list

This solution of providing factory data can be used with third-party build script because it uses only one command.
All parameters can be edited manually by providing them as an additional option for the west command e.g.:

```
$ west build -b nrf52840dk_nrf52840 -- --DCONFIG_CHIP_FACTORY_DATA_BUILD=y --DCONFIG_CHIP_DEVICE_DISCRIMINATOR=0xF11
```
or by adding them to a ***prj.conf*** file.

### Setting factory data parameters using interactive Kconfig interfaces

All configs can be edited also using interactive Kconfig interface.

See [Configuring nRF Connect examples](../guides/nrfconnect_examples_configuration.md) instruction to get know how to configure Kconfigs.

In configuration window expand an items named `Modules -> connectedhomeip (/home/arbl/matter/connectedhomeip/config/nrfconnect/chip-module) -> Connected Home over IP protocol stack`. After that there should be visible all factory data configs.

```
(65521) Device vendor ID
(32774) Device product ID
[*] Enable Factory Data build
[*]     Enable merging generated factory data with the build tar
[*]     Use default certificates located in Matter repository
[ ]     Enable spake2 verifier generation
[*]     Enable generation of a new Rotating device id unique id
(11223344556677889900) Serial number of device
(Nordic Semiconductor ASA) Human-readable vendor name
(not-specified) Human-readable product name
(2022-01-01) Manufacturing date in ISO 8601
(0) Integer representation of hardware version
(prerelease) user-friendly string representation of hardware ver
(0xF00) Device pairing discriminator
(20202021) Spake2+ passcode
(1000) Spake2+ iteration count
(U1BBS0UyUCBLZXkgU2FsdA==) Spake2+ salt in string format
(uWFwqugDNGiEck/po7KHwwMwwqZgN10XuyBajPGuyzUEV/iree4lOrao5GuwnlQ
(91a9c12a7c80700a31ddcfa7fce63e44) A rotating device id unique i
```

> Note: To get more information about how to use Interactive Kconfig interfaces go to [Kconfig docummentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/build/kconfig/menuconfig.html).

<hr>
<a name="Programming factory data"></a>

## Programming factory data

Ready HEX file containing factory data can be programmed into device's FLASH memory using *nrfjprog* and J-Link programmer. To do that use following command:

```
$ nrfjprog --program factory_data.hex
```

In this command a *--family* argument can be added and then provide name of board: **NRF52** for nrf52840DK or **NRF53** for nrf5340DK. E.g. 

```
$ nrfjprog --family NRF52 --program factory_data.hex
```

> Note: Go [here](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrf_cltools%2FUG%2Fcltools%2Fnrf_nrfjprogexe.html) to get more information about how to use nrfjprog utility.


Another way to program factory data to a device is to use the nrfconnect build system described in [Building an example with factory data](#building-an-example-with-factory-data), and build an example with option `--DCONFIG_CHIP_MERGE_FACTORY_DATA_WITH_FIRMWARE=y`. After that, use the following command from the example's directory to write firmware and newly generated factory data at the same time:
```
$ west flash
```
