# alif-sample-app — branch `2FY-BLE`

This branch demonstrates **BLE** and **WiFi** connectivity on the Alif E7 devkit using the **Murata 2FY** module (Infineon CYW55513 / CYW43xxx).

- **BLE**: connectable advertising over HCI-UART on the HE core (RTSS-HE), H4 transport via UART3.
- **WiFi**: AIROC CYW55513 offload via SDHC (SDIO protocol).
- **Ethernet**: on-chip DWMAC with Realtek RTL8201FR PHY.

## Hardware

| Overview | Murata 2FY module |
|:-:|:-:|
| ![Devkit E7 + Murata 2FY](doc/IMG_20260611_143810.jpg) | ![Murata 2FY module](doc/IMG_20260611_143826.jpg) |

> **Important:** Make sure the jumpers on the devkit are placed exactly as shown in the photos above before powering on the board.

## Pin-out

Pin assignment on the Alif E7 devkit.

### UART3 — BLE HCI (CYW43xxx)

| Signal | Periph | Alif Pin | Direction | Description       |
| ------ | ------ | -------- | --------- | ----------------- |
| RX     | UART3  | P1_2     | Input     | HCI UART receive  |
| TX     | UART3  | P1_3     | Output    | HCI UART transmit |
| CTS    | UART3  | P7_2     | Input     | Flow control CTS  |
| RTS    | UART3  | P7_3     | Output    | Flow control RTS  |

115200 baud, hardware flow control enabled.

### GPIO — BT control

| Signal    | GPIO    | Alif Pin | Direction | Description              |
| --------- | ------- | -------- | --------- | ------------------------ |
| BT-REG-ON | gpio0.2 | P0_2     | Output    | BLE module enable (high) |

### GPIO — WiFi control

| Signal    | GPIO     | Alif Pin | Direction | Description               |
| --------- | -------- | -------- | --------- | ------------------------- |
| WL-REG-ON | gpio14.7 | P14_7    | Output    | WiFi module enable (high) |

The WiFi (AIROC CYW55513) is connected via the SoC SDHC interface.

### Ethernet (DWMAC)

| Signal     | Alif Pin | Signal     | Alif Pin |
| ---------- | -------- | ---------- | -------- |
| ETH_TXD0   | P6_0     | ETH_RXD0   | P11_3    |
| ETH_TXD1   | P10_5    | ETH_RXD1   | P11_4    |
| ETH_TXEN   | P10_6    | ETH_CRS_DV | P11_5    |
| ETH_REFCLK | P11_0    | ETH_RST    | P11_6    |
| ETH_MDIO   | P11_1    | ETH_IRQ    | P11_7    |
| ETH_MDC    | P11_2    |            |          |

Default MAC address: `00:16:3E:11:22:33`

### Wiring diagram

```
                    +------------------+
                    |   Murata 2FY     |
                    |  (CYW43xxx BLE)  |
   P1_2 (RX)  ---->| UART RX          |
   P1_3 (TX)  <----| UART TX          |
   P7_2 (CTS) ---->| CTS              |
   P7_3 (RTS) <----| RTS              |
   P0_2 (GPIO) --->| BT-REG-ON        |
                    +------------------+

                    +------------------+
                    |   Murata 2FY     |
                    |  (CYW55513 WiFi) |
   SDHC bus  <----->| SDIO             |
   P14_7 (GPIO) -->| WL-REG-ON        |
                    +------------------+

                    +------------------+
                    |   RTL8201FR      |
   DWMAC  <------->| RMII/MDIO        |
   P6/P10/P11      |                  |
                    +------------------+
```

---

## Getting started

### 1. Open in VSCode devcontainer

Open the repository folder in VSCode. When prompted, click **Reopen in Container** to start the devcontainer with all required tools pre-installed.

### 2. Initialize

Click the **⚙ Initialize** button in the status bar. This runs `west init -l project` and sets up the west workspace.

> This step is only needed once (or after a full clean).

### 3. Update

Click the **↻ Update** button. This fetches all west modules and the Infineon HAL blobs required for the Murata 2FY.

### 4. Configure — HE + HP with MCUboot

Click the **⊙ Configure** button and select:

- Core: **🔒 🟣 HE & HP with MCUboot**
- MRAM: choose whether to erase MRAM before flashing

### 5. Build

Click the **⊙ Build** button (or press `Ctrl+Shift+B`). This builds both the HE and HP applications with MCUboot.

### 6. Flash

Connect the devkit via USB, then click the **🚀 Flash** button. The firmware is written to MRAM via the Alif SE Tools.

### 7. Monitor serial output

Open the Serial Monitor (`ttyACM1`, **115200 baud**) to view the application logs from the HE core.

---

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

---

## Troubleshooting — BLE / CYW55513 (Murata 2FY)

### Working baseline

BLE initialises correctly at commit **`83aaccd0`** (`feat: Add BLE with 2FY`) with the patches in `project/zephyr/patches/zephyr/bt-hci-infineon-cyw55513-support.patch` applied to the Zephyr tree.

### Infineon ACS-1852 recommendations — do NOT apply on this hardware

Commit **`ec19642`** (`feat: apply Infineon ACS-1852 recommendations for CYW55513 BLE`) applies the recommendations received from Infineon BT SW (Jia, ACS-1852). **All three recommendations fail on the Alif E7 + Murata 2FY combination.** The analysis below documents each failure so Infineon can reproduce and diagnose from this branch.

---

### Failure 1 — `fw-download-speed = <3000000>` triggers UPDATE_BAUDRATE, which the CYW55513 ROM does not implement

**Recommendation:** set `fw-download-speed = <3000000>` in the board devicetree overlay to switch the HCI UART from 115200 to 3 Mbaud before firmware download, making the `BT_POWER_ON_SETTLING_TIME_MS` and pre-WRITE_RAM delay workarounds unnecessary.

**Observed failure (`ec19642`):**

```
ASSERTION FAIL [err == 0] @ zephyr/subsys/bluetooth/host/hci_core.c:436
        Controller unresponsive, command opcode 0xfc18 timeout with err -11
```

**Analysis:**

The Infineon driver (`h4_ifx_cyw43xxx.c`) reads the `fw-download-speed` DT property and, when it differs from 115200, sends `HCI_UPDATE_BAUDRATE` (vendor opcode `0xFC18`) to the controller **before** the HCD firmware download. The CYW55513 ROM firmware — the minimal firmware running immediately after `BT_REG_ON` — does not implement this vendor command. The controller never responds and the Zephyr BT host times out after its 10 s HCI command deadline (`err -11` = `-EAGAIN`).

Evidence: the timeout occurs regardless of settling time. With 500 ms settling (`ec19642`) the timeout appears at `[00:00:13.928]`; restoring 1500 ms settling shifts it to `[00:00:14.928]` — exactly +1 s, confirming the settling time patch is applied but irrelevant to this root cause.

**Required workaround:** do not set `fw-download-speed`. The HCD firmware must be downloaded at the default 115200 baud using `DOWNLOAD_MINIDRIVER (0xFC2E)` + `WRITE_RAM (0xFC4C)`.

**Open question for Infineon:** does the CYW55513 ROM implement `UPDATE_BAUDRATE (0xFC18)`? If not, at what stage (and via which opcode) can the UART baud rate be changed?

---

### Failure 2 — `BT_POWER_ON_SETTLING_TIME_MS` must remain 1500 ms

**Recommendation:** revert `BT_POWER_ON_SETTLING_TIME_MS` from 1500 ms back to the upstream default of 500 ms.

**Observed failure:** same `0xFC18` timeout as Failure 1 (when `fw-download-speed` is not set, the next command in the sequence is `DOWNLOAD_MINIDRIVER` which requires the chip to be ready).

**Analysis:** with 500 ms settling, `DOWNLOAD_MINIDRIVER` appears to succeed but the first `WRITE_RAM` returns HCI status `0x12` ("Invalid HCI Command Parameters"). The chip is still completing its internal boot sequence. 1500 ms is the minimum observed settling time for the CYW55513 on this power rail before it reliably accepts vendor commands at 115200 baud.

**Required workaround:** keep `BT_POWER_ON_SETTLING_TIME_MS = 1500`.

---

### Failure 3 — 50 ms delay between DOWNLOAD_MINIDRIVER and first WRITE_RAM is required at 115200 baud

**Recommendation:** remove the 50 ms `k_msleep()` inserted after `DOWNLOAD_MINIDRIVER` and before the first `WRITE_RAM` chunk.

**Observed failure:** first `WRITE_RAM (0xFC4C)` returns HCI status `0x12` ("Invalid HCI Command Parameters").

**Analysis:** after receiving `DOWNLOAD_MINIDRIVER`, the CYW55513 needs a short internal transition time before it is ready to accept `WRITE_RAM` chunks. Without the delay the host sends the first RAM chunk before the chip has finished switching to download mode. The symptom does not appear at higher baud rates (the inter-frame gap is larger), which is consistent with the Infineon claim that the workaround is unnecessary at 3 Mbaud — but since 3 Mbaud is blocked by Failure 1, it is still needed at 115200.

**Required workaround:** keep `k_msleep(50)` between `DOWNLOAD_MINIDRIVER` and the first `WRITE_RAM`.

---

### Failure 4 — SCO-route-to-PCM VSC (`WRITE_PCM_INT_PARAM 0xFC1C`) is mandatory in BLE-only mode

**Recommendation:** remove the `bt_update_sco_route()` call (vendor command `WRITE_PCM_INT_PARAM`, opcode `0xFC1C`) from `bt_h4_vnd_setup()`, on the grounds that it is not needed in a LE-only design.

**Observed failure (after successfully downloading the HCD firmware):**

```
<wrn> bt_hci_core: opcode 0x0c33 status 0x12
<err> app: bt_enable failed: -22
```

**Analysis:**

Opcode `0x0C33` is `HCI_Host_Buffer_Size` (OGF=3 Control & Baseband, OCF=0x33). Zephyr's BT host sends this command during initialisation to declare the host's ACL and synchronous buffer sizes. In a BLE-only build (`CONFIG_BT_BREDR=n`) the synchronous fields are set to zero.

When the SCO audio route is left at its power-on default (Transport), the CYW55513 controller rejects `HCI_Host_Buffer_Size` with status `0x12` because the zero synchronous packet count is inconsistent with a Transport-routed SCO path. Re-routing SCO to PCM via `WRITE_PCM_INT_PARAM (0xFC1C)` before `HCI_Host_Buffer_Size` is sent eliminates the rejection.

The Infineon ACS-1852 feedback stated this VSC is "not needed in LE-only design". This holds if the controller firmware suppresses the SCO route check when no SCO connections are active, but the behaviour observed here shows the check fires unconditionally during HCI init. The fix may depend on the firmware version or on a different initialisation order.

**Required workaround:** keep `bt_update_sco_route()` (`WRITE_PCM_INT_PARAM 0xFC1C`, zero-initialised parameters) at the end of `bt_h4_vnd_setup()` when `CONFIG_BT_CYW555XX=y`.

**Open question for Infineon:** is there a firmware version or Kconfig flag that suppresses the SCO route check during `HCI_Host_Buffer_Size` when no SCO connections are configured?