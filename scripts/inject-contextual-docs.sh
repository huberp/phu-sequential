#!/usr/bin/env bash
# inject-contextual-docs.sh
# userPromptSubmitted hook: inject small, relevant navigation hints based on the prompt.
# stdin:  JSON {"prompt": "..."}
# stdout: {"additionalContext": "..."} or {} when nothing matches.
# Token policy: never inline large docs; emit module-map rows and file pointers, capped at MAX_BYTES.
# Portable to bash 3.2 (macOS): no associative arrays. Requires jq.

set -euo pipefail

MAX_BYTES=8000

PROMPT=$(jq -r '.prompt // ""' 2>/dev/null | tr '[:upper:]' '[:lower:]' || true)
if [ -z "$PROMPT" ]; then
  echo '{}'
  exit 0
fi

has_word() { printf '%s' "$PROMPT" | grep -Eqiw "$1"; }

# Topic rules: "<ERE of whole words>|<file>|<hint>"
RULES=(
  'sequencers?|lanes?|latch|channel mode|chord.follow|spec/03-sequencer-lanes.md|spec §4'
  'timing|grid|swing|subdivision|spec/04-timing.md|spec §5'
  'steps?|gate|spec/05-steps.md|spec §6'
  'velocity|spec/06-velocity.md|spec §7'
  'output routing|transpose|spec/07-output-routing.md|spec §8'
  'voices?|note-?offs?|spec/08-voice-management.md|spec §9'
  'transport|spec/09-transport.md|spec §10'
  'live changes?|spec/10-live-changes.md|spec §11'
  'parameters?|automation|spec/11-parameters.md|spec §12'
  'state|presets?|spec/12-state-presets.md|spec §13'
  'ui|editor|spec/13-ui.md|spec §14'
  'implementation|spec/14-implementation.md|spec §15-16'
  'robustness|real-?time|spec/15-robustness.md|spec §17'
  'testability|spec/16-testability.md|spec §18'
  'code style|spec/17-code-style.md|spec §19'
  'initial state|defaults?|spec/18-initial-state.md|spec §20'
  'bypass|spec/19-bypass.md|spec §21'
  'input processing|chords?|channel filter|pass-?through|spec/02-input-processing.md|spec §3'
  'arp|arpeggiator|docs/architecture/04-arpeggiator-modules.md|M12-M17'
  'juce|adapters?|docs/architecture/06-juce-adapter.md|M19-M24'
  'threading|docs/architecture/07-threading.md|arch §7'
  'build|cmake|ci|docs/architecture/08-build-decisions.md|arch §8'
  'tests?|docs/architecture/11-testing-architecture.md|arch §11 (rows: docs/test-matrix.md)'
  'design decisions?|docs/architecture/10-design-decisions.md|D-n index'
  'ambiguit(y|ies)|docs/architecture/12-spec-ambiguities.md|A-n'
)

OUT=""
POINTERS=""
for rule in "${RULES[@]}"; do
  hint=${rule##*|}; rest=${rule%|*}
  file=${rest##*|}; words=${rest%|*}
  [ -f "$file" ] || continue
  if has_word "$words"; then
    case "$POINTERS" in *"$file"*) continue ;; esac
    POINTERS+="- $file ($hint)"$'\n'
  fi
done

ROWS=""
i=1
while [ "$i" -le 24 ]; do
  id=$(printf 'm%02d' "$i")
  if has_word "$id"; then
    row=$(grep -i "^| ${id} " docs/module-map.md 2>/dev/null || true)
    [ -n "$row" ] && ROWS+="$row"$'\n'
  fi
  i=$((i + 1))
done

if [ -z "$POINTERS" ] && [ -z "$ROWS" ]; then
  echo '{}'
  exit 0
fi

if [ -n "$ROWS" ]; then
  OUT+="## Module rows (docs/module-map.md)"$'\n\n'
  OUT+=$(sed -n '9,10p' docs/module-map.md)$'\n'"$ROWS"$'\n'
fi
if [ -n "$POINTERS" ]; then
  OUT+="## Relevant docs (do not read whole files)"$'\n'"$POINTERS"
fi
OUT+=$'\n'"Read only needed ranges: grep -n '^##' <file>, then view with view_range."$'\n'

if [ "${#OUT}" -gt "$MAX_BYTES" ]; then
  OUT="${OUT:0:$MAX_BYTES}"$'\n... (truncated)\n'
fi

jq -n --arg ctx "$OUT" '{"additionalContext": $ctx}'
