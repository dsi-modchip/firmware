# dsi-modchip firmware

Firmware for the DSi bootrom glitcher PCB (RP2040)

## Installation

### Compiling the firmware

CMake options:

* `PICO_BOARD` (string): RP2040 board name to use. Possible options:
  * `haxboard_r1`: first revision of the PCB. You probably don't want to use this.
  * `haxboard_r2`: second revision of the PCB. Use this one.
  * `pico`
  * `seeed_xiao`
  * `waveshare_rp2040_one`
  * `waveshare_rp2040_zero`
  * `waveshare_rp2040_tiny`
  * `adafruit_itsybitsy_rp2040`

Development flags:

* `HAXMODE_OVERRIDE` (string): `flash`, `attack`, or empty
* `HAXMODE_DEV_FEATURES` (bool)
* `PASSIVE_TEST` (bool)
* `PICO_NO_FLASH` (bool)
* `PICO_COPY_TO_RAM` (bool)

### Compiling the payload image

Run `make` inside the `payload-example` folder.

### Installing the modchip on the DSi

TODO

## Usage

### USB interfaces

* USB-CDC "SPI printf() backdoor" interface
* DFU interface
* RP2 vendor reset interface: used for resetting the RP2040 back into
  bootloader mode, enabled using `picotool load -f` directly without pressing
  the RESET+BOOT buttons.

### Updating the RP2040 firmware

Updating the firmware can only be done when the PCB is removed from the DSi
mainboard. Once this is done, connect the PCB to your computer via USB.

Updating can be done using a simple `picotool -f -v -x update.uf2` command.
Alternatively, press both the BOOT and RESET buttons at the same time, and the
RP2040 should switch into bootloader mode.

### Updating the SPI payload image

Updating the SPI payload image should be done through DFU; similar to an RP2040
firmware update, this can only be done when the PCB is detached from the DSi.

Flashing/updating the payload can be done as follows:

```
dfu-util -a 0 -D payload.bin
```

Running `make dfu` inside the `payload-example` folder also runs the above command.

### SPI printf() backdoor interface

The RP2040 provide a "backdoor" interface to the code running on the ARM7 of
the DSi, by piggy-backing on the otherwise-innocous RDSR (0x05) command:

* `0x05 0x50`: print: all following bytes will be sent to a USB-CDC
  interface to enable `printf()`-style debugging.
* `0x05 0x13 0x37 0x42`: signal success condition to the modchip firmware.
* TODO: more stuff

### SWD debugging and UART

A [SOICbite](https://github.com/SimonMerrett/SOICbite) 'port' is used, with
the [standard SWD pinout](https://github.com/SimonMerrett/SOICbite#pin-assignment),
with additional UART `TX2`/`RX2` pins.

## TODO

* test for wifiboard presence
  * nCS2 weak PD in RP2040 -> stronger PU on wifiboard!
  * seems borked?

* better parameter sweep pattern (spiral)
  * currently using workaround (initial value inside rectangular sweep), prolly good enough
* save training data & use it when finished

* SPI modeswitch to bidirectional comms
* SPI control interface (command set part 2)
  * change SPI flash
* fancy SPI commands
  * read calibration and stats (and calibrated bool)
  * reset calibration
  * read & write parameter range
  * flash new firmware image (redundancy!)
* USB-CDC on SPI itf: commands TO rp2040
* maybe also vendor itf for configuring modchip?

* spiflash driver: fix DMA, use interrupts
* csmux only works on pio1, not pio0

### TODO for payload

* stage2 chainloader
* libdawn &amp; real bootloader

