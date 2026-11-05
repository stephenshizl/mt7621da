# MT7621DA four-LAN / one-WAN profile (no Wi-Fi)

The existing profile name is retained for firmware compatibility. The
product has four LAN ports and one WAN port, not five bridged LAN ports.

| SoC PHY | Schematic connector | Role |
| --- | --- | --- |
| 0 | PJ5 | LAN1 |
| 1 | PJ4 | LAN2 |
| 2 | PJ3 | LAN3 |
| 3 | PJ2 | LAN4 |
| 4 | PJ1 | WAN |

On first boot, board.d/02_network generates VLAN 1 with ports 0 1 2 3 6t
on eth0.1 and VLAN 2 with ports 4 6t on eth0.2. CPU port 6 must be tagged
for both interfaces. The supported llllw DT preset sets the PHY roles;
the final CPU-port VLAN mapping is applied by OpenWrt network setup.

LAN uses the standard static management address (192.168.1.1) and DHCP
server. WAN is a DHCP client, with the default WAN firewall zone and
LAN-to-WAN masquerading policy. HNAT remains disabled in the board DT;
gigabit routed/NAT throughput has not been demonstrated on hardware.

Existing overlay configuration is preserved by ordinary sysupgrade and
can retain the previous five-LAN layout. Back up settings and use a clean
configuration for board acceptance, or migrate VLANs and interfaces
deliberately; replacing the image alone does not migrate saved settings.

The board uses the DA embedded 128 MiB KGD DDR3 and an 8 MiB MD25Q64C
NOR (JEDEC C8 40 17). Do not add a duplicate flash ID by model-name alias.

The supplied schematic J6 header is pin 1 = 3.3 V, pin 2 = RXD1,
pin 3 = TXD1, pin 4 = GND. Use 3.3 V TTL, 57600 8N1, no flow control.
Do not connect adapter ground to pin 1. Confirm the fitted pin numbering.

The two config-4.4 files deliberately disable WLAN. m4 and findutils are
built from their original pinned source archives plus glibc compatibility
patches, not copied from /usr/bin. Keep their source hashes unchanged.

A successful build or image CRC check does not establish that the board
boots, DDR is stable, WAN/LAN isolation works, or factory MAC programming
has been completed. All of these still require board-level acceptance.
