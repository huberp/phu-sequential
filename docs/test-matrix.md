# Test matrix (#3)

Spec: `spec/arp-spec.md` (wins on conflict). Modules `Mnn`: `docs/architecture.md` §3–§6.

## Conventions
- ID `REQ-<§>-<n>`. Catch2 tag `[REQ-<§>-<n>]` + module tag `[Mnn]`. One test may carry several IDs.
- Defaults (§20) unless stated: 120 bpm, 48 kHz → 1 beat = 24000 samples; 1/16 step = 0.25 beat = 6000 samples; 16 steps, gate 100, out ch 1, transpose 0, Thru off, Consume on, chord in ch 1.
- Notation: `NOn(ch,pitch,vel)`, `NOff(ch,pitch)`, `@b` = beat, `@s` = sample (absolute), `L1..L8` = lanes (L1 = chord-follow idx 0 if unspecified), `T+`/`T-` = transport start/stop, `loc(b)` = locate.
- Status: `todo` = test not yet written; `n/t` = not testable (reason). Owner issue ticks `todo` → `done` in its PR.
- Invariants after every scenario (arch §11): each Note-On has one Note-Off by stop/bypass flush; offsets non-decreasing; 0 allocations in `processBlock`.

## §3 Input

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-3.1-1 | 3.1 | Chord = held notes; Note-On adds, Note-Off removes | M07 | NOn 60, 64, 67; NOff 64 → chord {60,67} | #7 | todo |
| REQ-3.1-2 | 3.1 | Note-On vel 0 = Note-Off | M07 | NOn(1,60,100); NOn(1,60,0) → chord empty | #7 | todo |
| REQ-3.1-3 | 3.1 | Pitch at most once; repeat Note-On replaces | M07 | NOn 60 vel 50; NOn 60 vel 90 → size 1, vel 90 | #7 | todo |
| REQ-3.1-4 | 3.1 | Pedal per channel: held after Note-Off while CC64 ≥ 64 | M07 | CC64=127 ch1; NOn/NOff 60 ch1 → still in chord; same on ch2 w/o pedal → gone | #7 | todo |
| REQ-3.1-5 | 3.1 | Pedal off (<64) releases all unheld notes | M07 | continue REQ-3.1-4; CC64=63 → chord empty; keys still down stay | #7 | todo |
| REQ-3.1-6 | 3.1 | Pedal logic equal for channel-mode lanes | M07,M08 | L2 channel 3; pedal ch3 on; NOn/NOff 60 ch3 → L2 keeps playing until pedal off | #7 | todo |
| REQ-3.1-7 | 3.1 | Threshold: 63 off, 64 on | M07 | CC64=63 → no hold; 64 → hold | #7 | todo |
| REQ-3.2-1 | 3.2 | Chord-follow uses only chord input channel | M07,M08 | chord ch=2; NOn(1,60), NOn(2,64) → chord {64} | #7 | todo |
| REQ-3.2-2 | 3.2 | Chord input channel range 1–16, change takes effect | M07,M20 | change ch 1→5 mid-play; chord rebuilt from ch5 held notes | #7,#13 | todo |
| REQ-3.2-3 | 3.2 | Channel mode independent per lane | M08 | L1 ch3, L2 ch5; NOn ch3, ch5 → each lane own note | #7 | todo |
| REQ-3.3-1 | 3.3 | Thru off: no input notes out; Thru on: all 16 ch passed | M16 | Thru off: NOn → no output. Thru on: NOn ch1..16 → 16 outputs | #11 | todo |
| REQ-3.3-2 | 3.3 | Consume on: followed channels not passed, even Thru on | M16 | Thru on, Consume on, chord ch1, L2 ch3: NOn ch1,3 blocked; ch2 passes | #11 | todo |
| REQ-3.3-3 | 3.3 | Consume off: followed channels pass if Thru on | M16 | same, Consume off → ch1,3 pass | #11 | todo |
| REQ-3.3-4 | 3.3 | Per-note tracking: Note-Off passed iff Note-On passed | M16 | Thru on NOn 60; Thru off; NOff 60 → NOff passed. Reverse: Thru off NOn; on; NOff → blocked | #11 | todo |
| REQ-3.3-5 | 3.3 | Tracking immune to Consume/follow-channel changes | M16 | NOn passed; Consume on + follow ch added; NOff → passed | #11 | todo |
| REQ-3.3-6 | 3.3 | Pedal does not alter pass-through Note-Off | M16 | Thru on; CC64 on; NOn/NOff 60 → NOff passed immediately, CC64 passed | #11 | todo |
| REQ-3.3-7 | 3.3 | CC, PB, AT, PC always passed, any Thru/Consume | M16 | Thru off Consume on: CC1, PB, AT, PC pass unchanged | #11,#12 | todo |
| REQ-3.3-8 | 3.3 | CC64 passed and interpreted | M16,M07 | CC64 ch1 → in output and hold active | #11 | todo |
| REQ-3.3-9 | 3.3 | CC120/CC123 only passed, no internal reset | M16,M11 | lane sounding; CC123 in → passed, lane notes unaffected | #11 | todo |
| REQ-3.3-10 | 3.3 | Thru, Consume automatable | M20 | layout contains params, in param list | #13 | todo |
| REQ-3.4-1 | 3.4 | Sort ascending; idx 0 = lowest | M07,M08 | NOn 67,60,64 → idx0=60, idx1=64, idx2=67 | #7 | todo |

## §4 Lanes

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-4-1 | 4 | Exactly 8 lanes | M01,M12 | `laneCount == 8`; config has 8 lanes | #5,#10 | todo |
| REQ-4-2 | 4 | Lane color: no effect on MIDI | M14 | two runs differing only in lane colors → identical output | #10,#14 | todo |
| REQ-4-3 | 4 | Lane color persistence: 8 colors saved/loaded exactly (opaque RGB) | M21,M22 | set L1..L8 distinct RGB; state save → load → same 8 values; preset save → load likewise | #14 | todo |
| REQ-4-4 | 4 | Lane color in state, not a host parameter | M20,M22 | color absent from APVTS param list; present in state XML | #13,#14 | todo |
| REQ-4-5 | 4/20 | Default palette: 8 distinct colors in lane order | M01,M21 | `defaultLaneColor(i)` = palette[i]; all 8 distinct | #5,#14 | todo |
| REQ-4-6 | 4/14 | Changing one lane's color leaves others unchanged | M21 | `setLaneColor(2, c)` → lanes ≠2 equal before | #14,#15 | todo |
| REQ-4-7 | 4/13 | Color migration: older state without colors → defaults; invalid/missing channel clamped | M22 | v1 fixture w/o colors → palette; color attr 300 → 255 | #14 | todo |
| REQ-4.1-1 | 4.1 | Lanes non-exclusive | M14,M17 | L1, L2 both idx 0, chord {60} → both output | #10,#11 | todo |
| REQ-4.2-1 | 4.2 | idx ≥ chord size → silent | M08,M14 | chord {60,64}, L idx 2 → no output | #7,#10 | todo |
| REQ-4.2-2 | 4.2 | Lane plays as soon as chord big enough | M08,M14 | idx 2, chord {60,64}; NOn 67 mid-step → plays from next step start | #7,#10 | todo |
| REQ-4.2-3 | 4.2 | Empty chord → silent | M08,M14 | no notes → no output | #7,#10 | todo |
| REQ-4.2-4 | 4.2 | Index larger than chord (edge, §18) | M08,M14 | idx 15 with chord of 3 → silent, no hang, no crash | #7,#10 | todo |
| REQ-4.2-5 | 4.2 | Chord re-sorting: release with others held → lane switches pitch at next step start | M08,M14 | chord {60,64,67}, idx 1 sounds 64; NOff 64 → idx 1 now 67; running note unchanged, next step plays 67; no Note-Off for 64 forced | #7,#10 | todo |
| REQ-4.2-6 | 4.2 | Re-sort is not a release | M14 | as REQ-4.2-5; no immediate stop | #10 | todo |
| REQ-4.2-7 | 4.2 | Source completely empty → immediate stop | M14 | chord {60}, idx 0 sounding; NOff 60 mid-step → Note-Off at that sample | #10 | todo |
| REQ-4.2-8 | 4.2 | Idx drops out of chord by release → immediate stop | M14 | chord {60,64}, idx 1 sounding; NOff 64 → immediate Note-Off | #10 | todo |
| REQ-4.2-9 | 4.2 | Follow idx range 0–15; out-of-range clamped | M12,M20 | 16 → 15; −1 → 0 | #10,#13 | todo |
| REQ-4.3-1 | 4.3 | Channel mode follows most recent note on channel | M08 | NOn 60 ch3, NOn 64 ch3 → plays 64 | #7 | todo |
| REQ-4.3-2 | 4.3 | No note stack: release of followed note stops lane even if older held | M08,M14 | NOn 60, NOn 64; NOff 64 → lane stops immediately, 60 not resumed | #7,#10 | todo |
| REQ-4.3-3 | 4.3 | Silent before first note on channel | M08 | no notes → no output | #7 | todo |
| REQ-4.3-4 | 4.3 | Release of older (non-followed) note has no effect | M08 | NOn 60, NOn 64; NOff 60 → still playing 64 | #7 | todo |
| REQ-4.4-1 | 4.4 | New note adopted at next step start | M14 | L1 idx 0 sounding 60 on step k; NOn 48 at step k+0.5 → step k keeps 60 until its end; step k+1 plays 48 | #10 | todo |
| REQ-4.4-2 | 4.4 | Source release overrides quantization | M14 | see REQ-4.2-7 | #10 | todo |
| REQ-4.5-1 | 4.5 | Position continues during silence | M13,M14 | chord empty for 2.0 beats; NOn at beat 2.1 → first note at next grid step (step 9 of 16) | #10 | todo |
| REQ-4.5-2 | 4.5 | Legato changes never affect position | M14 | chord changes at various times → note starts stay on grid | #10 | todo |
| REQ-4.5-3 | 4.5 | Mute does not stop position | M14 | mute 1 beat, unmute → next step is beat-aligned (step 5) | #10 | todo |
| REQ-4.5-4 | 4.5 | No reset-on-input | M14 | NOn mid-loop → no restart to step 1 | #10 | todo |
| REQ-4.6-1 | 4.6 | Latch off: release stops lane (§9.2) | M14 | see REQ-4.2-7 | #10 | todo |
| REQ-4.6-2 | 4.6 | Latch on: continue on last followed note after release | M14 | latch on; NOn 60; NOff 60 → lane keeps playing 60 | #10 | todo |
| REQ-4.6-3 | 4.6 | Latch on: new note updates followed note at next step start | M14 | latch; NOn 60; NOn 64 mid-step; NOff all → step+1 plays 64 | #10 | todo |
| REQ-4.6-4 | 4.6 | Latch ends on transport stop/mute/bypass/latch off: Note-Offs + memory cleared | M14,M17 | latch sounding; each of T-, mute, bypass, latch off → all lane Note-Offs at once; memory empty | #10,#11 | todo |
| REQ-4.6-5 | 4.6 | Latch param stays after mute/stop | M14 | mute → config.latch still on | #10 | todo |
| REQ-4.6-6 | 4.6 | After un-mute/restart: plays only if source has held note now; latch memory not restored | M14,M17 | latch; release; mute; unmute → silent; with key held → plays | #10,#11 | todo |
| REQ-4.6-7 | 4.6 | Latch velocity: most recent followed note's velocity | M14,M09 | latch; NOn 60 vel 100; release; NOn 64 vel 40 → next step vel 40 | #10 | todo |
| REQ-4.6-8 | 4.6 | Latch automatable | M20 | param exists | #13 | todo |

## §5 Timing

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-5.1-1 | 5.1 | Base list 1/32,1/16,1/8,1/4,1/2,1/1 | M01,M02 | lengths in beats: 0.125, 0.25, 0.5, 1, 2, 4 | #5 | todo |
| REQ-5.1-2 | 5.1 | Triplet = ×2/3, dotted = ×3/2 | M02,M03 | 1/8 → 1/3, 3/4 beat | #5 | todo |
| REQ-5.1-3 | 5.1 | Swing 0–100 %, clamped | M03,M12 | 150 → 100; −5 → 0 | #5,#10 | todo |
| REQ-5.1-4 | 5.1 | Swing shifts even 1-based steps backward; step 1 never | M03,M14 | 1/8, swing 100: starts (beats) 0, 0.5−0.25=0.25, 1.0, 1.25 … | #5,#10 | todo |
| REQ-5.1-5 | 5.1 | Shift = swing% × 50 % of step length | M03 | swing 50 → shift 0.125 beat for 1/8 | #5 | todo |
| REQ-5.1-6 | 5.1 | Counting restarts at step 1 each loop | M03,M13 | 7-step lane: step 8 (= step 1 of loop 2) unshifted | #5,#10 | todo |
| REQ-5.1-7 | 5.1 | Note length unchanged by swing | M14 | swung note length = step × gate | #10 | todo |
| REQ-5.1-8 | 5.1 | Swung note may overlap next step | M14,M11 | swing 100, gate 100 → note of step 2 ends where step 3 starts; no hang | #10 | todo |
| REQ-5.1-9 | 5.1 | Swing only for base timing, not text grid | M03,M14 | text grid + swing 100 → starts unshifted | #5,#10 | todo |
| REQ-5.1-10 | 5.1 | Swing: transport start on swung step position → unswung step 0 at playhead | M14 | arch D-4 | #10 | todo |
| REQ-5.2-1 | 5.2 | Timing Source switch; inactive source keeps settings | M12,M21 | switch BT→TG→BT: base params and text unchanged | #10,#14 | todo |
| REQ-5.2-2 | 5.2 | One step-data array shared by both sources | M10,M21 | edit step 3 in BT; switch to TG → step 3 same data | #8,#14 | todo |
| REQ-5.2-3 | 5.2 | Source switch/step-count change: data transferred by position, surplus discarded, missing default | M10 | 8 steps with vel pattern → 12: first 8 kept, 4 default (on); → 4: first 4 kept | #8 | todo |
| REQ-5.2-4 | 5.2 | Format "1/8t, 1/8., 1/4" → 3 steps (1/3, 3/4, 1 beat) | M05 | parse example | #6 | todo |
| REQ-5.2-5 | 5.2 | Numerator exactly 1; denominators 1,2,4,8,16,32,64 | M05 | valid: 1/1…1/64; invalid: 2/4, 3/8, 1/3, 1/128, 1/0, 0/4 | #6 | todo |
| REQ-5.2-6 | 5.2 | Suffix t or . optional, mutually exclusive | M05 | "1/8t." invalid; "1/8." ok; "1/8t" ok | #6 | todo |
| REQ-5.2-7 | 5.2 | Loop length = sum of steps, unrestricted | M05,M03 | "1/1,1/1,1/1,1/1,1/1" → 20 beats | #5,#6 | todo |
| REQ-5.2-8 | 5.2 | Max 64 entries | M05 | 64 ok; 65 invalid | #6 | todo |
| REQ-5.2-9 | 5.2 | Whitespace tolerance, empty string/entry | M05 | " 1/8 ,1/4 " ok; "" and "1/8,,1/4" invalid | #6 | todo |
| REQ-5.2-10 | 5.2 | New text grid: step data transferred by position | M10,M21 | 3 steps (off,on,off) → 5 steps: first 3 kept, 2 default | #8,#14 | todo |
| REQ-5.2-11 | 5.2 | Step start positions = prefix sums of lengths | M03 | "1/4,1/8,1/8" → 0, 1, 1.5, loop 2 | #5 | todo |
| REQ-5.3-1 | 5.3 | Invalid entries flagged (index/range) for red marking | M05,M23 | parser returns per-entry errors with positions | #6,#15 | todo |
| REQ-5.3-2 | 5.3 | Invalid grid not applied; last valid kept | M05,M21 | apply invalid → active grid unchanged | #6,#14 | todo |
| REQ-5.4-1 | 5.4 | Step count 1–64, clamped | M12 | 0 → 1; 65 → 64 | #10 | todo |
| REQ-5.4-2 | 5.4 | Loop length = count × step length | M03 | 7 × 1/8 = 3.5 beats | #5 | todo |
| REQ-5.4-3 | 5.4 | Increase: new steps on, default vel | M10 | 4 → 8 | #8 | todo |
| REQ-5.4-4 | 5.4 | Decrease discards; increase does not restore | M10 | set step 6 off; 8 → 4 → 8 → step 6 on | #8 | todo |
| REQ-5.4-5 | 5.4 | Change effective at next step start; phase re-anchored: current index kept mod new count; no skip/repeat | M13,M14 | 16 → 7 steps at step 10 (idx 9): continues idx 9 mod 7 = 2 at next step start, then 3, 4 … | #10 | todo |
| REQ-5.4-6 | 5.4 | Step count change: sounding notes get Note-Off (§9.3) | M14 | gate 200; change count → every note ended | #10 | todo |
| REQ-5.4-7 | 5.4 | Different counts per lane | M17 | L1 16, L2 7 → L2 restarts every 7 steps | #10,#11 | todo |

## §6 Steps

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-6-1 | 6 | Off step silent for its duration | M14 | step 2 off → no event in [0.25,0.5) | #10 | todo |
| REQ-6-2 | 6 | Length = step × gate / 100 | M14,M03 | 1/16, gate 50 → Note-Off 3000 samples after On | #10 | todo |
| REQ-6-3 | 6 | Gate range 1–200, clamped; default 100 | M12 | 0 → 1; 500 → 200 | #10 | todo |
| REQ-6-4 | 6 | Gate > 100: overlap, polyphonic | M14,M11 | gate 200, chord idx 0: Note-On of step k+1 happens while k sounding (different pitch: both sound; same pitch: retrigger §9.1) | #10 | todo |
| REQ-6-5 | 6 | Gate 1 %: minimal note length ≥ 1 sample, Note-Off after On | M04,M14 | 1/32 gate 1 at 48 kHz → On, Off ≥ 1 sample later | #5,#10 | todo |
| REQ-6-6 | 6 | Gate automatable | M20 | param exists | #13 | todo |

## §7 Velocity

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-7-1 | 7 | Default mode incoming velocity | M09,M12 | fresh lane → incoming | #8,#10 | todo |
| REQ-7-2 | 7 | Each step stores value per mode; switch restores | M10 | set by-step 80, delta −20; switch incoming → by-step → 80 still there; delta −20 still there | #8 | todo |
| REQ-7-3 | 7 | No mode overwrites another's values | M10 | write relative 150 → by-step unchanged | #8 | todo |
| REQ-7.1-1 | 7.1 | Incoming: note vel = input vel | M09 | input 90 → 90; step values ignored | #8 | todo |
| REQ-7.1-2 | 7.1 | By step: absolute 0–127 | M09 | step 100, input 20 → 100 | #8 | todo |
| REQ-7.1-3 | 7.1 | Delta: input + delta (−127…+127) | M09 | input 100, delta −30 → 70 | #8 | todo |
| REQ-7.1-4 | 7.1 | Relative: input × pct / 100 (0–200) | M09 | input 100, 150 % → 150 → clamp 127; 50 % → 50 | #8 | todo |
| REQ-7.2-1 | 7.2 | Clamp high to 127 | M09 | input 120, delta +50 → 127 | #8 | todo |
| REQ-7.2-2 | 7.2 | Clamp low to 0 | M09 | input 20, delta −50 → 0 | #8 | todo |
| REQ-7.2-3 | 7.2 | Velocity 0: no note sent, step silent | M09,M14 | by-step 0 → no Note-On, no Note-Off | #8,#10 | todo |
| REQ-7.2-4 | 7.2 | Velocity 0 step: position advances, other sounding voices unaffected | M14 | gate 200; step k vel 20, step k+1 delta → 0: note k continues to its scheduled Off | #10 | todo |
| REQ-7.3-1 | 7.3 | Input velocity sampled at step trigger | M14 | NOn vel 100; step starts; NOn same pitch vel 50 mid-step → sounding note keeps 100; next step uses 50 | #10 | todo |
| REQ-7.3-2 | 7.3 | Latch: last followed note's velocity | M14 | see REQ-4.6-7 | #10 | todo |
| REQ-7.3-3 | 7.3 | Mode change affects only newly triggered | M14 | change mode mid-note → running note keeps vel | #10 | todo |
| REQ-7.3-4 | 7.3 | Velocity mode automatable | M20 | param exists | #13 | todo |

## §8 Output routing

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-8-1 | 8 | Output channel 1–16; default 1; never auto-changed | M12,M14 | follow ch5, out ch stays 1; out ch 7 → events on ch 7 | #10 | todo |
| REQ-8-2 | 8 | Out channel stored in state/presets | M22 | round-trip | #14 | todo |
| REQ-8-3 | 8 | Transpose −24…+24; out = in + transpose | M12,M14 | 60 + 12 = 72; clamp param 30 → 24 | #10 | todo |
| REQ-8-4 | 8 | Pitch outside 0–127: not sent, no clamping | M14 | 120 + 24 → no event, no Note-Off; 5 − 24 → none | #10 | todo |
| REQ-8-5 | 8 | Unique voice id per output note | M11 | two Note-Ons → distinct ids | #9 | todo |

## §9 Voices

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-9.1-1 | 9.1 | Voices tracked per (channel, pitch) across lanes and pass-through | M11 | L1 and L2 same ch/pitch; pass-through same pitch | #9 | todo |
| REQ-9.1-2 | 9.1 | Retrigger: Note-Off then Note-On immediately, ownership transfers | M11,M17 | L1 plays 60 ch1 (gate 200); L2 triggers 60 ch1 → [NOff 60, NOn 60] same sample, in this order | #9,#11 | todo |
| REQ-9.1-3 | 9.1 | Previous owner's later Note-Off suppressed; released when newest ends | M11 | continue REQ-9.1-2; L1 end time → no event; L2 end → NOff | #9 | todo |
| REQ-9.1-4 | 9.1 | Cross-lane retrigger and ownership transfer (edge, §18) | M11,M17 | three owners chained L1→L2→L3 → one Note-Off at L3 end | #9,#11 | todo |
| REQ-9.1-5 | 9.1 | Retrigger vs pass-through (both directions) | M11,M16 | Thru on, key 60 ch1 passed; lane triggers 60 ch1 → [NOff, NOn]; key NOff suppressed; lane's end sends NOff. Reverse: lane voice sounding, passed NOn 60 → same retrigger | #9,#11 | todo |
| REQ-9.1-6 | 9.1 | Same pitch, different channel: independent voices | M11 | 60 ch1 and 60 ch2 → no retrigger | #9 | todo |
| REQ-9.1-7 | 9.1 | Note-Off velocity 0 | M11,M19 | all generated Note-Offs vel 0 | #9,#12 | todo |
| REQ-9.1-8 | 9.1 | Same lane overlap same pitch (gate > 100, idx 0): retrigger | M11,M14 | gate 200: each step retriggers pitch; one Off per sound end | #10 | todo |
| REQ-9.2-1 | 9.2 | Release stops lane at once; Note-Offs for all sounding lane notes | M14 | gate 200 (2 overlapping); NOff source → both Offs at once | #10 | todo |
| REQ-9.2-2 | 9.2 | Applies to both input modes | M14 | chord-follow and channel mode variants | #10 | todo |
| REQ-9.2-3 | 9.2 | Latch on: no stop | M14 | see REQ-4.6-2 | #10 | todo |
| REQ-9.3-1 | 9.3 | Every Note-On gets a Note-Off: grid change | M14 | grid change at next step start, gate 200 → all sounding notes end | #10 | todo |
| REQ-9.3-2 | 9.3 | … step count change | M14 | see REQ-5.4-6 | #10 | todo |
| REQ-9.3-3 | 9.3 | … Timing Source change | M14 | switch BT↔TG with sounding notes | #10 | todo |
| REQ-9.3-4 | 9.3 | … playhead discontinuity | M17 | see REQ-10-7 | #11 | todo |
| REQ-9.3-5 | 9.3 | … transport stop | M17 | see REQ-10-3 | #11 | todo |
| REQ-9.3-6 | 9.3 | … mute | M14 | mute during note → Off at that sample | #10 | todo |
| REQ-9.3-7 | 9.3 | … latch end | M14 | see REQ-4.6-4 | #10 | todo |
| REQ-9.3-8 | 9.3 | … bypass | M17 | see REQ-21-2 | #11 | todo |
| REQ-9.3-9 | 9.3 | Note-Off guarantee property test: random event streams, invariant holds | M17 | seeded random input/params/transport 10 000 blocks → no hanging notes after final stop | #11 | todo |
| REQ-9.4-1 | 9.4 | ≤ 32 voices per lane (owned voices) | M11,M14 | gate 200, 1/32 steps, rotating pitches (≥ 33 distinct via transpose changes) → owned count never > 32 | #9,#10 | todo |
| REQ-9.4-2 | 9.4 | Over limit: oldest voice gets Note-Off before new Note-On | M11 | 33rd Note-On: [NOff oldest, NOn new] | #9 | todo |
| REQ-9.4-3 | 9.4 | Limit counts owned voices only (transferred ownership not counted) | M11 | L1 owns 32; L2 steals one → L1 owns 31; L1 new note no eviction | #9 | todo |
| REQ-9.4-4 | 9.4 | Limit per lane, not global | M11 | 8 lanes × 32 = 256 ok | #9 | todo |

## §10 Transport

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-10-1 | 10 | Transport start resets all lanes to step 1 | M15,M17 | T+ at beat 3.1 → first step at playhead (offset), steps count from 1 | #11 | todo |
| REQ-10-2 | 10 | Start at arbitrary PPQ (also non-integer) | M13 | T+ at 7.3 → phase offset 7.3 | #10,#11 | todo |
| REQ-10-3 | 10 | Stop: all lanes stop, Note-Offs for all sounding notes | M17 | sounding gate 200; T- → all Offs at stop sample | #11 | todo |
| REQ-10-4 | 10 | Without transport: silent, no internal clock | M17 | transport never started + notes held → no output | #11 | todo |
| REQ-10-5 | 10 | Held keys at transport start drive lanes at once | M17 | NOn before T+; T+ → plays at step 1 | #11 | todo |
| REQ-10-6 | 10 | Stop clears latch memory only | M14,M17 | see REQ-4.6-4/6; held-note state intact | #11 | todo |
| REQ-10-7 | 10 | Playhead discontinuity (backward, forward beyond one buffer): Note-Offs, pending changes applied, position re-derived | M15,M17 | loop wrap 8→0; locate 4→100 → Offs; pending grid change applied; position = (playhead − offset) mod loop; offset unchanged | #11 | todo |
| REQ-10-8 | 10 | Only start sets phase offset to map playhead to step 1 | M13 | locate keeps offset; position by formula | #10,#11 | todo |
| REQ-10-9 | 10 | Position formula (playhead − offset) mod loop in beats | M13 | offset 1.0, playhead 5.5, loop 4 → 0.5 | #10 | todo |
| REQ-10-10 | 10 | Tempo change: step positions and Note-Off times follow (beats) | M14,M04 | 120 → 60 bpm mid-note → Off sample moves, beat unchanged | #10,#11 | todo |
| REQ-10-11 | 10 | Sample-accurate offsets within buffer | M04,M17 | events at expected `offset = round(beats × samplesPerBeat)`; block sizes 64/480/1024 yield identical absolute samples | #5,#11 | todo |
| REQ-10-12 | 10 | Forward move of ≤ one buffer is no discontinuity | M15 | normal playback block-to-block | #11 | todo |
| REQ-10-13 | 10 | Block splitting invariance: same absolute output for any block size | M17 | run same scenario with block sizes 32…4096 | #11 | todo |
| REQ-10-14 | 10 | Beats only, never samples (core stores beats) | M01–M17 | `n/t` as test; covered by REQ-10-10/-13 | #5 | n/t: design rule, indirectly verified |

## §11 Live changes

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-11-1 | 11 | Grid/step count/Timing Source change at next step start of that lane | M14 | change mid-step → effective at step end; other lanes independent | #10 | todo |
| REQ-11-2 | 11 | Step on/off + velocity edits immediate | M14,M10 | toggle upcoming step before its start → honoured; mid-step → affects only future triggers | #8,#10 | todo |
| REQ-11-3 | 11 | Input mode/Follow Index/Follow Channel change at next step start; release stops at once | M14 | idx 0→1 mid-step → new idx from next step | #10 | todo |
| REQ-11-4 | 11 | Mute/Latch immediate | M14 | see REQ-9.3-6, REQ-4.6-4 | #10 | todo |
| REQ-11-5 | 11 | Transpose/gate/swing/out channel change affects only new notes; sounding keep pitch, velocity, Note-Off time | M14 | transpose +12 mid-note → Off for old pitch at old time | #10 | todo |
| REQ-11-6 | 11 | Glitch-free: no stuck/dup notes on any parameter change | M17 | parameter fuzz + invariants | #11 | todo |
| REQ-11-7 | 11 | Gate change on sounding note: Off time not moved | M14 | gate 100 → 50 mid-note | #10 | todo |

## §12 Parameters

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-12-1 | 12 | Exact global param set: Thru, Consume, Chord Input Channel | M20 | layout IDs | #13 | todo |
| REQ-12-2 | 12 | Exact lane param set ×8 (14 params) | M20 | layout IDs per lane | #13 | todo |
| REQ-12-3 | 12 | Ranges and defaults match §20/§4–§8 | M20 | each param min/max/default | #13 | todo |
| REQ-12-4 | 12 | Text grid, step data, colors not host parameters | M20 | absent from layout | #13 | todo |
| REQ-12-5 | 12 | Parameter → config mapping | M20 | snapshot → `SequencerConfiguration` | #13 | todo |
| REQ-12-6 | 12 | Param IDs stable (automation compat) | M20 | golden list of IDs | #13 | todo |

## §13 State and presets

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-13-1 | 13 | State contains params, text grids, colors, step data | M22 | fill all; save → load → equal | #14 | todo |
| REQ-13-2 | 13 | Preset file = JUCE ValueTree XML, same scope incl. globals | M22 | save/load preset incl. Thru, Consume, chord ch | #14 | todo |
| REQ-13-3 | 13 | Factory presets ship and load | M24 | each factory preset loads cleanly | #16 | todo |
| REQ-13-4 | 13 | User folder presets listed/loaded/saved | M22 | write to temp dir, list, load | #14 | todo |
| REQ-13-5 | 13 | Version integer in state and presets | M22 | attribute present | #14 | todo |
| REQ-13-6 | 13 | Older version migrated (state version migration, §18) | M22 | v1 fixture (each older version) → current | #14 | todo |
| REQ-13-7 | 13 | Unknown fields ignored | M22 | extra attr/child | #14 | todo |
| REQ-13-8 | 13 | Invalid values clamped | M22 | gate 999 → 200; ch 0 → 1 | #14 | todo |
| REQ-13-9 | 13 | Newer version → safe default state | M22 | version = current+1 → defaults | #14 | todo |
| REQ-13-10 | 13 | Corrupt/empty data → safe default, no crash | M22 | garbage bytes | #14 | todo |

## §14 UI

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-14-1 | 14 | Single resizable window; global bar; 8 lane rows; vertical scroll | M23 | editor construct, resize, 8 rows exist | #15 | todo |
| REQ-14-2 | 14 | Control strip: all lane params + Timing Source + color picker | M23 | components per lane present | #15 | todo |
| REQ-14-3 | 14 | Lane color used for on-step pads and drag bars; off steps neutral | M23 | `n/t` pixel level; test color selection function: on → lane color, off → neutral | #15 | todo |
| REQ-14-4 | 14 | Colors visible for all lanes at once, distinct by default | M23 | default palette distinct (see REQ-4-5) | #15 | todo |
| REQ-14-5 | 14 | Color picker changes only own lane | M23,M21 | see REQ-4-6 | #15 | todo |
| REQ-14-6 | 14 | Shared scale: width = loop beats × common pixels/beat; longest lane = full width | M23 | lanes of 8×1/8 and 8×1/8 → equal widths = available width | #15 | todo |
| REQ-14-7 | 14 | Shared scale: 12×1/8 = 8×1/8 + 4×1/8 | M23 | L1 12×1/8 (6 beats), L2 8×1/8 (4 beats), avail W: ppb = W/6; w1 = W; w2 = 4W/6; w1−w2 = 4 × (W/12) | #15 | todo |
| REQ-14-8 | 14 | Shared scale: 7×1/8 = 8×1/8 − 1×1/8 | M23 | L1 8×1/8, L2 7×1/8: w1 − w2 = one 1/8 width | #15 | todo |
| REQ-14-9 | 14 | Shared scale: 4×1/4 = 8×1/8 (equal note values, equal width; different step counts) | M23 | L1 4×1/4, L2 8×1/8 (both 4 beats) → w1 == w2; step width of 1/4 = 2 × width of 1/8 across lanes | #15 | todo |
| REQ-14-10 | 14 | Scale recomputed when longest loop changes (grid, step count, source) | M23 | longest lane 8 → 4 steps: ppb grows, other lanes widen proportionally | #15 | todo |
| REQ-14-11 | 14 | Swing does not change displayed widths | M23 | swing 0 vs 100 → identical geometry | #15 | todo |
| REQ-14-12 | 14 | Min step width clamp: scale raised, content wider than available, lanes scroll together | M23 | avail 400 px, 64×1/32 (8 beats) + `minimumStepDisplayWidth` → ppb = min/0.125; content > 400; shared by all lanes | #15 | todo |
| REQ-14-13 | 14 | Shared scale uses active timing source (text grid: sum of entries, triplet/dotted widths) | M23 | "1/8t,1/8.,1/4" widths 1/3 : 3/4 : 1 beat | #15 | todo |
| REQ-14-14 | 14 | Empty/all-empty lanes: width 0, no divide-by-zero | M23 | all lanes empty → ppb 0 | #15 | todo |
| REQ-14-15 | 14 | Step toggle click; drag bar range per mode (0–127, −127…+127, 0–200 %); numeric value while dragging | M23 | model-level: value→bar mapping + clamping | #15 | todo |
| REQ-14-16 | 14 | Incoming mode: drag bars disabled | M23 | enabled flag false | #15 | todo |
| REQ-14-17 | 14 | Playback position indicator per lane | M23 | `currentStepIndex` → x; hidden if out of range (arch §4.9) | #15 | todo |
| REQ-14-18 | 14 | Text grid editable regardless of source; invalid shown red | M23 | edit while BT active; invalid marked | #15 | todo |
| REQ-14-19 | 14 | Text Grid active: Base Timing, Subdivision, Swing, Step Count disabled | M23 | enabled flags | #15 | todo |

## §15–§17 Implementation, architecture, real time

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-15-1 | 15 | C++20, JUCE, VST3 on Win/macOS/Linux | build | CI matrix builds all three; pluginval on the VST3 | #4,#12 | todo |
| REQ-16.1-1 | 16.1 | Reusable libs have no JUCE includes | lib/* | CI/CMake check: `lib` targets do not link JUCE; grep for `juce` in `lib/` = 0 | #4 | todo |
| REQ-16.2-1 | 16.2 | Timing values and modifier factors defined once | M01,M02 | static assertions + grep for literals `2.0/3`, `1.5` outside Constants | #5 | todo |
| REQ-16.2-2 | 16.2 | Beats → samples: pure function of BPM and sample rate | M04 | 1 beat @120/48k = 24000; @60 = 48000; 0.25 @ 44.1k/133 bpm | #5 | todo |
| REQ-16.3-1 | 16.3 | No magic constants (named, central; velocity limits in one place) | M01 | review checklist; grep for 127/32/16/8 outside `Constants.h` | #5 | n/t: code review/grep script, not a runtime test |
| REQ-16.4-1 | 16.4 | Velocity modes / index selection as registered strategies, no switch chains | M08,M09 | registry lists all options; adding a test strategy needs no existing code change | #7,#8 | todo |
| REQ-16.5-1 | 16.5 | One responsibility per module, narrow interfaces | all | `n/t`: design review | – | n/t: architectural review |
| REQ-17-1 | 17 | No allocation/locks/blocking in audio path | M17,M19 | `AllocationGuard` over every scenario path (start/stop/locate/bypass/max events) → 0 `new` | #11,#12 | todo |
| REQ-17-2 | 17 | UI→audio via atomics/lock-free | M18 | triple buffer concurrent stress (TSan) | #12 | todo |
| REQ-17-3 | 17 | Invalid inputs rejected or clamped | M05,M12 | out-of-range config values, NaN tempo, negative sample count | #5,#10,#11 | todo |
| REQ-17-4 | 17 | No exceptions in audio path | M17 | build with `-fno-exceptions` for core or `noexcept` static asserts | #11 | todo |
| REQ-17-5 | 17 | RAII, no raw new/delete | all | `n/t`: lint (clang-tidy `cppcoreguidelines-owning-memory`) | #4 | n/t: static analysis |
| REQ-17-6 | 17 | Odd host input: 0-sample blocks, huge event counts, unsorted events | M17,M19 | arch §11 adapter cases | #11,#12 | todo |

## §18 Edge cases (all must be tests)

| Edge case | IDs |
|---|---|
| Index larger than chord | REQ-4.2-1, REQ-4.2-4 |
| Chord re-sorting | REQ-4.2-5, REQ-4.2-6, REQ-3.4-1 |
| Velocity clamping and zero velocity | REQ-7.2-1…4 |
| Overlap / gate > 100 | REQ-6-4, REQ-5.1-8, REQ-9.1-8 |
| Retrigger and ownership transfer | REQ-9.1-2…5 |
| Transport reset | REQ-10-1, REQ-10-2 |
| Playhead discontinuity | REQ-10-7, REQ-10-8, REQ-10-12 |
| Note-Off guarantee | REQ-9.3-1…9 |
| Voice limit | REQ-9.4-1…4 |
| Per-note pass-through | REQ-3.3-4…6 |
| Latch / mute / transport stop | REQ-4.6-1…8, REQ-9.3-6, REQ-10-3 |
| Swing | REQ-5.1-3…10 |
| Step-count re-anchoring | REQ-5.4-5, REQ-5.4-6, REQ-5.4-7 |
| Bypass flush | REQ-21-1…5 |
| State migration | REQ-13-6…10, REQ-4-7 |
| Lane color persistence | REQ-4-3…7 |
| Shared display scale | REQ-14-6…14 |

## §20 Initial state

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-20-1 | 20 | Lane defaults: Base Timing, 1/16, straight, swing 0, 16 steps all on | M12,M21 | default config/doc | #10,#14 | todo |
| REQ-20-2 | 20 | Vel mode incoming; chord-follow idx 0; out ch 1; transpose 0; gate 100; latch/mute off | M12 | default config | #10 | todo |
| REQ-20-3 | 20 | Globals: chord ch 1, Thru off, Consume on | M20 | default params | #13 | todo |
| REQ-20-4 | 20 | Default lane colors = fixed palette in lane order | M01,M21 | see REQ-4-5 | #5,#14 | todo |
| REQ-20-5 | 20 | Fresh plugin, chord 60 held, T+ → 16th notes on 60, vel = input, ch 1 | M17 | end-to-end: NOn(1,60,100) @0 held, T+ @beat 0 → NOn @0, @6000, @12000 …; each NOff at the next step start (@6000, …), Off before On at equal sample | #11 | todo |

## §21 Bypass

| ID | § | Rule | Module | Scenario | Issue | Status |
|---|---|---|---|---|---|---|
| REQ-21-1 | 21 | Bypass: all input events passed unchanged | M17,M19 | any note/CC input → identical output; lanes silent | #11,#12 | todo |
| REQ-21-2 | 21 | Entering bypass: Note-Offs for passed-through and sounding lane notes; tracking cleared | M17,M16 | Thru on key held + lane sounding; bypass → Offs for both | #11 | todo |
| REQ-21-3 | 21 | Lanes stop, latch ends | M14,M17 | latch memory cleared | #11 | todo |
| REQ-21-4 | 21 | Leaving bypass: position derived from playhead, no reset to step 1 | M13,M17 | bypass at beat 3.1, off at 5.6 → step index from formula with old offset | #11 | todo |
| REQ-21-5 | 21 | After bypass: Note-Off of raw passed notes follows raw (no tracking) | M16 | Note-On during bypass, Note-Off after bypass ends → Thru off, not tracked: Note-Off not passed (unmatched Off; spec silent, see Spec observations) | #11 | todo |

## Not testable / out of scope
- §1, §2, §19: descriptive or style rules; checked by review (REQ-16.3-1, REQ-16.5-1, REQ-17-5).
- §2 VST3 MIDI-effect category: verified by pluginval/manual host check (REQ-15-1).
- §3.3 "no special handling" of CC120/123 is tested as negative (REQ-3.3-9).

## Spec observations
None blocking. REQ-21-5: spec silent whether a Note-Off for a note begun during bypass passes after bypass ends; tests follow arch decision when #11 lands. Re-check REQ-9.1-5 (retrigger vs pass-through ownership) and REQ-21-5 (post-bypass raw Note-Off) against arch decisions D-17/A-numbers when #9/#11 implement them.
