# zanytemp — Claude Instructions

## Board
- **Seeed XIAO nRF54L15** target: `xiao_nrf54l15_nrf54l15_cpuapp`
- NCS v3.2.3 at `C:/ncs/v3.2.3`
- Zephyr RTOS project (`find_package(Zephyr)`)

## Environment setup
Always source the nRF environment before running any `west` command:
```bash
source ~/nrf_env.sh
```

## Build
```bash
source ~/nrf_env.sh && west build
```
Force a clean rebuild:
```bash
source ~/nrf_env.sh && west build --pristine
```

## Flash
```bash
source ~/nrf_env.sh && west flash
```