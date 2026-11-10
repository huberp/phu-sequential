# Copilot Instructions for phu-sequential

**When making changes to the plugin, always:**

1. **Consult `spec/arp-spec.md`** — the single source of truth for plugin behavior and architecture (§2, §15, §16, §18).
2. **Consult `docs/architecture.md`** — the specification for directory layout, target names, namespaces, and module interfaces.
3. **Report any deviations** — if you introduce behavior or interfaces that differ from the spec or architecture document, tell the user explicitly.
4. **Keep documentation updated** — if the architecture document differs from what you build (names, interfaces, module organization), update `docs/architecture.md` in the same commit.
