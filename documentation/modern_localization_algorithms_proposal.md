# Proposal: Improving Localization with Existing LoRa and UWB Hardware

**Project:** `hitan_rust`  
**Date:** 21 October 2026  
**Status:** Proposed research and evaluation  
**Hardware scope:** Existing LoRa radios, UWB radios, and installed antennas only

## Recommendation

Improve LoRa direction finding through measured antenna-pattern calibration and probabilistic angle estimation, feeding the LoRa arc only. Extend the existing robust smoother to account for measurement timing and to estimate or compensate for non-line-of-sight (NLOS) ranging bias.

**Standing rule:** the UWB needle and the LoRa arc are separate indicators. LoRa evidence never drives, seeds or nudges the UWB bearing, and UWB never steers the arc. No fusion of the two is proposed or planned (decision reaffirmed 21 October 2026).

These changes address specific limitations in the current measurement model. Their benefit must be established against the existing Polar Kalman filter and fixed-lag smoother using recorded data and controlled experiments.

The algorithms below are candidates for evaluation. Publication results do not establish expected accuracy or runtime on this project's hardware, anchor geometry, or radio configuration.

## Scope: software improvements on the installed hardware

The installed LoRa radios, UWB radios, and antennas are the complete hardware basis for this work. All proposed changes are software, calibration, recording, or evaluation changes using the existing system.

Inputs are the UWB ranges and diagnostics, LoRa RSSI/SNR, and timing and freshness information available through the existing [radio interface](../crates/hitan-core/src/protocol/replies.rs). Algorithms must work with these measurements.

The goal is a more useful direction-and-distance estimate through calibrated LoRa amplitude comparison and UWB range estimation. The installed patches primarily cover the front semicircle; software should preserve ambiguous directions and report low confidence when the measurements cannot distinguish them. Accuracy and angular coverage must be established using the actual rig.

All tracking remains relative to the station. Rotating or moving the station does not provide a known global pose, so combining observations into a geographic map is outside this proposal.

## Current implementation

The current working tree already includes:

- **Least-squares trilateration**, with propagation of range uncertainty into bearing uncertainty: [`solve.rs`](../crates/hitan-core/src/trilateration/solve.rs).
- **Separate constant-velocity Kalman filters for bearing and range**, with innovation gates, motion constraints, and a two-model (still/moving) interacting multiple model for standing still: [`bearing_filter.rs`](../crates/hitan-core/src/trilateration/bearing_filter.rs).
- **Per-exchange UWB quality gates and variance inflation**, based on first-path diagnostics and consistency between anchor ranges: [`uwb_quality.rs`](../crates/hitan-core/src/uwb_quality.rs).
- **A robust fixed-lag smoother** operating on raw anchor ranges, using Gauss–Newton optimization with damping and Huber or Cauchy losses: [`smoother.rs`](../crates/hitan-core/src/trilateration/smoother.rs).
- **LoRa amplitude-comparison direction finding**, using a weighted circular mean of patch boresights, RSSI offsets, and SNR weighting: [`lora_bearing.rs`](../crates/hitan-core/src/trilateration/lora_bearing.rs). Its calibration currently describes offsets and boresights, rather than full measured antenna response curves.

The selectable estimators are `Raw`, `PolarKalman` (the default), and `Smoother`, as defined in [`config.rs`](../crates/hitan-analyzer/src/trilat_pipeline/config.rs). The [estimator document](polar_kalman_and_polar_ekf.html) explains the Polar Kalman, the still model and the smoother in plain language, and why the Polar EKF was dropped.

## Candidate algorithms

The dates refer to the cited research, not necessarily the invention of the underlying algorithm family. The suitability assessments are based on comparison with the project code; they have not been benchmarked on this system.

| Candidate | What it adds | Applicability and limitations |
| --- | --- | --- |
| **Calibrated antenna-pattern matching with a Bayesian angular grid** | Scores possible bearings against measured per-antenna RSSI response curves and retains an angular distribution over time. | First software experiment for the existing LoRa hardware. This is an established estimation approach proposed for this project, not a claim of a newly published algorithm. Needs calibration at known angles and validation across tag orientations, distances, and environments. |
| **Heteroscedastic Bias-Robust Projected Gradient Descent — HBR-PGD (2026)** | Separates changing measurement uncertainty, persistent link bias, and occasional NLOS errors. Jointly estimates trajectory and bounded bias states over sliding windows. [Research paper](https://www.mdpi.com/1424-8220/26/14/4578) | A strong architectural match for the existing smoother. The current robust losses downweight large residuals but do not estimate persistent environmental bias. With only three anchors, additional bias variables require strong constraints so that they do not absorb real motion. |
| **Compensated-Ranging Weighted Least Squares — CR-WLS (2025)** | A neural network predicts a correction and weight from range and received-power measurements, without requiring full channel impulse response (CIR) traces. [Research paper](https://repository.sutd.edu.sg/esploro/outputs/journalArticle/Localization-through-mitigating-and-compensating-UWB/9911370309846) | A practical learning-based candidate because the project already exposes corresponding first-path and total-power diagnostics. Its correction stage could feed the existing smoother. Requires representative labelled recordings and validation on unseen sessions and environments. |
| **Adaptive Student's-t EKF with Allan variance (2025)** | Uses heavy-tailed measurement noise and adapts noise estimates over time. [Research paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC11946433/) | Worth evaluating when output delay is important. The existing Cauchy loss already provides heavy-tail robustness, so the additional value would mainly be noise adaptation. A symmetric heavy-tailed model alone does not correct persistent positive NLOS bias. |
| **Continuous-time estimation using non-uniform B-splines — CT-UIO (2025)** | Represents motion continuously, allowing measurements taken at different times to constrain one trajectory. The published system also uses IMU and odometry. [Research paper](https://arxiv.org/abs/2502.06287) | A reference for measurement timing only. Adapt the timing principle to existing radio observations; the complete IMU/odometry system is outside the hardware scope. Its reported performance cannot be assumed for the radio-only adaptation. |
| **Square-root marginalization for sliding-window estimation (2021)** | Preserves information from departing states in a compact prior using QR-based numerical methods. [Research paper](https://openaccess.thecvf.com/content/ICCV2021/html/Demmel_Square_Root_Marginalization_for_Sliding-Window_Bundle_Adjustment_ICCV_2021_paper.html) | A possible improvement to the existing smoother, which currently removes old nodes without a marginalization prior. Could improve continuity and numerical handling. It does not directly correct bad ranges, and the cited evaluation concerns visual and visual-inertial estimation. |

## Proposed experiment 1: Calibrated LoRa direction finding

Replace the current weighted-boresight heuristic experimentally with a measured model of how the installed antennas respond to direction.

1. **Collect calibration data at known relative angles.** Use the existing rig and tag at marked positions. Record fresh per-anchor RSSI/SNR, radio settings, tag orientation, and measurement timing. Repeat at representative distances and orientations, including angles behind the rig to characterize misleading back-lobe responses.
2. **Fit a response curve for each antenna.** Start with an interpolated lookup table and measured variation at each angle. Use the radio's verified RSSI encoding consistently; raw register counts should not be assumed to be dBm.
3. **Compare relative signal levels.** For each candidate angle, fit a shared level offset before scoring how well the antenna pattern matches the measurements. This reduces sensitivity to common transmit-power and path-loss changes. It does not remove antenna-specific fading or orientation effects.
4. **Maintain an angular probability distribution.** Use a small grid over candidate bearings and a motion prior. Preserve multiple plausible directions instead of immediately averaging them into a potentially misleading intermediate angle. Grid spacing is a computational choice, not an accuracy guarantee.
5. **Respect measurement quality and freshness.** Widen or withhold the estimate for weak signals, saturation, missing anchors, stale reports, or patterns unlike the calibration data. Repeated held or smoothed values must not count as independent new evidence. Apply temporal updates only when supported by fresh observations.

Calibrate confidence using held-out recordings. A narrow peak from the model is not sufficient evidence that its bearing is accurate. Keep the existing LoRa estimator as the comparison baseline and fallback until the new model demonstrates a benefit.

## Proposed experiment 2: Individual measurement timestamps

The [trilateration worker](../crates/hitan-analyzer/src/worker/trilat.rs) ranges anchors sequentially. The current estimator groups their measurements into a single round, and the smoother assigns those measurements to one state.

When the target or station moves between exchanges, part of the range difference can come from elapsed time. Treating that difference as simultaneous geometry can affect the estimated bearing.

Proposed work:

1. Record host request/response timing for each anchor measurement and document its relationship to the actual radio exchange, including timing uncertainty. A host receipt time is not an exact RF measurement timestamp. For LoRa, distinguish fresh reports from previously held values; uncertain report age must remain explicit.
2. Preserve these timestamps through recording, playback, and estimation. Retain a defined fallback for older recordings that contain only a round timestamp.
3. Evaluate each range against the predicted state at its measurement time.
4. Start with a simple motion model. Evaluate a continuous-time spline only if the simpler implementation leaves a measurable limitation.

Use controlled motion and synthetic timing offsets to distinguish timing errors from ranging noise and NLOS bias. This experiment does not require adopting the full CT-UIO sensor-fusion system.

## Proposed experiment 3: NLOS bias estimation or learned correction

Compare two approaches using the same evaluation data:

- **Constrained bias estimation:** Extend the smoother with bounded, slowly varying link-bias states and quality-dependent uncertainty, drawing on HBR-PGD. Keep environmental bias distinct from the existing static antenna-delay calibration. Use priors or regularization to limit ambiguity between position and bias.
- **Learned range correction:** Train a small CR-WLS-style model using range and available UWB diagnostics. Evaluate its correction and weight outputs as inputs to the existing smoother. Any conversion from a learned weight into measurement variance must be calibrated and checked.

The existing [`nlos_features.rs`](../crates/hitan-core/examples/nlos_features.rs) example provides a starting point for assessing diagnostic features against truth-labelled recordings. New data should cover the current radio configuration and representative operating conditions.

Split learning datasets by recording session and environment. Adjacent samples from the same run must not be divided between training and evaluation, because that can overstate generalization.

## Evaluation and adoption criteria

Compare each candidate against the current Polar Kalman filter, fixed-lag smoother, and LoRa bearing estimator, using identical inputs and consistent calibration. Evaluate the LoRa arc and the UWB needle separately; they are separate instruments. Compare estimates at the times they represent, and report output delay separately.

| Measure | Purpose |
| --- | --- |
| Bearing and range error | Measure accuracy against independent ground truth. |
| Median and 95th-percentile error | Capture typical performance and large failures. |
| Output delay and response to motion | Identify improvements obtained at the cost of slower tracking. |
| Dropout recovery and time without a usable bearing | Measure behavior when anchors disappear or measurements are rejected. |
| Uncertainty calibration | Check whether reported uncertainty corresponds to observed errors. |
| Runtime and memory on the target device | Confirm that estimation fits the live processing budget. |

Include stationary targets, starts and stops, station turns, passing targets, partial anchor loss, intermittent NLOS, and persistent per-anchor bias. Include varied measurement intervals, stale LoRa reports, tag orientations, receiver saturation, and anchor quality. Use marked positions and known relative directions for initial ground truth; no additional navigation sensor is required for those tests.

Adoption should require a demonstrated improvement in the intended scenario, acceptable latency and resource use, and no material regression in clean line-of-sight conditions. Numerical thresholds should be set before comparing experimental results.

## Suggested priority

1. Establish the common evaluation baseline, preserve measurement timing, and record fresh LoRa observations at known relative angles.
2. Evaluate individual measurement timing with a simple motion model (the LoRa freshness work in item 1 depends on it).
3. Evaluate calibrated antenna-pattern matching against the existing LoRa bearing heuristic, feeding the arc only.
4. Compare constrained NLOS bias estimation with learned range correction if persistent UWB errors remain a limiting factor.
5. Evaluate adaptive Student's-t filtering or square-root marginalization only when measured delay, historical information loss, or numerical behavior justifies the added complexity.

This proposal covers research and evaluation. It does not establish that any candidate is already superior to the current implementation.
