# Matter nRF Connect Light Switch Example Application

The nRF Connect Light Switch Example demonstrates how to remotely control a lighting devices such as light bulbs or LEDs. 
The application should be used together with the [lighting app example](../../lighting-app/nrfconnect/README.md).
The light switch uses buttons to test changing the lighting application example LED state and works as a brightness dimmer.
You can use this example as a reference for creating your own application.

<p align="center">
  <img src="../../platform/nrfconnect/doc/images/Logo_RGB_H-small.png" alt="Nordic Semiconductor logo"/>
  <img src="../../platform/nrfconnect/doc/images/nRF52840-DK-small.png" alt="nRF52840 DK">
</p>

The example is based on
[Matter](https://github.com/project-chip/connectedhomeip) and Nordic
Semiconductor's nRF Connect SDK, and supports remote access and control of a
lighting examples over a low-power, 802.15.4 Thread network.

The example behaves as a Matter accessory, that is a device that can be paired
into an existing Matter network and can be controlled by this network.

<hr>

-   [Overview](#overview)
    -   [Bluetooth LE advertising](#bluetooth-le-advertising)
    -   [Bluetooth LE rendezvous](#bluetooth-le-rendezvous)
    -   [Device Firmware Upgrade](#device-firmware-upgrade)
-   [Requirements](#requirements)
    -   [Supported devices](#supported_devices)
-   [Device UI](#device-ui)
    - [LEDs](#leds)
    - [Buttons](#buttons)
    - [Matter CLI](#matter-cli-commands)
-   [Setting up the environment](#setting-up-the-environment)
    -   [Using Docker container for setup](#using-docker-container-for-setup)
    -   [Using native shell for setup](#using-native-shell-for-setup)
-   [Building](#building)
    -   [Removing build artifacts](#removing-build-artifacts)
    -   [Building with release configuration](#building-with-release-configuration)
    -   [Building with low-power configuration](#building-with-low-power-configuration)
    -   [Building with Device Firmware Upgrade support](#building-with-device-firmware-upgrade-support)
-   [Configuring the example](#configuring-the-example)
-   [Flashing and debugging](#flashing-and-debugging)
-   [Testing the example](#testing-the-example)
    -   [Binding process](#binding-process)
    -   [Testing Device Firmware Upgrade](#testing-device-firmware-upgrade)

<hr>

<a name="overview"></a>

## Overview

This example is running on the nRF Connect platform, which is based on Nordic
Semiconductor's
[nRF Connect SDK](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
and [Zephyr RTOS](https://zephyrproject.org/). Visit Matter's
[nRF Connect platform overview](../../../docs/guides/nrfconnect_platform_overview.md)
to read more about the platform structure and dependencies.

A light switch device is a simple embedded controller, which has the ability to control lighting devices such as light-bulbs, LEDs, etc. After commissioning into a Matter network a light-switch device does not know what can control. It means there is no information about another device connected to the same network and the user must provide this information to a light switch. A solution for this problem is a process called binding. 

The Matter device that runs the light switch application is controlled by the Matter controller device over the Thread protocol. By default, the Matter device has Thread disabled, and it should be paired with Matter controller and get
configuration from it. Some actions required before establishing full
communication are described below.

The example also comes with a test mode, which allows to start Thread with the
default settings by pressing button manually. However, this mode does not
guarantee that the device will be able to communicate with the Matter controller
and other devices.

The example can be configured to use the secure bootloader and utilize it for
performing over-the-air Device Firmware Upgrade using Bluetooth LE.

### Bluetooth LE advertising

In this example, to commission the device onto a Matter network, it must be
discoverable over Bluetooth LE. For security reasons, you must start Bluetooth
LE advertising manually after powering up the device by pressing **Button 4**.

### Bluetooth LE rendezvous

In this example, the commissioning procedure is done over Bluetooth LE between a
Matter device and the Matter controller, where the controller has the
commissioner role.

To start the rendezvous, the controller must get the commissioning information
from the Matter device. The data payload is encoded within a QR code, printed to
the UART console, and shared using an NFC tag. NFC tag emulation starts
automatically when Bluetooth LE advertising is started and stays enabled until
Bluetooth LE advertising timeout expires.

#### Thread provisioning

Last part of the rendezvous procedure, the provisioning operation involves
sending the Thread network credentials from the Matter controller to the Matter
device. As a result, device is able to join the Thread network and communicate
with other Thread devices in the network.

### Device Firmware Upgrade

The example supports over-the-air (OTA) device firmware upgrade (DFU) using one
of the two available methods:

-   Matter OTA update that is mandatory for Matter-compliant devices and enabled
    by default
-   [Simple Management Protocol](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/guides/device_mgmt/index.html#device-mgmt)
    over Bluetooth LE, an optional proprietary method that can be enabled to
    work alongside the default Matter OTA update. Note that this protocol is not
    a part of the Matter specification.

For both methods, the
[MCUboot](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/mcuboot/index.html)
bootloader solution is used to replace the old firmware image with the new one.

#### Matter Over-the-Air Update

The Matter over-the-air update distinguishes two types of nodes: OTA Provider
and OTA Requestor.

An OTA Provider is a node that hosts a new firmware image and is able to respond
on an OTA Requestor's queries regarding availability of new firmware images or
requests to start sending the update packages.

An OTA Requestor is a node that wants to download a new firmware image and sends
requests to an OTA Provider to start the update process.

#### Simple Management Protocol

Simple Management Protocol (SMP) is a basic transfer encoding that is used for
device management purposes, including application image management. SMP supports
using different transports, such as Bluetooth LE, UDP, or serial USB/UART.

In this example, the Matter device runs the SMP Server to download the
application update image using the Bluetooth LE transport.

See the
[Building with Device Firmware Upgrade support](#building-with-device-firmware-upgrade-support)
section to learn how to enable SMP and use it for the DFU purpose in this
example.

#### Bootloader

MCUboot is a secure bootloader used for swapping firmware images of different
versions and generating proper build output files that can be used in the device
firmware upgrade process.

The bootloader solution requires an area of flash memory to swap application
images during the firmware upgrade. Nordic Semiconductor devices use an external
memory chip for this purpose. The memory chip communicates with the
microcontroller through the QSPI bus.

See the
[Building with Device Firmware Upgrade support](#building-with-device-firmware-upgrade-support)
section to learn how to change MCUboot and flash configuration in this example.

<hr>

<a name="requirements"></a>

## Requirements

The application requires a specific revision of the nRF Connect SDK to work
correctly. See [Setting up the environment](#setting-up-the-environment) for
more information.

<a name="supported_devices"></a>

### Supported devices

The example supports building and running on the following devices:

| Hardware platform                                                                         | Build target               | Platform image                                                                                                                                   |
| ----------------------------------------------------------------------------------------- | -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| [nRF52840 DK](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-DK) | `nrf52840dk_nrf52840`      | <details><summary>nRF52840 DK</summary><img src="../../platform/nrfconnect/doc/images/nRF52840_DK_info-medium.jpg" alt="nRF52840 DK"/></details> |
| [nRF5340 DK](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF5340-DK)   | `nrf5340dk_nrf5340_cpuapp` | <details><summary>nRF5340 DK</summary><img src="../../platform/nrfconnect/doc/images/nRF5340_DK_info-medium.jpg" alt="nRF5340 DK"/></details>    |

<hr>

<a name="device-ui"></a>

## Device UI

This section lists the User Interface elements that you can use to control and
monitor the state of the device. These correspond to PCB components on the
platform image.
### LEDs

This section describes all behaviors of LEDs located on platform image.

**LED 1** shows the overall state of the device and its connectivity. The
following states are possible:

-   _Short Flash On (50 ms on/950 ms off)_ &mdash; The device is in the
    unprovisioned (unpaired) state and is waiting for a commissioning
    application to connect.

-   _Rapid Even Flashing (100 ms on/100 ms off)_ &mdash; The device is in the
    unprovisioned state and a commissioning application is connected through
    Bluetooth LE.

-   _Short Flash Off (950ms on/50ms off)_ &mdash; The device is fully
    provisioned, but does not yet have full Thread network or service
    connectivity.

-   _Solid On_ &mdash; The device is fully provisioned and has full Thread
    network and service connectivity.

**LED 2** simulates the BLE DFU process. The following states are possible:

-   _Off_ &mdash; BLE is not advertising and DFU can not be performed.

-   _Rapid Even Flashing (30 ms off / 170 ms on)_ &mdash; BLE is advertising, DFU process can be started.

**LED 3** can be used to identify the device. The LED starts blinking evenly
(500 ms on/500 ms off) when the Identify command of the Identify cluster is
received. The command's argument can be used to specify the duration of the
effect.

### Buttons

This section describes a reaction to pressing or holding buttons located on the platform image.

**Button 1** can be used for the following purposes:

-   _Pressed for 6 s_ &mdash; Initiates the factory reset of the device.
    Releasing the button within the 3-second window cancels the factory reset
    procedure. **LEDs 1-4** blink in unison when the factory reset procedure is
    initiated.

-   _Pressed for less than 3 s_ &mdash; Initiates the OTA software update
    process. This feature is disabled by default, but can be enabled by
    following the
    [Building with Device Firmware Upgrade support](#building-with-device-firmware-upgrade-support)
    instruction.

**Button 2** can be used for the following purposes:

-   _Pressing the button once_ &mdash; changes the light state to the opposite one on a bound lighting bulb device ([lighting-app](../../lighting-app/nrfconnect/) example).

-   _Pressed for more than 2 s_ &mdash; changes the light's brightness (dimmer functionality) on a bound lighting bulb device ([lighting-app](../../lighting-app/nrfconnect/) example).
    Brightness is changing from 0 % to 100 % with 1% increments every 300 milliseconds as long as **Button 2** is pressed.

**Button 4** &mdash; Pressing the button once starts the NFC tag emulation and
enables Bluetooth LE advertising for the predefined period of time (15 minutes
by default).

**SEGGER J-Link USB port** can be used to get logs from the device or
communicate with it using the
[command line interface](../../../docs/guides/nrfconnect_examples_cli.md).

**NFC port with antenna attached** can be used to start the
[rendezvous](#bluetooth-le-rendezvous) by providing the commissioning
information from the Matter device in a data payload that can be shared using
NFC.

### Matter CLI commands

Matter CLI allows to run commands via serial interface after USB cable connection to the Nordic Semiconductor kit.

To enable matter CLI a light_switch-app example should be compiled with additional option __-DCONFIG_CHIP_LIB_SHELL=y__. Run the following command with _build-target_ replaced with the build target name of the Nordic Semiconductor kit you are using (for example _nrf52840dk_nrf52840_):

    west build -b build-target -- -DCONFIG_CHIP_LIB_SHELL=y

Use these commands to control a device running lighting-app example via Matter CLI:

    uart:~$ switch onoff on     : sends unicast On command to bound device
    uart:~$ switch onoff off    : sends unicast Off command to bound device
    uart:~$ switch onoff toggle : sends unicast Toggle command to bound device

Use these commands to control group of devices running lighting-app example via Matter CLI:

    uart:~$ switch groups onoff on     : sends multicast On command to all bound devices in a group
    uart:~$ switch groups onoff off    : sends multicast Off command to  all bound devices in a group
    uart:~$ switch groups onoff toggle : sends multicast Toggle command to all bound devices in a group

Check the [CLI tutorial](../../../docs/guides/nrfconnect_examples_cli.md) to
learn how to use other command-line interface of the application.

<hr>

## Setting up the environment

Before building the example, check out the Matter repository and sync submodules
using the following command:

        $ git submodule update --init

The example requires a specific revision of the nRF Connect SDK. You can either
install it along with the related tools directly on your system or use a Docker
image that has the tools pre-installed.

If you are a macOS user, you won't be able to use the Docker container to flash
the application onto a Nordic development kit due to
[certain limitations of Docker for macOS](https://docs.docker.com/docker-for-mac/faqs/#can-i-pass-through-a-usb-device-to-a-container).
Use the [native shell](#using-native-shell) for building instead.

### Using Docker container for setup

To use the Docker container for setup, complete the following steps:

1.  If you do not have the nRF Connect SDK installed yet, create a directory for
    it by running the following command:

        $ mkdir ~/nrfconnect

2.  Download the latest version of the nRF Connect SDK Docker image by running
    the following command:

        $ docker pull nordicsemi/nrfconnect-chip

3.  Start Docker with the downloaded image by running the following command,
    customized to your needs as described below:

         $ docker run --rm -it -e RUNAS=$(id -u) -v ~/nrfconnect:/var/ncs -v ~/connectedhomeip:/var/chip \
             -v /dev/bus/usb:/dev/bus/usb --device-cgroup-rule "c 189:* rmw" nordicsemi/nrfconnect-chip

    In this command:

    -   _~/nrfconnect_ can be replaced with an absolute path to the nRF Connect
        SDK source directory.
    -   _~/connectedhomeip_ must be replaced with an absolute path to the CHIP
        source directory.
    -   _-v /dev/bus/usb:/dev/bus/usb --device-cgroup-rule "c 189:_ rmw"\*
        parameters can be omitted if you are not planning to flash the example
        onto hardware. These parameters give the container access to USB devices
        connected to your computer such as the nRF52840 DK.
    -   _--rm_ can be omitted if you do not want the container to be
        auto-removed when you exit the container shell session.
    -   _-e RUNAS=\$(id -u)_ is needed to start the container session as the
        current user instead of root.

4.  Update the nRF Connect SDK to the most recent supported revision, by running
    the following command:

         $ cd /var/chip
         $ python3 scripts/setup/nrfconnect/update_ncs.py --update

Now you can proceed with the [Building](#building) instruction.

### Using native shell for setup

To use the native shell for setup, complete the following steps:

1.  Download and install the following additional software:

    -   [nRF Command Line Tools](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Command-Line-Tools)
    -   [GN meta-build system](https://gn.googlesource.com/gn/)

2.  If you do not have the nRF Connect SDK installed, follow the
    [guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_assistant.html#)
    in the nRF Connect SDK documentation to install the latest stable nRF
    Connect SDK version. Since command-line tools will be used for building the
    example, installing SEGGER Embedded Studio is not required.

    If you have the SDK already installed, continue to the next step and update
    the nRF Connect SDK after initializing environment variables.

3.  Initialize environment variables referred to by the CHIP and the nRF Connect
    SDK build scripts. Replace _nrfconnect-dir_ with the path to your nRF
    Connect SDK installation directory, and _toolchain-dir_ with the path to GNU
    Arm Embedded Toolchain.

         $ source nrfconnect-dir/zephyr/zephyr-env.sh
         $ export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
         $ export GNUARMEMB_TOOLCHAIN_PATH=toolchain-dir

4.  Update the nRF Connect SDK to the most recent supported revision by running
    the following command (replace _matter-dir_ with the path to Matter
    repository directory):

         $ cd matter-dir
         $ python3 scripts/setup/nrfconnect/update_ncs.py --update

Now you can proceed with the [Building](#building) instruction.

<hr>

<a name="building"></a>

## Building

Complete the following steps, regardless of the method used for setting up the
environment:

1.  Navigate to the example's directory:

        $ cd examples/light-switch-app/nrfconnect

2.  Run the following command to build the example, with _build-target_ replaced
    with the build target name of the Nordic Semiconductor's kit you own, for
    example `nrf52840dk_nrf52840`:

         $ west build -b build-target

    You only need to specify the build target on the first build. See
    [Requirements](#requirements) for the build target names of compatible kits.

The output `zephyr.hex` file will be available in the `build/zephyr/` directory.

### Removing build artifacts

If you're planning to build the example for a different kit or make changes to
the configuration, remove all build artifacts before building. To do so, use the
following command:

    $ rm -r build

### Building with release configuration

To build the example with release configuration that disables the diagnostic
features like logs and command-line interface, run the following command:

    $ west build -b build-target -- -DOVERLAY_CONFIG=third_party/connectedhomeip/config/nrfconnect/app/release.conf

Remember to replace _build-target_ with the build target name of the Nordic
Semiconductor's kit you own.

### Building with low-power configuration

You can build the example using the low-power configuration, which enables
Thread's Sleepy End Device mode and disables debug features, such as the UART
console or the **LED 1** usage.

To build for the low-power configuration, run the following command with
_build-target_ replaced with the build target name of the Nordic Semiconductor's
kit you own (for example `nrf52840dk_nrf52840`):

    $ west build -b build-target -- -DOVERLAY_CONFIG=overlay-low_power.conf

For example, use the following command for `nrf52840dk_nrf52840`:

    $ west build -b nrf52840dk_nrf52840 -- -DOVERLAY_CONFIG=overlay-low_power.conf

### Building with Device Firmware Upgrade support

Support for DFU using Matter OTA is enabled by default.

To enable DFU over Bluetooth LE, run the following command with _build-target_
replaced with the build target name of the Nordic Semiconductor kit you are
using (for example `nrf52840dk_nrf52840`):

    $ west build -b build-target -- -DBUILD_WITH_DFU=BLE

To completely disable support for both DFU methods, run the following command
with _build-target_ replaced with the build target name of the Nordic
Semiconductor kit you are using (for example `nrf52840dk_nrf52840`):

    $ west build -b build-target -- -DBUILD_WITH_DFU=OFF

> **Note**:
>
> There are two types of Device Firmware Upgrade modes: single-image DFU and
> multi-image DFU. Single-image mode supports upgrading only one firmware image,
> the application image, and should be used for single-core nRF52840 DK devices.
> Multi-image mode allows to upgrade more firmware images and is suitable for
> upgrading the application core and network core firmware in two-core nRF5340
> DK devices.

#### Changing Device Firmware Upgrade configuration

To change the default DFU configuration, edit the following overlay files
corresponding to the selected configuration:

-   `overlay-mcuboot_qspi_nor_support.conf` - general file enabling MCUboot and
    QSPI NOR support, used by all DFU configurations
-   `overlay-single_image_smp_dfu_support.conf` - file enabling single-image DFU
    over Bluetooth LE using SMP
-   `overlay-multi_image_smp_dfu_support.conf` - file enabling multi-image DFU
    over Bluetooth LE using SMP
-   `overlay-ota_requestor.conf` - file enabling Matter OTA Requestor support.

The files are located in the `config/nrfconnect/app` directory. You can also
define the desired options in your example's `prj.conf` file.

#### Changing bootloader configuration

To change the default MCUboot configuration, edit the
`mcuboot_single_image_dfu.conf` or `mcuboot_multi_image_dfu.conf` overlay files
depending on whether the build target device supports multi-image DFU (nRF5340
DK) or single-image DFU (nRF52840 DK). The files are located in the
`configuration` directory.

Make sure to keep the configuration consistent with changes made to the
application configuration. This is necessary for the configuration to work, as
the bootloader image is a separate application from the user application and it
has its own configuration file.

#### Changing flash memory settings

In the default configuration, the MCUboot uses the
[Partition Manager](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/scripts/partition_manager/partition_manager.html#partition-manager)
to configure flash partitions used for the bootloader application image slot
purposes. You can change these settings by defining
[static partitions](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/scripts/partition_manager/partition_manager.html#ug-pm-static).
This example uses this option to define using an external flash.

To modify the flash settings of your board (that is, your _build-target_, for
example `nrf52840dk_nrf52840`), edit the `pm_static.yml` file located in the
`configuration/build-target/` directory.

<hr>

<a name="configuring"></a>

## Configuring the example

The Zephyr ecosystem is based on Kconfig files and the settings can be modified
using the menuconfig utility.

To open the menuconfig utility, run the following command from the example
directory:

    $ west build -b build-target -t menuconfig

Remember to replace _build-target_ with the build target name of the Nordic
Semiconductor's kit you own.

Changes done with menuconfig will be lost if the `build` directory is deleted.
To make them persistent, save the configuration options in the `prj.conf` file.
For more information, see the
[Configuring nRF Connect SDK examples](../../../docs/guides/nrfconnect_examples_configuration.md)
page.

<hr>

<a name="flashing"></a>

## Flashing and debugging

To flash the application to the device, use the west tool and run the following
command from the example directory:

        $ west flash --erase

If you have multiple development kits connected, west will prompt you to pick
the correct one.

To debug the application on target, run the following command from the example
directory:

        $ west debug

<hr>

## Testing the example
This section contains information about how to prepare devices, perform the binding process and add proper ACLs.

To test this example, two devices - a device running [Lighting-App example](../../lighting-app/nrfconnect/) and a light switch device must be commissioned to the same Matter Network. Check [chip-tool guide](../../../docs/guides/chip_tool_guide.md) to learn how to do it using chip-tool.

### Binding process

To perform the binding process you need a controller which can write binding table to light switch device and write proper ACL to an endpoint light bulb ([Lighting-App](../../lighting-app/nrfconnect/)). For example, the [chip-tool for Windows/Linux](../../chip-tool/README.md) can be used as a controller. An ACL should contain information about all clusters which can be called by light-switch. See [chip-tool-guide interacting with ZCL clusters section](../../../docs/guides/chip_tool_guide.md#interacting-with-zcl-clusters) for more information about ACLs.

### Unicast binding to the remote endpoint using chip-tool for Windows/Linux
Binding process consists of some steps which must be completed before communication between devices.

In this example all commands below are written for light switch device commissioned to a Matter network with nodeId = 2 and light bulb device commissioned to the same network with node = 1.

To perform binding process you need to complete following steps:

1. Navigate to the CHIP root directory:

2. Build chip-tool using [chip-tool guide](../../../docs/guides/chip_tool_guide.md#building).

3. Go to chip-tool build directory.

4. Add ACL (Access Control List) in the device running [Lighting-App example](../../lighting-app/nrfconnect/) permissions to receive commands from Light Switch:

        chip-tool accesscontrol write acl '[{"fabricIndex": 1, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 1, "privilege": 3, "authMode": 2, "subjects": [2], "targets": [{"cluster": 6, "endpoint": 1, "deviceType": null}, {"cluster": 8, "endpoint": 1, "deviceType": null}]}]' 1 0

    Where:  
    __{"fabricIndex": 1, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}__ is an ACL for communication with chip-tool.

    __{"fabricIndex": 1, "privilege": 3, "authMode": 2, "subjects": [2], "targets": [{"cluster": 6, "endpoint": 1, "deviceType": null}, {"cluster": 8, "endpoint": 1, "deviceType": null}]}__ is an ACL for binding (cluster nr 6 is a _onoff_ and cluster nr 8 is a _LevelControl_).

5. Add binding table to Light Switch binding cluster: 
        
        chip-tool binding write binding '[{"fabricIndex": 1, "node": 1, "endpoint": 1, "cluster": 6}, {"fabricIndex": 1, "node": 1, "endpoint": 1, "cluster": 8}]' 2 1
    
    Where:  
    __{"fabricIndex": 1, "node": <1>, "endpoint": 1, "cluster": 6}__ is a binding for __onoff__ cluster 

    __{"fabricIndex": 1, "node": <1>, "endpoint": 1, "cluster": 8}__ is a binding for __LevelControl__ cluster. 


### Groupcast binding to the group of remote endpoints using chip-tool for Windows/Linux
A groupcast binding allows light switch controls all devices running [Lighting-App example](../../lighting-app/nrfconnect/) and added to one multicast group. After that, Light Switch can send multicast requests, and all of the devices listening bound groups can run the received command. It allows controlling more than one lighting device via a single light switch simultaneously.

To run this example with binding for groupcast you need to complete following steps:

1. Navigate to the CHIP root directory:

2. Build chip-tool using [chip-tool guide](../../../docs/guides/chip_tool_guide.md#building).

3. Go to chip-tool build directory.

4. Add ACL (Access Control List) in the lighting endpoint permissions to receive commands from Light Switch:

        chip-tool accesscontrol write acl '[{"fabricIndex": 1, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 1, "privilege": 3, "authMode": 2, "subjects": [2], "targets": [{"cluster": 6, "endpoint": 1, "deviceType": null}, {"cluster": 8, "endpoint": 1, "deviceType": null}]}]' 1 0

5. Add Light Switch device to multicast group:

        chip-tool tests TestGroupDemoConfig --nodeId 1

6. Add all lighting devices to the same multicast group by applying command below for each of them:

        chip-tool tests TestGroupDemoConfig --nodeId <lighting node id>

7. Add Binding commands for group multicast:

        chip-tool binding write binding '[{"fabricIndex": 1, "group": 257}]' 2 1


To test communication between Light Switch and bound devices use [buttons](#buttons) or [CLI commands](#matter-cli-commands). Both of them are described in [Device UI section](#device-ui).

Notes:

To use a light switch without brightness dimmer, apply only the first binding command with cluster nr 6.

If a light switch device is rebooting binding table is restored from flash memory, the device tries to bind a known device running [Lighting-App example](../../lighting-app/nrfconnect/).
### Testing Device Firmware Upgrade

Read the
[DFU tutorial](../../../docs/guides/nrfconnect_examples_software_update.md) to
see how to upgrade your device firmware.
