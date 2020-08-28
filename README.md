# oscillatord - daemon for disciplining an oscillator

## Overview

The **oscillatord** daemon, takes input from a 1pps phase error device,
reporting once per second, the phase error between an oscillator and a reference
GNSS receiver.
For an example of such a device, please see the [dmnd_1pps][dmnd_1pps] kernel
driver.

The phase error read is then used as an input to the
[liboscillator-disciplining] library which will compute a setpoint, used by
**oscillatord** to control an oscillator.

## Operation

**oscillatord** uses a config file, for which examples are provided in the
**example_configurations/** directory.

So to invoke **oscillatord**, one can, for example do:

    oscillatord /etc/oscillatord.conf

Super user rights may be required to access the devices.

The daemon can be terminated with a **SIGINT** (Ctrl+C) or a **SIGTERM**.

**liboscillator-disciplining** allows to export the data to a CSV file by
exporting the environment variable **OD_CSV**.
It's value must contain a valid path to where the CSV data must be saved.

## Oscillators supported

* **rakon** is the only real oscillator supported at the moment.
* **dummy** is a dummy oscillator, faking the control commands sent to the
oscillator, it is intended for oscillatord debugging purpose.
Note that it uses the 1pps and tsync devices anyway.
* **sim** is a simulated oscillator, it allows to test the algorithm in a
reproductible way and using the **libosc_sim_stubs.so** library provided, it
allows to run oscillatord without using any real hardware and with accelerated
time.

## Configuration

The configuration file is a succession of lines of the form:

    key=value

Any unsupported key will be ignored and thus can be used as a comment, but using
'#' at the start of a comment line is a good practice.
No blank character must be added before or after the '=' sign, or they will be
considered as part of, respectively, the **key** or the **value**.

### Common configuration keys

* **oscillator**: name of the oscillator to use, accepted: rakon, sim or dummy.
**Required**.
* **pps-device**: path to the 1PPS phase error device.
**Required**.
* **tsync-device**: path to the tsync device.
**Required**.
* **device-index**: index of the GNSS tsync device to use.
**Required**.
* **opposite-phase-error**: if **true**, the opposite of the phase error
reported by the 1PPS phase error device, will be fed into
**liboscillator-discpining**.
Any other value means **false**.
**Optional**, defaults to **false**.
* **libod-config-path**: Path to the liboscillator-disciplining configuration
file.
If not set, then the current config file will be used by
liboscillator-disciplining, which will only use its own config keys and ignore
those of escillatord.
Please refer to the liboscillatord-documentation for information on the valid
config keys.
**Optional**, unset by default.
* **enable-debug**: enables the **debug** level of logging.
**Optional**, defaults to **false**.

### Rakon-specific configuration keys

* **rakon-i2c-num**: index of the i2c device to use.
**Required**.
* **rakon-i2c-addr**: i2c address used by the rakon to communicate.
**Required**.

### Sim-specific configuration keys

* **simulation-period**: Duration of the simulated seconds in ns.
**Optional**, defaults to **1000000000** ns (1s).
* **turns**: Number of turns after which **oscillatord** must stop, 0 meaning
that **oscillatord** must run forever.
**Optional**, defaults to **0**.

**Notes**:
 * the **tsync-device** and **device-index** values, albeit required,
will be ignored when **libosc_sim_stubs.so** is LD\_PRELOAD-ed.
 * **pps-device** is not required, since its value will be provided internally
 by the simulator.


### Dummy-specific configuration keys

This oscillator has no specific configuration key.

## libosc_sim_stubs

This library is provided for testing oscillatord whithout using a real hardware.
By LD\_PRELOAD-ing it, the required tsync symbols will be overriden to report an
always valid PPS signal.

**clock_gettime** is also overriden to simulate a monotonic clock always growing
of 1 second at each call, independently of the frequency at which it is called,
in order to be able to accelerate or slow down the time.

It is intended for use with the **sim** oscillator.

## The oscillator simulator

Usage example:

    OD_CSV=oscillatord.sim.csv LD_PRELOAD=libosc_sim_stubs.so oscillatord ./example_sim.conf

Will run **oscillatord** using the simulator and generating a CSV data record
file **oscillatord.sim.csv** in the current directory.

Multiple instances can run in parallel provided they are ran in different
directories.

It simulates an oscillator by reporting a phase error which increase
proportionally to the distance between the current setpoint and the middle of
the admissible setpoints range [31500, 1016052].
Then a small random noise is added to the phase error.

## Source tree organisation

    .
    ├── example_configurations : per oscillator type configuration examples
    ├── src                    : main oscillatord source code
    │   └── oscillators        : oscillator implementations
    ├── systemd                : systemd service file
    └── tests                  : code of the oscillator simulator
        └── lib_osc_sim_stubs  : code of the lib_osc_sim_stubs library

### Coding style

This project follows the linux kernel coding style everywhere relevant.
In order to check the code is conformant, please execute:

    for f in $(find . -name '*.c' -o -name '*.h')
        do ../linux/scripts/checkpatch.pl $f
    done


[dmnd_1pps]: https://bitbucket.org/spectracom/dmnd-1pps-phase-module/src/master/
[liboscillator-disciplining]: https://bitbucket.org/spectracom/disciplining-lqr/src/master/