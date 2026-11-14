# GitHub Copilot Hooks for Documentation Injection

This directory contains hooks that automatically inject relevant project documentation into GitHub Copilot agent sessions.

## Purpose

To improve token efficiency and AI agent effectiveness by:
- Injecting documentation context automatically
- Providing relevant docs based on user prompts
- Reducing the need for manual prompt engineering

## Hooks Configured

### 1. `sessionStart` Hook
- **Script**: `scripts/inject-docs.sh`
- **Trigger**: At the start of every Copilot session
- **Action**: Injects the Table of Contents from both specification and architecture documentation
- **Output**: ~5KB of navigation context

### 2. `userPromptSubmitted` Hook  
- **Script**: `scripts/inject-contextual-docs.sh`
- **Trigger**: When user submits a prompt
- **Action**: Dynamically injects relevant documentation based on keywords in the prompt
- **Output**: Only the specific documentation files that match the user's query

## Keyword Mapping

The `inject-contextual-docs.sh` script maps keywords to specific documentation files:

| Keywords | Documentation File |
|----------|-------------------|
| sequencer, lane, lanes | spec/03-sequencer-lanes.md |
| timing | spec/04-timing.md |
| step, steps | spec/05-steps.md |
| velocity | spec/06-velocity.md |
| voice | spec/08-voice-management.md |
| transport | spec/09-transport.md |
| M01-M11, M18 | docs/architecture/03-sequencing-modules.md |
| M12-M17 | docs/architecture/04-arpeggiator-modules.md |
| M19-M24, juce, adapter | docs/architecture/06-juce-adapter.md |
| threading | docs/architecture/07-threading.md |
| build, cmake, ci | docs/architecture/08-build-decisions.md |
| test | docs/architecture/11-testing-architecture.md |
| design decision | docs/architecture/10-design-decisions.md |

## Token Savings

| Scenario | Without Hooks | With Hooks | Savings |
|----------|---------------|------------|---------|
| General question | ~195KB (both docs) | ~5KB (TOCs) | 97% |
| Specific topic (e.g., sequencer) | ~25KB (full spec) | ~3.5KB (specific file) | 86% |
| Module question (e.g., M01) | ~170KB (full arch) | ~36KB (specific file) | 79% |

## Requirements

- `jq` must be installed on the system
- Hooks configuration file must be on the default branch
- For cloud agents: hooks run in Linux sandbox at `/workspace`

## Testing

Test locally with Copilot CLI:

```bash
# Test sessionStart hook
echo '{}' | ./scripts/inject-docs.sh | jq .

# Test userPromptSubmitted hook with a prompt
echo '{"prompt": "How do sequencer lanes work?"}' | ./scripts/inject-contextual-docs.sh | jq .
```

## Cloud Agent Compatibility

✅ Both hooks are fully compatible with GitHub Copilot Cloud Agents:
- Run in the ephemeral Linux sandbox
- Access files from `/workspace` (cloned repository)
- Only `bash` field is honored (not `powershell`)
- Output limit: 10 MiB per hook

## Files

- `.github/hooks/doc-injection.json` - Hook configuration
- `scripts/inject-docs.sh` - Session start injection script
- `scripts/inject-contextual-docs.sh` - Dynamic prompt-based injection script

## References

- [GitHub Copilot Hooks Documentation](https://docs.github.com/en/copilot/how-tos/copilot-on-github/customize-copilot/customize-cloud-agent/use-hooks)
- [Hooks Reference](https://docs.github.com/en/copilot/reference/hooks-reference)
