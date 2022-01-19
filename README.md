# oscillatord - daemon for disciplining an oscillator

Oscillatord aims at disciplining an oscillator to an external reference. It is responsible for fetching oscillator and reference data and pass them to a disciplining algorithm, and apply the decision of the algorithm regarding the oscillator.

It can be used to discipline the [Atomic Reference Card](https://www.orolia.com/about-the-atomic-reference-time-card-art-card/) from Orolia

Oscillatord can either be used to discipline and/or monitor a oscillatord a GNSS external reference.

For now only the following oscillators are supported:
- Orolia's mRO50
- Microsemi's SA3X
## Requirements

* cmake
* libgps26/28
* pps-tools
* [disciplining-minipod](https://github.com/Orolia2s/disciplining-minipod)
* [ubloxcfg](https://github.com/Orolia2s/ubloxcfgs) : use commit **2c37136c15d0f75cfc0db52433b8d945b403eec6**
* libsystemd for tests

## Installation

This program is built using **cmake**:

```
mkdir build
cd build
cmake ..
make
sudo make install
```

- **make install** will install the executable as well as a service to run oscillatord
- for the service to work, one must copy the [oscillatord_default.conf](./example_configurations/oscillatord_default.conf) file and rename it in */etc/oscillatord.conf*

For test purposes, it is easier to compile the executable and run it from a terminal:
in project's root directory
```
make -C build
sudo ./build/src/oscillatord example_configurations/oscillatord_default.conf
```


## Overview

The **oscillatord** daemon, takes input from a PHC clock,
reporting once per second, the phase error between an oscillator and a reference
GNSS receiver.
For an example of such a device, please see the **ptp_ocp** kernel
driver.

The phase error read is then used as an input to the
[disciplining-minipod](https://github.com/Orolia2s/disciplining-minipod) library which will compute a setpoint, used by
**oscillatord** to control an oscillator and discipline it to the 1PPS from a GNSS receiver.
**Oscillatord** also sets PHC'stime at start up, using Output from a GNSS receiver.

To communicate with GNSS receiver's serial it uses [ubloxcfg](https://github.com/Orolia2s/ubloxcfg). This library handles the serial connection to the GNSS receiver and parses Ublox messages.

## Operation

**oscillatord** uses a config file, for which examples are provided in the
**example_configurations/** directory.

So to invoke **oscillatord**, one can, for example do:

    oscillatord /etc/oscillatord.conf

Super user rights may be required to access the devices.

The daemon can be terminated with a **SIGINT** (Ctrl+C) or a **SIGTERM**.

## Oscillators supported

* **mRO50**

## Configuration

The configuration file is a succession of lines of the form:

    key=value

Any unsupported key will be ignored and thus can be used as a comment, but using
'#' at the start of a comment line is a good practice.
No blank character must be added before or after the '=' sign, or they will be
considered as part of, respectively, the **key** or the **value**.

### Common configuration keys

* **oscillator**: name of the oscillator to use, accepted: mRO50 only **Required**.
* **ptp-clock**: path to the PHC used to get the phase error and set time **Required**.
* **mro50-device**: Path the the mro50 device used to control the oscillator
**Required**
* **pps-device**: path to the 1PPS phase error device.
**Required**.
* **gnss-device-tty**: path to the device tty (e.g /dev/ttyS2)
**Required**.
* **opposite-phase-error**: if **true**, the opposite of the phase error
reported by the 1PPS phase error device, will be fed into
**disciplining-minipod**.
Any other value means **false**.
* **debug**: set debug level.

It also contains configuration keys for disciplining-minipod program (check [default config](./example_configurationns/oscillatord_default.conf) for description of parameters)

## ART Integration tests

ART Integration tests check wether art card handled by ptp_ocp driver works by interacting will all its devices
Integration tests can be run to test the behaviour an ART Card. they are located in *tests/art_integration_testsuite*.
Integration test do the following:
- Analyze filesystem to scan for */sys/class/timecard/\** directories
- Test GNSS Receiver
- Test PTP Hardware clock primary and auxillary fnctions
- Test mRO50 oscillator
- Check for EEPROM presence
- Start oscillatord service and check that phase error is not upon a threshold during 10 minutes.

## Build tests

```
mkdir build
cmake -D BUILD_TESTS=true ..
make
```

## Source tree organisation

    .
    ├── example_configurations        : per oscillator type configuration examples
    ├── gnss_config                   : GNSS default config file as output by libubloxcfg
    ├── src                           : main oscillatord source code
    │   └── oscillators               : oscillator implementations
    ├── systemd                       : systemd service file
    └── tests                         : code of the oscillator simulator and integration tests
        └── art_integration_testsuite : Integration tests for the ART card
        └── lib_osc_sim_stubs         : code of the lib_osc_sim_stubs library

