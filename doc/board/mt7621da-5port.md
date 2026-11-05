# MT7621DA five-port board notes

This board uses the embedded 128 MiB DDR3 KGD and an 8 MiB MD25Q64C
SPI NOR. The schematic still uses an MT7621A symbol; verify the fitted
SoC is the integrated-memory variant before bring-up.

## J6 serial header

The supplied five-port schematic, page 5, defines this four-pin header:

| Pin | Board signal | USB-to-3.3V-TTL connection |
| --- | --- | --- |
| 1 | 3.3 V | Leave disconnected for externally powered board bring-up |
| 2 | RXD1 | Adapter TX, when command input is needed |
| 3 | TXD1 | Adapter RX |
| 4 | GND | Adapter GND |

Start with receive-only wiring (pins 3 and 4), 57600 8N1, no flow control.
Confirm physical pin numbering on the PCB. Pin 1 is not ground.
TXD1 also participates in the reset boot straps. Do not infer reset-time
levels from its later UART output.

## Product port roles

The final product has four LAN ports and one WAN port, without Wi-Fi.
Linux uses PHY ports 0..3 as LAN and PHY port 4 as WAN. In the supplied
schematic these are PJ5=LAN1, PJ4=LAN2, PJ3=LAN3, PJ2=LAN4 and PJ1=WAN.
OpenWrt applies VLAN 1 (0 1 2 3 6t) and VLAN 2 (4 6t) through CPU port 6.
U-Boot recovery Ethernet is a separate boot-stage configuration; Linux
WAN/LAN roles do not establish which recovery ports have been validated.

## DDR dependencies and configuration

The legacy DRAMC binaries are required source inputs from the distributor
archive u-boot-mt7621-2018.09-20210902-git41e9b526.tar.xz. Keep both inputs
versioned, including the normally ignored .bin suffix:

* mt7621_stage_sram.bin: SHA256 1dda68aa089f0ff262e01539b990dea478952e9fb68bcc0a8cd6f76f0135c62e
* mt7621_stage_sram_noprint.bin: SHA256 8ee419275144fc298e9444d413d98e965a55d283152a74ea6a1f8de79eb516b6

Use mt7621_5port_defconfig: 128 MiB KGD DDR3, the 1200 data-rate setting,
and an 880 MHz CPU. A selected DDR2 fallback table does not force DDR2;
the reset DRAM_TYPE strap must select DDR3. A successful build does not
establish that DDR training or the physical board is working.

## Image layout

U-Boot occupies 0x000000..0x030000, environment 0x030000..0x040000,
factory 0x040000..0x050000, and firmware 0x050000..0x800000.
u-boot-mt7621.bin is a bootloader image only, not a full-flash image.
Back up the actual flash and preserve unique factory data before updates.
