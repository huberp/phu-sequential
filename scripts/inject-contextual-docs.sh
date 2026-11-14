#!/usr/bin/env bash
# inject-contextual-docs.sh
# Dynamically inject relevant documentation based on user's prompt
# for GitHub Copilot Cloud Hooks

set -euo pipefail

# Read the entire input (JSON with prompt)
INPUT=$(cat)

# Extract the prompt and convert to lowercase for matching
PROMPT=$(echo "$INPUT" | jq -r '.prompt // ""' 2>/dev/null | tr '[:upper:]' '[:lower:]')

# If we can't parse the prompt, fall back to TOC injection
if [ -z "$PROMPT" ]; then
  echo '{"additionalContext": "Unable to parse prompt. See spec/README.md and docs/README.md for documentation."}'
  exit 0
fi

# Define mapping of keywords to documentation files
# Format: KEYWORD=filepath
declare -A DOC_MAP=(
  ["sequencer"]="spec/03-sequencer-lanes.md"
  ["lane"]="spec/03-sequencer-lanes.md"
  ["lanes"]="spec/03-sequencer-lanes.md"
  ["timing"]="spec/04-timing.md"
  ["step"]="spec/05-steps.md"
  ["steps"]="spec/05-steps.md"
  ["velocity"]="spec/06-velocity.md"
  ["output routing"]="spec/07-output-routing.md"
  ["voice"]="spec/08-voice-management.md"
  ["transport"]="spec/09-transport.md"
  ["live change"]="spec/10-live-changes.md"
  ["parameter"]="spec/11-parameters.md"
  ["state"]="spec/12-state-presets.md"
  ["preset"]="spec/12-state-presets.md"
  ["ui"]="spec/13-ui.md"
  ["implementation"]="spec/14-implementation.md"
  ["bypass"]="spec/19-bypass.md"
  ["m01"]="docs/architecture/03-sequencing-modules.md"
  ["m02"]="docs/architecture/03-sequencing-modules.md"
  ["m03"]="docs/architecture/03-sequencing-modules.md"
  ["m04"]="docs/architecture/03-sequencing-modules.md"
  ["m05"]="docs/architecture/03-sequencing-modules.md"
  ["m06"]="docs/architecture/03-sequencing-modules.md"
  ["m07"]="docs/architecture/03-sequencing-modules.md"
  ["m08"]="docs/architecture/03-sequencing-modules.md"
  ["m09"]="docs/architecture/03-sequencing-modules.md"
  ["m10"]="docs/architecture/03-sequencing-modules.md"
  ["m11"]="docs/architecture/03-sequencing-modules.md"
  ["m12"]="docs/architecture/04-arpeggiator-modules.md"
  ["m13"]="docs/architecture/04-arpeggiator-modules.md"
  ["m14"]="docs/architecture/04-arpeggiator-modules.md"
  ["m15"]="docs/architecture/04-arpeggiator-modules.md"
  ["m16"]="docs/architecture/04-arpeggiator-modules.md"
  ["m17"]="docs/architecture/04-arpeggiator-modules.md"
  ["m18"]="docs/architecture/03-sequencing-modules.md"
  ["m19"]="docs/architecture/06-juce-adapter.md"
  ["m20"]="docs/architecture/06-juce-adapter.md"
  ["m21"]="docs/architecture/06-juce-adapter.md"
  ["m22"]="docs/architecture/06-juce-adapter.md"
  ["m23"]="docs/architecture/06-juce-adapter.md"
  ["m24"]="docs/architecture/06-juce-adapter.md"
  ["juce"]="docs/architecture/06-juce-adapter.md"
  ["adapter"]="docs/architecture/06-juce-adapter.md"
  ["threading"]="docs/architecture/07-threading.md"
  ["build"]="docs/architecture/08-build-decisions.md"
  ["cmake"]="docs/architecture/08-build-decisions.md"
  ["ci"]="docs/architecture/08-build-decisions.md"
  ["test"]="docs/architecture/11-testing-architecture.md"
  ["design decision"]="docs/architecture/10-design-decisions.md"
  ["ambiguity"]="docs/architecture/12-spec-ambiguities.md"
)

# Collect matching documents
MATCHED_FILES=()

for keyword in "${!DOC_MAP[@]}"; do
  if echo "$PROMPT" | grep -qi "$keyword"; then
    file="${DOC_MAP[$keyword]}"
    # Avoid duplicates
    if [[ ! " ${MATCHED_FILES[@]} " =~ " ${file} " ]]; then
      MATCHED_FILES+=("$file")
    fi
  fi
done

# Build context using jq for proper JSON escaping
if [ ${#MATCHED_FILES[@]} -gt 0 ]; then
  CONTEXT="## Relevant Documentation\n\n"
  for file in "${MATCHED_FILES[@]}"; do
    if [ -f "$file" ]; then
      filename=$(basename "$file")
      CONTEXT+="### $filename\n\n"
      CONTEXT+="$(cat "$file")\n\n"
      CONTEXT+="---\n\n"
    fi
  done
  CONTEXT+="\nTip: See spec/README.md and docs/README.md for full table of contents."
else
  CONTEXT="See spec/README.md for specification details and docs/README.md for architecture. Ask about specific topics like 'sequencer lanes', 'M01', 'timing', etc."
fi

# Output JSON with additionalContext - use jq to properly escape
jq -n --arg ctx "$CONTEXT" '{"additionalContext": $ctx}'
