# Configuring factory data for nRF Connect examples

Factory data is a set of device parameters written to a non-volatile memory during the manufacturing process. All of these parameters are protected against modifications by the software. They are not meant to change, except in exceptional cases. In particular, they should be preserved at software updates and factory resets.

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
    -   [Verifying using a JSON Schema tool](#verifying-using-a-json-schema)
    -   [Preparing factory data partition on a device](#preparing-factory-data-partition-on-a-device)
    -   [Generating factory data partition](#generating-factory-data-partition)
-   [Building an example with factory data](#building-an-example-with-factory-data)
    -   [Providing factory data parameters as a build argument list](#providing-factory-data-parameters-as-a-build-argument-list)
    - [Setting factory data parameters using interactive Kconfig interfaces](#setting-factory-data-parameters-using-interactive-kconfig-interfaces)
-   [Programming factory data](#programming-factory-data)

<hr>

<a name="overview"></a>

## Overview

The factory data parameter set includes different types of information, for example about device certificates, cryptographic keys, device identifiers, and hardware.

For the nRF Connect platform, the factory data is stored by default in a separate partition of the internal flash memory. This helps to keep the factory data secure by applying hardware write protection. Parameters are read at the boot time of a device, and then they can be used in the Matter stack and in user application.

The factory data set described in [Factory data components](#factory-data-components) can be implemented in various ways as long as the final HEX file will contain all mandatory components defined in the factory data components table. In this document, the [Generating factory data](#generating-factory-data) and the [Building an example with factory data](#building-an-example-with-factory-data) sections describe one of the implementations of the factory data set created by the nRF Connect platform's maintainers. Following this process generates a HEX file, which contains the factory data partition in the CBOR format.

The default implementation of the Factory Data Accessor assumes that factory data stored in the device's flash memory is provided in the CBOR format.
However, it is possible to generate the factory data set without using the nRF Connect scripts and implement another parser and Factory Data Accessor.
This is possible if the newly provided implementation is consistent with the [Factor Data Provider](../../src/platform/nrfconnect/FactoryDataProvider.h).
For more information about preparing a factory data accessor, see the [Using own factory data implementation](#using-own-factory-data-implementation) section.

> Note: Encryption and security of the factory data partition is not provided yet for this feature.

### Factory data components

The following table lists the parameters of a factory data set:

|Key name|Full name|Length|Format|Conformance|Description|
|:------:|:-------:|:----:|:----:|:---------:|:---------:|
|``sn``|serial number|<1, 32> B|ASCII string|mandatory|A serial number parameter defines an unique number of manufactured device. Maximum length of the serial number is 32 characters.|
|``vendor_id``|vendor ID|2 B|uint16|mandatory|A CSA-assigned ID for the organization responsible for producing the device.|
|``product_id``|product ID|2 B|uint16|mandatory|A unique ID assigned by the device vendor to identify the product. Defaults to a CSA-assigned ID that designates a non-production or test product.|
|``vendor_name``|vendor name|<1, 32> B|ASCII string|mandatory|A human-readable vendor name that provides a simple string containing identification of device's vendor for the application and Matter stack purposes.|
|``product_name``|product name|<1, 32> B|ASCII string|mandatory|A human-readable product name that provides a simple string containing identification of the product for the application and the Matter stack purposes.|
|``date``|manufacturing date|<8, 10> B|ISO 8601|mandatory|A manufacturing date specifies the date that the device was manufactured. The date format used is ISO 8601, for example ``YYYY-MM-DD``.|
|``hw_ver``|hardware version|2 B|uint16|mandatory|A hardware version number that specifies the version number of the hardware of the device. The value meaning and the versioning scheme defined by the vendor.|
|``hw_ver_str``|hardware version string|<1, 64> B|uint16|mandatory|A hardware version string parameter that specifies the version of the hardware of the device as a more user-friendly value than that presented by the hardware version integer value. The value meaning and the versioning scheme defined by the vendor.|
|``rd_uid``|rotating device ID unique ID|<20, 36> B|byte string|mandatory|The unique ID for rotating device ID, which consists of a randomly-generated 128-bit (or longer) octet string. This parameter should be protected against reading or writing over-the-air after initial introduction into the device, and stay fixed during the lifetime of the device.|
|``dac_cert``|(DAC) Device Attestation Certificate|<1, 1204> B|byte string|mandatory|The Device Attestation Certificate (DAC) and the corresponding private key are unique to each Matter device. The DAC is used for the Device Attestation process and to perform commissioning into a fabric. The DAC is a DER-encoded X.509v3-compliant certificate, as defined in RFC 5280.|
|``dac_key``|DAC private key|68 B|byte string|mandatory|The private key associated with the Device Attestation Certificate (DAC). This key should be encrypted and maximum security should be guaranteed while generating and providing it to factory data.|
|``pai_cert``|Product Attestation Intermediate|<1, 1204> B|byte string|mandatory|An intermediate certificate is an X.509 certificate, which has been signed by the root certificate. The last intermediate certificate in a chain is used to sign the leaf (the Matter device) certificate. The PAI is a DER-encoded X.509v3-compliant certificate as defined in RFC 5280.||
|``spake2_it``|SPAKE2 iteration counter|4 B|uint32|mandatory|The SPAKE2 iteration counter is associated with the ephemeral PAKE passcode verifier to be used for the commissioning. The iteration counter is used as a crypto parameter to process the SPAKE2 verifier.|
|``spake2_salt``|SPAKE2 salt| <36, 68> B|byte string|mandatory|The SPAKE2 salt is random data that is used as an additional input to a one-way function that “hashes” data. A new salt should be randomly generated for each password.|
|``spake2_verifier``|SPAKE2 verifier| 97 B|byte string|mandatory|The SPAKE2 verifier generated using the default SPAKE2 salt, iteration counter, and passcode. This value can be used for development or testing purposes.|
|``discriminator``|Discriminator| 2 B|uint16|mandatory|A 12-bit value matching the field of the same name in the setup code. The discriminator is used during the discovery process.|
|``passcode``|SPAKE passcode|4 B|uint32|optional|A pairing passcode is a 27-bit unsigned integer which serves as a proof of possession during commissioning. Its value must be restricted to the values ``0x0000001`` to ``0x5F5E0FE`` (``00000001`` to ``99999998`` in decimal), excluding the following invalid passcode values: ``00000000``, ``11111111``, ``22222222``, ``33333333``, ``44444444``, ``55555555``, ``66666666``, ``77777777``, ``88888888``, ``99999999``, ``12345678``, ``87654321``.|
|``user``|User data| variable|JSON string|max 1024 B|The user data is provided in the JSON format. This parameter is optional and depends on user's or manufacturer's purpose (or both). It is provided as a string from persistent storage and then should be parsed in the user application. This data is not used by the Matter stack.|

### Factory data format

The factory data set should be saved into a HEX file that can be written to the flash memory of the Matter device.

In the nRF Connect example, the factory data set is represented in the CBOR format that is stored in a HEX file and then programmed into a device. Two separate scripts are used to create the factory data and the JSON format is used as an intermediate, human-readable representation of the data regulated by the [JSON Schema](https://github.com/project-chip/connectedhomeip/blob/master/scripts/tools/nrfconnect/nrfconnect_factory_data.schema) file.

All parameters of the factory data set are either mandatory or optional:

- Mandatory parameters must always be provided, as they are required for example to perform commissioning to the Matter network.
- Optional parameters can be used for development and testing purposes. For example, the ``user`` data parameter consists of all data that is needed by a specific manufacturer and that is not included in the mandatory parameters.

In the factory data set, the following formats are used:

- uint16 and uint32 -- These are the numeric formats representing, respectively, two-bytes length unsigned integer and four-bytes length unsigned integer. This value is stored in a HEX file in the big-endian order.
- Byte string - This parameter represents the sequence of integers between ``0`` and ``255``(inclusive), without any encoding. Because the JSON format does not allow to use of byte strings, the ``hex:`` prefix is added to a parameter, and its representation is converted to a HEX string. For example, an ASCII string *abba* is represented as *hex:61626261* in the JSON file and then stored in the HEX file as *0x61626261*. The HEX string length in the JSON file is two times greater than the byte string plus the size of the prefix.
- ASCII string is a string representation in ASCII encoding without null-terminating.
- ISO 8601 format is a [date format](https://www.iso.org/iso-8601-date-and-time-format.html) that represents a date given as ``YYYY-MM-DD`` or ``YYYYMMDD``.
- All certificates stored in factory data are given in the [X.509](https://www.itu.int/rec/T-REC-X.509-201910-I/en) format.

<hr>
<a name="Using own factory data"></a>

## Using own factory data implementation



<hr>
<a name="Generating factory data"></a>

## Generating factory data

### Creating factory data JSON file

### Verifying using the JSON Schema tool

The JSON file that contains factory data can be verified using the [JSON Schema tool](https://github.com/project-chip/connectedhomeip/blob/master/scripts/tools/nrfconnect/nrfconnect_factory_data.schema). This tool validates the structure and contents of the JSON data. Use this tool to validate the factory data JSON file.

To check the JSON file using JSON Schema tool manually on a Linux machine, complete the following steps:

1. Install the ``php-json-schema`` package:
```
$ sudo apt install php-json-schema
```

2. Run the following command, with ``<path_to_JSON_file>`` and ``<path_to_schema_file>`` replaced with the paths to the JSON file and the Schema file, respectively:
```
$ validate-json <path_to_JSON_file> <path_to_schema_file>
```

The tool returns empty output in case of success.

You can have the JSON file be checked automatically by the Python script during its generation by providing the path to the JSON schema file as an additional argument, which should replace the ``<path_to_schema>`` variable in the following command:
```
$ python generate_nrfconnect_chip_factory_data.py --schema <path_to_schema>
```

> Note: To learn more about JSON scheme, visit [this unofficial JSON Schema tool usage website](https://json-schema.org/understanding-json-schema/).

### Preparing factory data partition on a device

The factory data partition is an area in a device's persistent storage where a factory data set is stored. This area is configured using the [Partition Manager](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/scripts/partition_manager/partition_manager.html), within which all partitions are declared in the ``pm_static.yml`` file.

To prepare an example that supports factory data, a *pm_static* file must contain a partition called ``factory_data``, whose size should be a multiple of one flash page (for nRF52 and nRF53 SoCs, a single page size equals 4 kB).

See the following code snippet for an example of how to create a factory data partition in the ``pm_static.yml`` file.
The snippet is based on the ``pm_static.yml`` file from the [Lock application example](../../examples/lock-app/nrfconnect/configuration/nrf52840dk_nrf52840/pm_static_dfu.yml) and uses the nRF52840 DK:


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

In this example, a ``factory_data`` partition has been placed after the application partition (``mcuboot_primary_app`` is a container to store the application firmware that can be replaced in the Direct Firmware Update process). Its size has been set to 4 kB (``0x1000``), which corresponds to one page of the nRF52840 DK flash memory.

You can check the partition allocation using the west build system to ensure that the partition is appropriately created.
To use Partition Manager's report tool, navigate to the example directory and run the following command:

```
$ west build -t partition_manager_report
```

The output will look similar to the following one:

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

To store the factory data set in the device's persistent storage, convert the data from the JSON file to its binary representation in the CBOR format.
To do this, use the [nRF Connect generate partition script](../../scripts/tools/nrfconnect/nrfconnect_generate_partition.py).

To generate the factory data partition using the [nRF Connect generate partition script](../../scripts/tools/nrfconnect/nrfconnect_generate_partition.py), navigate to the *connectedhomeip* root directory and run the following command pattern:

```
$ python scripts/tools/nrfconnect/nrfconnect_generate_partition.py -i <path_to_JSON_file> -o <path_to_output> --offset <partition_address_in_memory> --size <partition_size>
```
In this command:

- ``<path_to_JSON_file>`` is a path to the JSON file containing appropriate factory data.
- ``<path_to_output>`` is a path to an output file without any prefix. For example, providing ``/build/output`` as an argument will result in creating ``/build/output.hex`` and ``/build/output.bin``.
- ``<partition_address_in_memory>`` is an address in the device's persistent storage area where a partition data set is to be stored.
- ``<partition_size>`` is a size of partition in the device's persistent storage area. New data is checked according to this value of the JSON data to see if it fits the size.

To see the optional arguments for the script, use the following command:

```
$ python scripts/tools/nrfconnect/nrfconnect_generate_partition.py -h
```

**Example of command for nRF52840 DK:**

```
$ python scripts/tools/nrfconnect/nrfconnect_generate_partition.py -i build/zephyr/factory_data.json -o build/zephyr/factory_data --offset 0xfb000 --size 0x1000
```

As a result, ``factory_data.hex`` and ``factory_data.bin`` files are created in the ``/build/zephyr/`` directory. The first file contains the memory offset.
For this reason, it can be programmed directly to the device using a programmer (for example, nrfjprog).


<hr>
<a name="Building an example with factory data"></a>

## Building an example with factory data

You can generate the factory data set manually using the instructions described in the [Generating factory data](#generating-factory-data) section.
Another way is to use the nRF Connect build system that generates factory data content automatically using Kconfig options and includes the content in the final firmware binary.

To enable generating the factory data set automatically, go to the example's directory and build the example with the following option:

```
$ west build -b nrf52840dk_nrf52840 -- -DCONFIG_CHIP_FACTORY_DATA_BUILD=y
```

Alternatively, you can also add `CONFIG_CHIP_FACTORY_DATA_BUILD=y` Kconfig setting to the example's ``prj.conf`` file.

Each factory data parameter has a default value. These are described in the [Kconfig file](../../config/nrfconnect/chip-module/Kconfig). Setting a new value for the factory data parameter can be done either by providing it as a build argument list or by using interactive Kconfig interfaces.

### Providing factory data parameters as a build argument list

This way for providing factory data can be used with third-party build script, as it uses only one command.
All parameters can be edited manually by providing them as an additional option for the west command.
For example:

```
$ west build -b nrf52840dk_nrf52840 -- --DCONFIG_CHIP_FACTORY_DATA_BUILD=y --DCONFIG_CHIP_DEVICE_DISCRIMINATOR=0xF11
```

Alternatively, you can add the relevant Kconfig option lines to the example's ``prj.conf`` file.

### Setting factory data parameters using interactive Kconfig interfaces

You can edit all configuration options using the interactive Kconfig interface.

See the [Configuring nRF Connect examples](../guides/nrfconnect_examples_configuration.md) page for information about how to configure Kconfig options.

In the configuration window, expand the items ``Modules -> connectedhomeip (/home/arbl/matter/connectedhomeip/config/nrfconnect/chip-module) -> Connected Home over IP protocol stack``. You should see all factory data configuration options, as in the following snippet:

```
(65521) Device vendor ID
(32774) Device product ID
[*] Enable Factory Data build
[*]     Enable merging generated factory data with the build tar
[*]     Use default certificates located in Matter repository
[ ]     Enable SPAKE2 verifier generation
[*]     Enable generation of a new Rotating device id unique id
(11223344556677889900) Serial number of device
(Nordic Semiconductor ASA) Human-readable vendor name
(not-specified) Human-readable product name
(2022-01-01) Manufacturing date in ISO 8601
(0) Integer representation of hardware version
(prerelease) user-friendly string representation of hardware ver
(0xF00) Device pairing discriminator
(20202021) SPAKE2+ passcode
(1000) SPAKE2+ iteration count
(U1BBS0UyUCBLZXkgU2FsdA==) SPAKE2+ salt in string format
(uWFwqugDNGiEck/po7KHwwMwwqZgN10XuyBajPGuyzUEV/iree4lOrao5GuwnlQ
(91a9c12a7c80700a31ddcfa7fce63e44) A rotating device id unique i
```

> Note: To get more information about how to use the interactive Kconfig interfaces, read the [Kconfig docummentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/build/kconfig/menuconfig.html).

<hr>
<a name="Programming factory data"></a>

## Programming factory data

The HEX file containing factory data can be programmed into the device's flash memory using nrfjprog and the J-Link programmer.
To do this, use the following command:

```
$ nrfjprog --program factory_data.hex
```

In this command, you can add the ``--family`` argument and provide the name of the DK: ``NRF52`` for the nRF52840 DK or ``NRF53`` for the nRF5340 DK.
For example:

```
$ nrfjprog --family NRF52 --program factory_data.hex
```

> Note: For more information about how to use the nrfjprog utility, visit [Nordic Semiconductor's Infocenter](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrf_cltools%2FUG%2Fcltools%2Fnrf_nrfjprogexe.html).


Another way to program the factory data to a device is to use the nrfconnect build system described in [Building an example with factory data](#building-an-example-with-factory-data), and build an example with the option `--DCONFIG_CHIP_MERGE_FACTORY_DATA_WITH_FIRMWARE=y`. After that, use the following command from the example's directory to write firmware and newly generated factory data at the same time:

```
$ west flash
```
