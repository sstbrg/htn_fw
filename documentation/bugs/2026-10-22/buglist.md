**Firmware robustness and self-recovery bug list — 2026-10-22**

Scope: Tag and Anchor application firmware in the reviewed working tree. Bootloader work is excluded at the user's request. All six findings below are open; this report does not implement fixes.

The behavior contract is [HITAN firmware changes](../../HITAN_firmware_changes.html), including its **Implemented in anc3.2.0 / tag3.2.0 (2026-10-22)** clarification. That clarification takes precedence over earlier proposals in the document. Recovery changes must preserve the documented legacy frame formats, capability negotiation, and default-off feature switches.

The main recovery gap is that the software watchdog measures main-loop execution, while several failures leave that loop running with a disabled radio.

1. **REC-001 — Interrupt-disabled hangs bypass the watchdog. Severity: High. Affects: Tag and Anchor.**

   The watchdog countdown runs inside `TIM4_IRQHandler()`. Both `Error_Handler()` implementations disable interrupts and then loop forever, preventing the countdown from causing a reset. The application does not configure an independent hardware watchdog. Anchor `Lora_Status()` creates another vulnerable path by disabling interrupts around `Radio.GetStatus()`, whose SPI operations use `HAL_MAX_DELAY`.

   Trigger: enter `Error_Handler()`, or stall a blocking SPI operation while interrupts are disabled. Recovery then requires an external reset or power cycle. A main-loop stall with TIM4 interrupts still running is a different case and remains covered by the existing countdown.

   Evidence: [Tag watchdog](../../../Tag/Core/Src/stm32u5xx_it.c#L289), [Tag error handler](../../../Tag/Core/Src/main.cpp#L583), [Anchor error handler](../../../Anchor/Core/Src/main.cpp#L740), [Anchor LoRa status](../../../Anchor/Core/Lora/Src/Lora.cpp#L67), [blocking SPI reads](../../../Anchor/Core/Lora/Inc/Sx126x_Lib/sx126x-hal.cpp#L81).

   Recommended correction: provide interrupt-independent recovery and bound critical-section SPI operations. Check watchdog integration against existing FOTA, low-power, and update-entry behavior. Acceptance: inject an interrupt-disabled stall and verify a bounded reset and return to normal operation.

   Validation: source inspection; no hardware hang injection performed.

2. **REC-002 — Missing LoRa completion callbacks can stop transmission indefinitely. Severity: High. Affects: Tag and Anchor.**

   Starting a transmission clears `txDone` and `txTimeout`. The TX-idle predicates depend entirely on those callback flags, with no elapsed-time recovery deadline. If a completed transmission's interrupt is lost, or a radio reset prevents the expected completion event, the predicates remain false. The main loop continues refreshing the watchdog while further transmissions remain blocked.

   Evidence: [Tag TX start](../../../Tag/Core/Lora/Src/Lora.cpp#L220), [Tag idle predicate](../../../Tag/Core/Lora/Src/Lora.cpp#L349), [Tag main-loop gate](../../../Tag/Core/Src/main.cpp#L286), [Anchor idle predicate](../../../Anchor/Core/Lora/Src/Lora.cpp#L284), [Anchor main-loop gates](../../../Anchor/Core/Src/main.cpp#L516).

   Recommended correction: track a transmission deadline independently of the completion IRQ. On expiry, inspect and clear pending radio status, then reset/reconfigure the radio when needed and restore a valid application state. Allow enough time for the configured packet airtime.

   Validation: a host simulation of the Anchor idle predicate remained false through 10,000 calls with both completion flags cleared. Source inspection confirms the equivalent Tag predicate and the absence of an application deadline. Acceptance: suppress a TX completion callback and verify that subsequent transmissions resume within the defined recovery bound.

3. **REC-003 — A failed Tag UWB status read permanently latches `no_uwb`. Severity: High. Affects: Tag.**

   `dwmStatus()` sets `dwmInfo.no_uwb = true` when the device ID is invalid, but a subsequent valid ID does not clear it. The main loop skips ranging while this flag is set. `dwmInit()` clears the flag on successful initialization, but there is no automatic reinitialization path in the normal loop.

   Trigger: one transient failed device-ID read during a status request. Even after communication with the DW1000 recovers, ranging remains disabled until reset or explicit successful reinitialization. Anchor's status function does clear its flag after a valid read, so this particular latch defect is Tag-specific.

   Evidence: [Tag status and initialization](../../../Tag/Core/Src/dw1000.cpp#L87), [ranging gate](../../../Tag/Core/Src/main.cpp#L291), [status request](../../../Tag/Core/Src/main.cpp#L346).

   Recommended correction: distinguish transient status-read failures from persistent initialization failures and implement bounded reinitialization with configuration restoration. Acceptance: inject an invalid ID followed by valid reads and verify that ranging resumes automatically.

   Validation: a host simulation called the actual status function with an invalid ID, then the correct ID; `no_uwb` remained true.

4. **REC-004 — Anchor LoRa RX errors remain latched after reception restarts. Severity: Medium. Affects: Anchor.**

   `OnRxError()` sets `radioFlags.rxError`, and `Lora_RXIdle()` treats that flag as a reason to service RX. `Lora_Operation_RX()` clears `rxDone` and `rxTimeout`, but never clears `rxError`. After an RX error, normal main-loop passes repeatedly reconfigure and restart reception, disrupting packet reception while continuing to feed the watchdog.

   Evidence: [RX operation](../../../Anchor/Core/Lora/Src/Lora.cpp#L77), [error callback](../../../Anchor/Core/Lora/Src/Lora.cpp#L201), [RX-idle predicate](../../../Anchor/Core/Lora/Src/Lora.cpp#L288), [main-loop restart](../../../Anchor/Core/Src/main.cpp#L539).

   Recommended correction: consume the RX error once as part of a defined recovery transition, clear the relevant software and hardware events, and restart reception once. Acceptance: inject `OnRxError()` and verify that a later valid packet is received without continuous RX restarts.

   Validation: with `rxError` set and TX idle, a host simulation observed 100 RX restarts in 100 loop passes; the error flag remained set. This tests an invoked error callback, not the frequency of such errors on the configured PHY.

5. **REC-005 — Absolute backoff deadline comparisons fail at tick rollover. Severity: Medium. Affects: Anchor.**

   Backoff arming, polling gates, remaining-time reporting, and coexistence detection compare an absolute deadline directly with `_1ms_tick`. An expired deadline retained across rollover can therefore appear to be far in the future. Polling can remain suppressed while the watchdog continues to be refreshed; coexistence detection can also remain active.

   Reproduction: retain `backoff_timer = 0xFFFFFFF0`, then advance `_1ms_tick` to `0x00000010`. The deadline has expired, but `backoff_timer > _1ms_tick` is true and the reported remaining time becomes 4,294,967,264 ticks.

   Evidence: [backoff arming](../../../Anchor/Core/Src/dw1000.cpp#L225), [polling gate](../../../Anchor/Core/Src/main.cpp#L322), [remaining-time calculation](../../../Anchor/Core/Src/uart_communication.c#L26), [coexistence predicate](../../../Anchor/Core/Src/dw1000.cpp#L599).

   Recommended correction: use a consistent wrap-safe time comparison with a separate active-state representation where needed. Acceptance: test deadlines before, across, and after rollover, including the inactive zero value; verify polling and coexistence recovery remain bounded as documented.

   Validation: host simulation reproduced the incorrect remaining time and blocked polling condition.

6. **REC-006 — The nominal 1 ms timer period is approximately 2.05 ms. Severity: Medium. Affects: Tag and Anchor.**

   Both projects configure TIM4 with prescaler `1` and auto-reload value `160000`; their configured timer clock is 156 MHz. The resulting period is `(1 + 1) * (160000 + 1) / 156000000`, or approximately **2.051295 ms**. `_1ms_tick` consequently does not represent milliseconds.

   Impact: the 1,000-tick watchdog interval is approximately **2.05 seconds**, and a nominal 209 ms backoff lasts approximately **429 ms**. Other durations using this counter are similarly extended. Timeouts using `HAL_GetTick()` are separate and are not affected by this calculation.

   Evidence: [Tag timer configuration](../../../Tag/Core/Src/tim.c#L84), [Anchor timer configuration](../../../Anchor/Core/Src/tim.c#L84), [Tag timer clock](../../../Tag/Tag.ioc#L529), [Anchor timer clock](../../../Anchor/Anchor.ioc#L533), [watchdog reload](../../../Tag/Core/Src/stm32u5xx_it.c#L334).

   Recommended correction: derive a true 1 kHz update frequency from the configured timer clock, keep generated code and project configuration consistent, and review consumers of `_1ms_tick`. Recheck the documented backoff and host timing relationship before deployment. Acceptance: measure the interrupt period and representative watchdog/backoff intervals on hardware.

   Validation: calculation from the current clock and timer configuration; no physical timing measurement performed.

**Validation context**

The preceding review successfully built the Tag and Anchor applications. Host checks used selected firmware functions with stubbed peripherals; they do not establish full-device recovery. The Tag power-control handshake, 500 ms fallback to the previous power, 1 s fallback to maximum, tick rollover, and coexistence fallback passed those checks. Hardware fault injection remains outstanding.

The [FOTA documentation](../../../tools/htn_fota/README.md) explicitly describes loss of power during flash writing as requiring wired recovery. That documented limitation is not counted as a new application bug here. Recommendations above are proposed follow-up work; the firmware remains unchanged by this report.

**Resolution — anc3.2.0 / tag3.2.0 (2026-10-22)**

All six fixed and flashed on the bench (7 anchors, tag 19). Bootloader untouched.

1. **REC-001:** `Error_Handler()` resets (`HAL_NVIC_SystemReset`) instead of looping with interrupts off. Anchor `Lora_Status()` masks only the DIO1 interrupt (EXTI4) rather than all interrupts, so the TIM4 watchdog keeps running if BUSY never drops. EXTI4 priority moved from 0 to 1 (main.cpp and .ioc), so TIM4 preempts a radio handler stuck in its BUSY wait. The fault handlers already reset; the DW1000 SPI receive (the only other masked section) has its own spin limits. Not done: an independent hardware IWDG. It keeps running through a reset into the bootloader, which does not refresh it, so it needs bootloader work first.
2. **REC-002:** TX (tag and anchor) and RX (anchor) record their start tick. No completion event for 3 s — the radio's own timeouts are ~0.78 s — runs `Radio.Init()` + reconfiguration (current TX power kept) and lets the main loop restart TX/RX.
3. **REC-003:** `no_uwb` is set by a bad ID read, but a good ID read only clears it if the chip initialised (`dw_init_ok`). `dwRecover()` in the main loop retries every 5 s while `no_uwb` is set: an initialised chip that answers clears it, anything else gets `dwmInit()` + `dwConfig()`. Both boards; the anchor previously cleared `no_uwb` on a valid ID even if initialisation had failed.
4. **REC-004:** `Lora_Operation_RX()` clears `rxError`; one restart per error.
5. **REC-005:** `Backoff_Left()` compares wrap-safe (`(int32_t)(deadline - now)`), clears expired or implausible (> 2 s) deadlines and runs every main-loop pass, so a stale deadline cannot come back after the wrap. Arming never stores 0. All gates, the STATUS report and 9K coexistence use it. Same fix in the link-adaptation hello hold (now elapsed-based) and the tag's LoRa wake-up wait.
6. **REC-006:** TIM4 prescaler 155, period 999 → exactly 1 kHz (was `(1+1)·(160000+1)/156 MHz` = 2.051 ms: the prescaler register divides by value + 1, and the period assumed 160 MHz). Real timings kept: watchdog 2051 ticks (2.05 s), anchor RX wait 144 ms, backoff defaults 410/21 ms (21–430 ms as before). The opcode-19 `{top, min}` bytes are read in the old 2.051 ms units, so host settings keep their real timing. The STATUS backoff field is now in true ms (it was 2.05 ms units).
   Measured: backoff countdown 0.49 → 1.00 tick per host ms; `HAL_GetTick` 1.0000 ms per ms on two anchors.

**Clock audit (both boards identical):** HSE 12 MHz (confirmed by the TIM4 measurement) → PLL1 M1 N13 R1 = 156 MHz SYSCLK, VOS1, 4 wait states, buses ÷1. USART 921600 → 923077 baud (+0.16 %). SPI3 DW1000 19.5 MHz (2.44 MHz for init), SPI2 SX1262 9.75 MHz, SPI1 BMI088 9.75 MHz, I2C2 ~100 kHz, ADC on HSI. The RTC runs from LSI ÷128 (250 Hz) with 32 kHz prescalers, so its calendar would be 128× slow, but only the TAMP backup registers are used. The UWB time base is the DW1000's own 38.4 MHz crystal, independent of all of the above. Measured tag-vs-anchor crystal offsets +3.1 … −1.7 ppm, corrected by the carrier integrator to ±0.01 ppm (< 1 cm); range SD 6 cm behind a wall, 3.4 cm at 1 m line of sight.

**Found while fixing (2026-10-22)**

- **Tag flash writes were read back stale.** The tag's `Flash_Write()` did not invalidate ICACHE, unlike the anchor's (`Update_Cash_Mem()`). Reads of the self-id page kept showing the old data until a reset; a new tag id from `SET_ID` only appeared after a reset, for example. Fixed in tag3.2.0.
- **DW1000 crystal trim (new opcode 23 `XTAL_TRIM`, self-id page slot 3, applied after `dwt_initialise`; `SET_ID` keeps it).** On the Rev D board the full trim range 0x00–0x1F moves the crystal by only ~0.3 ppm, so it cannot close the ~1.6 ppm gap to the Rev C tag. It was left at the default. The carrier-integrator correction keeps the residual below 1 cm either way.
