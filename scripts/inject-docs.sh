#!/usr/bin/env bash
# inject-docs.sh
# Inject core documentation at session start for GitHub Copilot
# This provides baseline context for all sessions

set -euo pipefail

# Check if we're in the right directory
if [ ! -f "spec/README.md" ] || [ ! -f "docs/README.md" ]; then
  echo '{"additionalContext": "Project documentation not found in current directory."}'
  exit 0
fi

# Read TOCs - these are small and provide navigation
SPEC_TOC=$(cat spec/README.md 2>/dev/null || echo "")
ARCH_TOC=$(cat docs/README.md 2>/dev/null || echo "")

# Build the context using jq to properly escape everything
# We use --arg to pass the TOCs as arguments, which jq will properly escape
jq -n --arg spec "$SPEC_TOC" --arg arch "$ARCH_TOC" '
  {
    additionalContext: (
      "## Project Documentation Overview\n\n" +
      "### Specification (see spec/README.md)\n" +
      $spec + "\n\n" +
      "### Architecture (see docs/README.md)\n" +
      $arch + "\n\n" +
      "---\n" +
      "Tip: Ask for specific sections (e.g., sequencer lanes, M01, timing) for detailed information."
    )
  }'
