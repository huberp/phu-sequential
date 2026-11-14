#!/usr/bin/env bash
# inject-docs.sh
# Inject core documentation at session start for GitHub Copilot
# This provides baseline context for all sessions

set -euo pipefail

# Check if we're in the right directory
if [ ! -f "spec/README.md" ] || [ ! -f "docs/module-map.md" ]; then
  echo '{"additionalContext": "Project documentation not found in current directory."}'
  exit 0
fi

# Read spec TOC and module-map - these are small and provide navigation
SPEC_TOC=$(cat spec/README.md 2>/dev/null || echo "")
MODULE_MAP=$(cat docs/module-map.md 2>/dev/null || echo "")

# Build the context using jq to properly escape everything
# We use --arg to pass the content as arguments, which jq will properly escape
jq -n --arg spec "$SPEC_TOC" --arg modules "$MODULE_MAP" '
  {
    additionalContext: (
      "## Project Documentation\n\n" +
      "### Specification TOC (full details in spec/*.md)\n" +
      $spec + "\n\n" +
      "### Module Map — Quick Reference\n" +
      $modules + "\n\n" +
      "### Navigation\n" +
      "The specs and architectures are split into sub-docs for efficiency. Use:\n" +
      "- grep -n \"^## \" spec/*.md to find spec sections\n" +
      "- grep -n \"^### \" docs/architecture/*.md to find architecture subsections\n" +
      "- docs/README.md and spec/README.md are TOCs; they link to the actual content files\n"
    )
  }'
