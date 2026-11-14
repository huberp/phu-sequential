# phu-sequential — Architecture

**Directory layout, targets, namespaces, interfaces.** Source of truth for implementation.
If this document and the spec disagree, **the spec wins** (see `spec/README.md`).

This document has been split into focused sub-documents for better token efficiency and navigation.

## Quick Navigation

Use `docs/module-map.md` for token-efficient module lookup.

### Core Structure

| Section | Document | Description | Size |
|---------|----------|-------------|------|
| §1 | [Layers & Dependencies](architecture/01-layers-and-dependencies.md) | Layers, directories, namespaces, targets, dependency graph | ~7.5 KB |
| §2 | [Units & Data Types](architecture/02-units-and-types.md) | Units, numeric types, core data types | ~2.8 KB |
| §3 | [Sequencing Modules](architecture/03-sequencing-modules.md) | M01-M11, M18 (lib/sequencing) | ~36 KB |
| §4 | [Arpeggiator Modules](architecture/04-arpeggiator-modules.md) | M12-M17 (lib/arpeggiator) | ~33 KB |
| §5 | [Per-Block Flow](architecture/05-per-block-flow.md) | SequencerCore::processBlock flow | ~10.6 KB |
| §6 | [JUCE Adapter](architecture/06-juce-adapter.md) | M19-M24 (src/) | ~39 KB |

### Cross-Cutting Concerns

| Section | Document | Description | Size |
|---------|----------|-------------|------|
| §7 | [Threading Model](architecture/07-threading.md) | Threading model (§17) | ~5.3 KB |
| §8 | [Build Decisions](architecture/08-build-decisions.md) | Build/CMake/CI (§15) — D-38 | ~6.4 KB |
| §9 | [Issue → Module Map](architecture/09-issue-to-module-map.md) | Issue to module mapping | ~2.7 KB |

### Reference

| Section | Document | Description | Size |
|---------|----------|-------------|------|
| §10 | [Design Decisions](architecture/10-design-decisions.md) | D-1 through D-54 index | ~12 KB |
| §11 | [Testing Architecture](architecture/11-testing-architecture.md) | Testing architecture (§18) | ~3.4 KB |
| §12 | [Spec Ambiguities](architecture/12-spec-ambiguities.md) | A-n ambiguities | ~9.7 KB |

---

## For AI Agents

Load individual files by topic instead of the 170KB monolith to save tokens. Examples:

```bash
# For module M14 (LaneEngine):
cat docs/architecture/04-arpeggiator-modules.md

# For threading questions:
cat docs/architecture/07-threading.md

# For build/CI questions:
cat docs/architecture/08-build-decisions.md
```

## Backward Compatibility

All section citations (e.g., "§3.1", "§4.3") in other documents remain valid. They refer to the same content, now located in the appropriate sub-document.

The original `docs/architecture.md` file is maintained as a redirect for backward compatibility.

---

**Note:** See `spec/README.md` for the specification (source of truth for behavior).
