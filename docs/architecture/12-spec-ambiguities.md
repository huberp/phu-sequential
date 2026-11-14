# phu-sequential — Architecture — 12-spec-ambiguities.md

## 12. Spec ambiguities

Every gap, ambiguity or contradiction found in `spec/arp-spec.md` while designing. Each lists the decision taken here — **all provisional** until confirmed or corrected in the spec.

| ID | Spec | Ambiguity / gap | Provisional decision |
|---|---|---|---|
| A-1 | §5.1 | "Swing shifts … backward": spec says backward; conventional swing delays; "may extend into the following step" fits a delay better. | Doc follows spec — even 1-based steps start `len·swing/200` earlier — owner to confirm (D-4) |
| A-2 | §5.1 | With an odd step count, the loop's last step and the next loop's step 1 are adjacent unswung steps; not stated explicitly. | Swing depends on the index within the loop only (counting restarts, §5.1) — M03 rules |
| A-3 | §5.4, §5.2, §20 | "Default velocity values" of the step modes are not given. | By Step 100, Delta 0, Relative 100 % — D-5 |
| A-4 | §7.1 | Relative mode "divided by 100": rounding of non-integer results unspecified. | Round half up — D-6 |
| A-5 | §5.1, §6, §7.1 | Whether gate/swing/relative percentages are integer or fractional. | Integer percent — D-7 |
| A-6 | §5.2, §5.3 | Whitespace, case of `t`, leading zeros, empty entries, empty text: validity not stated. | Grammar of D-8; empty text invalid |
| A-7 | §5.2, §20 | Initial text-grid content is not specified; behavior of Timing Source = Text Grid when no valid grid was ever entered is undefined. | Empty text, no applied grid, lane silent, no step-data discard — D-9 |
| A-8 | §5.3, §13 | Whether an invalid (unapplied) text is persisted, and whether the applied grid is persisted separately. | Store both the typed text and the applied grid — D-9, D-34 |
| A-9 | §5.4, §11 | Only "step count, grid, Timing Source" are quantized; base timing and subdivision changes are not mentioned. | They are grid changes (next step start + re-anchoring) — D-10 |
| A-10 | §5.4 | "Current step index is kept modulo the new step count, and the new loop starts at that step start" is ambiguous (does the new loop start at step 0 or at index k'?). | `k' = k mod newCount` plays at the commit point; phase offset set so that k' starts there (loop index 0) — D-11 |
| A-11 | §5.1, §11 | "Next step start" under swing: grid start or swung start? | The effective (swung) start: commits and trigger both happen there — D-12 |
| A-12 | §4.2, §4.4 | A source appearing mid-step (silence → note) — start immediately or at the next step? | Next step trigger only — D-13 |
| A-13 | — | Ordering of simultaneous events (Note-Off/Note-On at the same sample, input vs generated) is unspecified. | §5.3 rules — D-14 |
| A-14 | §10, §3.3 | "All sounding notes receive Note-Offs" at transport stop / discontinuity: includes passed-through notes? Doing so cuts notes of keys that are still held. | All voices including pass-through; tracking cleared, the later input Note-Off is suppressed (no double Note-Off); a still-held key stays silent until re-pressed — D-17 |
| A-15 | §11, §10 | What happens to pending (quantized) changes at transport stop, and when changes take effect while stopped. (Open point of #2.) | Commit at stop without re-anchoring; apply immediately while stopped — D-18 |
| A-16 | §10 | Discontinuity "applies all pending changes": with or without re-anchoring? | Without; position re-derived from the playhead with the existing phase offset — D-19 |
| A-17 | §10 | Plugin instantiated / resumed while the host is already playing; host without PPQ or tempo. | First playing block = transport start; missing PPQ/tempo = not playing — D-20 |
| A-18 | §10 | "Forward jump beyond one buffer": tolerance for jitter / rounding not specified. | ≤ 1 sample backward and ≤ block length forward are continuous — D-16 |
| A-19 | §11, §12 | Granularity of parameter changes within a block (sample-accurate automation?). | Once per block at offset 0 — D-21 |
| A-20 | §4.6, §7.3 | When latch memory is updated ("at the next step start" vs at note-on), and whether it survives a follow-selector change. | Updated at event time, survives selector changes, adoption still at next trigger — D-22 |
| A-21 | §4.4, §11 | Selector change (input mode/index/channel) committing to an empty source: release or keep sounding? | Release at that step start if latch off — D-23 |
| A-22 | §3.2, §11 | Timing of a Chord Input Channel change is not listed in §11. | Immediate (block start) — D-24 |
| A-23 | §3.3 | Consume "follow channels of the channel-mode lanes": muted lanes? pending input-mode changes? | Current parameter values, muted included — D-25 |
| A-24 | §9.1, §3.3 | Note-Off velocity for passed-through notes (original release velocity or 0)? Output of incoming Note-On velocity 0. | Always 0; Note-On v=0 emitted as Note-Off — D-26 |
| A-25 | §21 | Notes held across bypass exit: their later Note-Offs — pass or drop? Is held-note/transport tracking active during bypass? Transport start during bypass: reset to step 1? | Tracking continues; held raw notes adopted as pass-through voices; start during bypass anchors the phase — D-27 |
| A-26 | §21, §13 | Whether host bypass is a parameter and whether it is stored. | JUCE bypass parameter, not stored — D-28 |
| A-27 | §9.1, §9.4 | Order of retrigger Note-Off vs voice-limit eviction when both occur. | Retrigger first, then eviction if still at limit — D-29 |
| A-28 | §4.3, §20 | Default Follow Channel not specified. | 1 — D-33 |
| A-29 | §2, §15 | VST3 has no MIDI-effect category; AU not mentioned. | "Fx\|Tools"; AU MIDI processor optional on macOS — D-39 |
| A-30 | §6 | Extremely short notes (gate 1 % of 1/64 triplet at high tempo) can start and end on the same sample. | Note-On then Note-Off at the same offset — D-40 |
| A-31 | §3.1 | Whether CC64 is evaluated on channels not followed by any lane. | Per channel on all 16 channels — D-41 |
| A-32 | §9.3 | Notes sounding when the host calls prepare/reset (sample-rate change) — §9.3 "under all conditions". | Flush at offset 0 of the next block — D-44 |
| A-33 | §13 | Fallback for a rejected *preset* (newer/corrupt) — §13 only says "loaders … fall back to a safe default state". | Same as state: default state — D-47 |
| A-34 | §4.6, §9.3, §11 | Mute "takes effect immediately": does it cut sounding notes or let them finish? | Cut immediately (Note-Offs) — D-48 |
| A-35 | §3.1, §3.3 | Repeated Note-On on an already held input key: how Note-Offs pair for pass-through. | One entry per key; first release ends it — D-49 |
| A-36 | §5.4, §5.2 | Step-data "discard" when the effective step count decreases and increases again before a step start (fast automation); whether switching to a text grid with fewer entries discards; races with concurrent UI edits. | Discard happens only on committed decreases (also for source switches), in the core at commit; revisioned hand-off so an increase never restores discarded data and a later UI edit is never lost — D-32 |
| A-37 | §3.3, §17 | SysEx and other non-channel events are not listed among non-note events; Thru/Consume wording covers notes only. | All non-note events pass unchanged at their original offsets, outside the core (never subject to core capacity); Thru/Consume do not apply; extreme-volume host-output limits per A-44 — D-31 |
| A-38 | §17 | Behavior on event-capacity overflow is unspecified. | Input to the core is never dropped (D-51); core output is partitioned so nothing is refused (D-30); host output partition keeps 2048 Note-Off slots so generated Note-Offs are never dropped; residual drops of raw/Note-On events listed in A-44; drops counted — D-30, D-45 |
| A-39 | §14 | Editor default/min/max size and the minimum step width value are not given. (Open point of #2.) | D-36, D-37 |
| A-40 | §14 | Display of a lane whose active Text Grid has no valid grid. | Placeholder "no grid", zero width — D-9, §6.5 |
| A-41 | §11, §5.1 | "Swing changes affect newly triggered notes", but a backward shift needs the swing value before the step's nominal start. | Sampled when the preceding step start schedules the swung step — D-50 |
| A-42 | §17 | Zero-sample host blocks (and events delivered in them) are unspecified. | Nothing emitted, state unchanged, events accumulated across consecutive zero-sample blocks and delivered at offset 0 of the next non-empty block; release-class events never dropped, non-release events beyond 8192 entries / 64 KiB evicted or refused (counted) — D-53 |
| A-43 | §17 | §17 forbids allocation on the audio thread, but the host-owned output `juce::MidiBuffer` (a `juce::Array`) cannot be pre-sized by the plugin; its existing storage may be smaller than the output. | **Known deviation**: the host buffer is grown once, on the audio thread, to the scratch capacity when needed (in practice once per host buffer object), counted in `hostOutputGrowthCount`; the alternative (truncate, risking dropped Note-Offs) is rejected. Owner to confirm or choose truncation — D-54 |
| A-44 | §17, §9.3 | Even with partitioned capacity, a host block with > ≈ 114 k output events cannot be fully delivered. | Generated Note-Offs are never dropped. Can be dropped (counted): raw release input beyond (2^20 − 18 432)/9 = 114 460 events; raw other events and Note-Ons beyond (2^20 − 18 864)/9 = 114 412 events (fewer with large SysEx); a dropped Note-On's Note-Off is suppressed but the core still counts the silent voice toward voice limits until it ends; deferred non-release events per A-42. Owner to confirm the thresholds — D-45 |

---

*End of document.*
