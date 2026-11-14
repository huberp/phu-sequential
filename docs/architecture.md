# phu-sequential — Architecture

**This document has been split into focused sub-documents for better token efficiency and navigation.**

> **Note:** This file is maintained for backward compatibility. All existing references to `docs/architecture.md` will continue to work, but the content has been reorganized into the `docs/architecture/` directory.

## New Structure

Please refer to the new split documentation:

- **[docs/README.md](README.md)** — Main architecture table of contents

### Quick Access by Section

| Original Section | New Document | Size |
|-----------------|--------------|------|
| §1 Layers & Dependencies | [architecture/01-layers-and-dependencies.md](architecture/01-layers-and-dependencies.md) | ~7.5 KB |
| §2 Units & Data Types | [architecture/02-units-and-types.md](architecture/02-units-and-types.md) | ~2.8 KB |
| §3 Sequencing Modules (M01-M11, M18) | [architecture/03-sequencing-modules.md](architecture/03-sequencing-modules.md) | ~36 KB |
| §4 Arpeggiator Modules (M12-M17) | [architecture/04-arpeggiator-modules.md](architecture/04-arpeggiator-modules.md) | ~33 KB |
| §5 Per-Block Flow | [architecture/05-per-block-flow.md](architecture/05-per-block-flow.md) | ~10.6 KB |
| §6 JUCE Adapter (M19-M24) | [architecture/06-juce-adapter.md](architecture/06-juce-adapter.md) | ~39 KB |
| §7 Threading Model | [architecture/07-threading.md](architecture/07-threading.md) | ~5.3 KB |
| §8 Build Decisions | [architecture/08-build-decisions.md](architecture/08-build-decisions.md) | ~6.4 KB |
| §9 Issue → Module Map | [architecture/09-issue-to-module-map.md](architecture/09-issue-to-module-map.md) | ~2.7 KB |
| §10 Design Decisions (D-n) | [architecture/10-design-decisions.md](architecture/10-design-decisions.md) | ~12 KB |
| §11 Testing Architecture | [architecture/11-testing-architecture.md](architecture/11-testing-architecture.md) | ~3.4 KB |
| §12 Spec Ambiguities (A-n) | [architecture/12-spec-ambiguities.md](architecture/12-spec-ambiguities.md) | ~9.7 KB |

### For Module-Specific Lookup

Use `docs/module-map.md` for token-efficient module lookup with direct links to architecture sections.

### For AI Agents

Load individual files by topic instead of this 170KB monolith to save tokens. Example:
```bash
# Instead of loading 170KB, load only 33KB for arpeggiator modules
cat docs/architecture/04-arpeggiator-modules.md
```

### For Backward Compatibility

All section citations (e.g., "§3.1", "§4.3", "D-12") in other documents remain valid. They refer to the same content, now located in the appropriate sub-document.

All module citations (e.g., "M14", "M01") can be found via `docs/module-map.md`.

---

**Source of truth for behavior:** `spec/arp-spec.md` (cited as §n). If this document and the spec disagree, **the spec wins**; fix this document in the same PR.

*Last updated: Split into sub-documents for token efficiency*
