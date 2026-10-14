# alif-sample-app

This repository is used to highlight features and bugs related to ALIF's E7 devkit.
Each of them will be in its own branch, and there is a template branch to start from.

## This branch: USB CDC-ECM on the DWC3 device controller

The HE application exposes an Ethernet interface over USB (CDC-ECM class of the `device_next`
stack, board at `192.0.2.1/24`). A Linux host pings it with
[`project/tools/scripts/usb_ecm_check.py`](project/tools/scripts/usb_ecm_check.py), around the frame
sizes that need a zero-length packet. Built with and without two patches of the `udc_dwc3` driver
(`zephyr_alif` v2.3.0), it shows what they fix.

### The patches

| Patch | Issue | Fix |
| ----- | ----- | --- |
| `udc-dwc3-report-reset-after-connect-done.patch` | The driver reports the bus reset to the stack at `USB_EVENT_RESET`, before the controller has negotiated the speed (`USB_EVENT_CONNECT_DONE`). The stack reads the high-speed default and, on a full-speed link, keeps its high-speed configuration: 512-byte bulk endpoints instead of 64. | Report the reset on Connect Done, once `DSTS` holds the speed (E7 HWRM 15.10.5.2.1). |
| `udc-dwc3-bulk-in-zlp.patch` | The driver ignores the zero-length packet the class asks for (`udc_ep_buf_set_zlp`). A bulk IN transfer whose length is a multiple of the packet size never ends on the bus: the host waits for the rest of the frame. | Send the ZLP once the data has completed. |

The ZLP fix alone is not enough on a full-speed link: without the first patch the class checks the
frame length against 512 instead of 64 and never asks for the ZLP.

Two more patches are applied in every build, because the link does not work at all without them:
`usb-cdc-ecm-ep-pool-in-non-secure0.patch` (the CDC-ECM endpoint buffers must live in `NON_SECURE0`
for the controller to reach them) and `udc-dwc3-reset-to-default-state.patch` (enumeration loops after
a bus reset). All four are described in [`project/zephyr/patches.yml`](project/zephyr/patches.yml).

### Setup

- E7 DK, HE core, the SoC USB port plugged into a Linux PC (`cdc_ether` driver, part of the kernel).
- High-speed link: plug the board directly into the PC.
- Full-speed link: plug it through a full-speed-only (USB 1.1) hub; the first issue only shows on a
  full-speed link.

### Running the demonstration

1. Build with the fixes: `./project/tools/scripts/update.py`, then build and flash the HE core
   (`west build -b alif_e7_dk/ae722f80f55d5xx/rtss_he project/apps/he_app -d build/he`).
2. On the PC: `sudo ./project/tools/scripts/usb_ecm_check.py`. Every size must come back.
3. Build without the fixes: `./project/tools/scripts/update.py --without-fixes`, rebuild, flash, and run the
   script again.

What to look at without the fixes. These are the expected results, from the driver analysis and
from tests on E7-based boards behind a full-speed STM32 host; the sample itself has not been run on
a DK yet:

| Link | Board console | Host |
| ---- | ------------- | ---- |
| High speed | `Bus reset: the stack uses its high speed configuration, bulk endpoints of 512 bytes` | Pings whose frame is a multiple of 512 bytes (payloads 470 and 982) lose their replies |
| Full speed | Same line: high speed and 512 bytes, while the host runs the link at 12 Mb/s | Kernel warnings about the endpoint size; pings whose frame is a multiple of 64 bytes lose their replies |

With the fixes, the console reports the speed the host negotiated (`full speed`, 64 bytes behind a
full-speed hub) and every ping comes back.

## Prerequisites

### Git & Docker

```bash
sudo apt install git
sudo apt install docker
sudo apt install docker-buildx
sudo usermod -aG docker $USER
```

### UDEV rules

```bash
wget -O 60-openocd.rules https://sf.net/p/openocd/code/ci/master/tree/contrib/60-openocd.rules?format=raw
sudo cp 60-openocd.rules /etc/udev/rules.d
sudo udevadm control --reload
```