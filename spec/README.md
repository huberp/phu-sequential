# ARP Plugin Specification v2

**Source of truth for behavior.** If this document and architecture disagree, **the spec wins**.

This document has been split into focused sub-documents for better token efficiency and navigation. Each file covers a specific aspect of the specification.

## Table of Contents

| Section | Document | Description |
|---------|----------|-------------|
| §1-2 | [Overview](01-overview.md) | Overview, Plugin Format and Category |
| §3 | [Input Processing](02-input-processing.md) | Chord definition, channel filter, pass-through, sorting |
| §4 | [Sequencer Lanes](03-sequencer-lanes.md) | Lane configuration, chord-follow, channel mode, latch |
| §5 | [Timing](04-timing.md) | Base timing, custom grid (text grid), step count |
| §6 | [Steps](05-steps.md) | Step on/off, gate parameter |
| §7 | [Velocity](06-velocity.md) | Velocity modes, clamping, sampling |
| §8 | [Output Routing](07-output-routing.md) | Output channel, transpose, voice tracking |
| §9 | [Voice Management](08-voice-management.md) | Voice tracking, release behavior, guaranteed Note-Offs, voice limit |
| §10 | [Transport](09-transport.md) | Transport and synchronization |
| §11 | [Live Changes](10-live-changes.md) | Glitch-free changes during playback |
| §12 | [Parameters](11-parameters.md) | Automatable parameters, non-automatable state |
| §13 | [State and Presets](12-state-presets.md) | VST3 state, presets, versioning |
| §14 | [UI](13-ui.md) | Editor layout, controls, visualization |
| §15-16 | [Implementation](14-implementation.md) | Implementation and tooling, architecture layers |
| §17 | [Robustness](15-robustness.md) | Real-time constraints, validation |
| §18 | [Testability](16-testability.md) | Unit tests, edge cases |
| §19 | [Code Style](17-code-style.md) | Coding conventions |
| §20 | [Initial State](18-initial-state.md) | Default values for all parameters |
| §21 | [Bypass](19-bypass.md) | Host bypass behavior |

---

**Note:** Section citations (e.g., "Section 3.1", "Section 4.2") within the sub-documents refer to the original section numbering and are preserved for backward compatibility with `docs/architecture.md` and other documents.

**Navigation tip:** Use `grep -n '^## ' spec/*.md` to find sections, or load individual files by topic for token efficiency.
