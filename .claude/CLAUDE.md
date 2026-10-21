@RTK.md

# HITAN firmware: Anchor, Tag, bootloader

Three STM32CubeIDE projects for the STM32U5A5ZJTxQ (Cortex-M33). Each has its
version string in `<Project>/Core/Inc/main.h` (`#define VERSION`).

- `Anchor/`: anchor firmware (DW1000 UWB + SX126x LoRa).
- `Tag/`: tag firmware.
- `bootloader_hitan/`: bootloader in flash bank 1 (0x08000000).

## Build
`make -C <Project>/Debug -j16 all` with arm-none-eabi-gcc. The generated Debug
makefiles are tracked; build outputs are gitignored. Bump `VERSION` in
`main.h` for every firmware you hand out.

## Flash
- The apps are linked at **0x08200000** (bank 2, `STM32U5A5ZJTXQ_FLASH_MMT_TEMPLATE.ld`).
  Flash the `.hex` (it carries its addresses). Never write an app `.bin` to
  0x08000000: that overwrites the bootloader.
- ST-Link: `Anchor/flash_anchor.sh` (st-flash, stlink-tools >= 1.8.0). Ubuntu
  OpenOCD 0.12.0 has no U5A5 flash driver.
- Ask before every flash. Burning hardware is the user's call.

## Serial
The anchor and tag UARTs run at **921600** baud. Never open a port at any
other baud: with no `HAL_UART_ErrorCallback`, a framing error leaves the
anchor UART deaf until a power cycle. USB cables are flaky. On
`error -71` / failed enumeration, ask the user to replug.

## Delegation
- Git commits, pushes, PRs: the `git-ops` subagent (`.claude/agents/git-ops.md`).
- CI status: the `ci-watcher` subagent.

## vexp - Context-Aware AI Coding <!-- vexp v3.3.7 -->

### Context strategy: call run_pipeline ONCE at task start
If the task already names the files/symbols to touch, SKIP vexp. Otherwise one
`run_pipeline({ "task": "..." })` returns ranked pivot files with line ranges and
blast radius. Do NOT open files one by one to find your way around - every extra
tool call costs a turn. Call it again ONLY when the task moves to a new area.
`get_skeleton` for files to understand, not edit. `verify_done` before calling a
multi-file task complete, then RUN the tests it names.

### Query shape (do this)
Anchor the task on real identifiers (ClassName, functionName) or file paths:
`run_pipeline({ "task": "fix JWT expiry in AuthService.validateToken" })`

vexp runs entirely on this machine, index in `.vexp/`;
`run_pipeline` transmits nothing to any external service.
On `status: "degraded"` or 0 pivots the index is still building - use your own tools.
For literal string sweeps use your native search - do NOT route text sweeps through vexp.
Repo SOURCE only: ST HAL `Drivers/`, `Debug/`, `Release/` are NOT indexed.
