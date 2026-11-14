# ARP Plugin Specification v2

**This document has been split into focused sub-documents for better token efficiency and navigation.**

> **Note:** This file is maintained for backward compatibility. All existing references to `spec/arp-spec.md` will continue to work, but the content has been reorganized.

## New Structure

Please refer to the new split documentation:

- **[spec/README.md](README.md)** - Table of Contents with complete section mapping

### Quick Access

| Original Section | New Document |
|-----------------|--------------|
| §1-2 Overview & Plugin Format | [01-overview.md](01-overview.md) |
| §3 Input Processing | [02-input-processing.md](02-input-processing.md) |
| §4 Sequencer Lanes | [03-sequencer-lanes.md](03-sequencer-lanes.md) |
| §5 Timing | [04-timing.md](04-timing.md) |
| §6 Steps | [05-steps.md](05-steps.md) |
| §7 Velocity | [06-velocity.md](06-velocity.md) |
| §8 Output Routing | [07-output-routing.md](07-output-routing.md) |
| §9 Voice Management | [08-voice-management.md](08-voice-management.md) |
| §10 Transport | [09-transport.md](09-transport.md) |
| §11 Live Changes | [10-live-changes.md](10-live-changes.md) |
| §12 Parameters | [11-parameters.md](11-parameters.md) |
| §13 State & Presets | [12-state-presets.md](12-state-presets.md) |
| §14 UI | [13-ui.md](13-ui.md) |
| §15-16 Implementation & Architecture | [14-implementation.md](14-implementation.md) |
| §17 Robustness | [15-robustness.md](15-robustness.md) |
| §18 Testability | [16-testability.md](16-testability.md) |
| §19 Code Style | [17-code-style.md](17-code-style.md) |
| §20 Initial State | [18-initial-state.md](18-initial-state.md) |
| §21 Bypass | [19-bypass.md](19-bypass.md) |

### For AI Agents

Load individual files by topic instead of this monolithic file to save tokens. Example:
```bash
# Instead of loading 25KB, load only 2KB for timing questions
cat spec/04-timing.md
```

### For Backward Compatibility

All section citations (e.g., "Section 4.2", "§5.1") in other documents remain valid. They refer to the same content, now located in the appropriate sub-document.

---

*Last updated: Split into sub-documents for token efficiency*
