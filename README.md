# alif-sample-app

This repository is used to highlight features and bugs related to ALIF's E7 devkit.
Each of them will be in its own branch, and there is a template branch to start from.

## This branch: CPU caches left disabled by `CONFIG_INIT_ARCH_HW_AT_BOOT`

On both cores, an application built with `CONFIG_INIT_ARCH_HW_AT_BOOT=y` runs with the I-cache and
the D-cache off, although `CONFIG_ICACHE=y` and `CONFIG_DCACHE=y`. Every application booted by
MCUboot has this option.

Every few seconds the HE and HP applications log `SCB->CCR` and the time of a 4 KB `memcpy` between
two buffers of a cacheable SRAM: SRAM0 for HE, SRAM1 for HP ([`project/apps/common/cache_report.hpp`](project/apps/common/cache_report.hpp)). The
buffers are placed there on purpose: the default RAM of these targets is the DTCM, which is never
cached and would show no difference. Built with and without one patch of the Alif SoC support
(`zephyr_alif` v2.3.0), they show what it fixes.

### The issue

The application's own reset path turns the caches off, not the bootloader:

1. `soc_reset_hook` ([`soc/alif/ensemble/common/startup.S`](https://github.com/alifsemi/zephyr_alif/blob/v2.3.0/soc/alif/ensemble/common/startup.S))
   calls the HAL `SystemInit()`, which enables both caches.
2. `reset.S` then calls `z_arm_init_arch_hw_at_boot()` when `CONFIG_INIT_ARCH_HW_AT_BOOT` is set.
   That function puts the core back in its reset state and so disables both caches
   (`arch/arm/core/cortex_m/scb.c`).
3. Nothing enables the caches again: the Ensemble SoC has no `soc_early_init_hook`, and
   `arch_cache_init()` does nothing on Cortex-M.

`CONFIG_BOOTLOADER_MCUBOOT` implies `CONFIG_INIT_ARCH_HW_AT_BOOT` (`modules/Kconfig.mcuboot`), and so
does TF-M. MCUboot also disables the caches before it jumps to the application
(`CONFIG_BOOT_DISABLE_CACHES`), but that plays no part, since `SystemInit()` enables them again in
step 1. Without `CONFIG_INIT_ARCH_HW_AT_BOOT`, the caches stay on.

The `zephyr_alif` `main` branch has the same reset path, and so does the Ensemble port in upstream
Zephyr: there `soc_reset_hook` enables the caches, and `z_arm_init_arch_hw_at_boot()` runs right
after it.

### The patch

| Patch | Fix |
| ----- | --- |
| `alif-ensemble-enable-caches-at-boot.patch` | `RTSS_HE` and `RTSS_HP` select `SOC_EARLY_INIT_HOOK`, and `soc_early_init_hook()` enables both caches. The hook runs after `z_arm_init_arch_hw_at_boot()` and before the `PRE_KERNEL_1` drivers, so every driver initialises with the caches on, whether `CONFIG_INIT_ARCH_HW_AT_BOOT` is set or not. |

It is described in [`project/zephyr/patches.yml`](project/zephyr/patches.yml). Balletto has the same
reset path and is not covered.

### Running the demonstration

1. Select the "HE & HP with MCUboot" configuration (*Configure* task), or "HE only" / "HP only" with
   MCUboot.
2. Build without the fix: `./project/tools/scripts/update.py --without-fixes`, then build and flash
   (*Build* and *Flash* tasks). Read the console of each core.
3. Build with the fix: `./project/tools/scripts/update.py`, rebuild and flash.
4. For reference, select the same cores without MCUboot, rebuild and flash.
5. To rule MCUboot out, build each core without MCUboot but with `CONFIG_INIT_ARCH_HW_AT_BOOT`,
   with and without the fix, and flash it with the "HE only" or "HP only" configuration (`hp_app`
   and `rtss_hp` for HP):

   ```bash
   west build -p -b alif_e7_dk/ae722f80f55d5xx/rtss_he project/apps/he_app -- -DCONFIG_INIT_ARCH_HW_AT_BOOT=y
   ```

### Results on an E7 DK

| Build | HE: `SCB->CCR` | HE: 4 KB copy in SRAM0 | HP: `SCB->CCR` | HP: 4 KB copy in SRAM1 |
| ----- | -------------- | ---------------------- | -------------- | ---------------------- |
| MCUboot, without the fix | `0x00080611`: I-cache OFF, D-cache OFF | 411.1 µs | `0x00080611`: I-cache OFF, D-cache OFF | 2213.8 µs |
| MCUboot, with the fix | `0x000b0611`: I-cache on, D-cache on | 77.0 µs | `0x000b0611`: I-cache on, D-cache on | 30.8 µs |
| Without MCUboot (no `CONFIG_INIT_ARCH_HW_AT_BOOT`) | `0x000b0611`: I-cache on, D-cache on | 77.0 µs | `0x000b0611`: I-cache on, D-cache on | 30.8 µs |
| Without MCUboot, `CONFIG_INIT_ARCH_HW_AT_BOOT=y`, without the fix | `0x00080611`: I-cache OFF, D-cache OFF | 411.1 µs | `0x00080611`: I-cache OFF, D-cache OFF | 2213.8 µs |
| Without MCUboot, `CONFIG_INIT_ARCH_HW_AT_BOOT=y`, with the fix | `0x000b0611`: I-cache on, D-cache on | 77.0 µs | `0x000b0611`: I-cache on, D-cache on | 30.8 µs |

Without MCUboot, `CONFIG_INIT_ARCH_HW_AT_BOOT=y` alone turns the caches off: MCUboot plays no
part. With the fix, both cores run exactly as they do without this option.

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