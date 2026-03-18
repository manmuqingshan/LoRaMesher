#!/usr/bin/env bash
# Run LLVM instrumented tests and generate a coverage report.
# Usage: bash scripts/run_coverage.sh [--no-html]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROFILE_DIR="$ROOT/.pio/coverage"
PROFDATA="$PROFILE_DIR/loramesher.profdata"
BINARY="$ROOT/.pio/build/test_native_profile/program"
HTML_DIR="$ROOT/coverage"

# Prefer versioned binaries (installed by apt llvm-18); fall back to unversioned.
PROFDATA_BIN="$(command -v llvm-profdata-18 2>/dev/null || command -v llvm-profdata)"
COV_BIN="$(command -v llvm-cov-18 2>/dev/null || command -v llvm-cov)"

echo "==> Running instrumented tests..."
LLVM_PROFILE_FILE="$PROFILE_DIR/%e-%p.profraw" \
  pio test -e test_native_profile -v

echo "==> Merging profile data..."
"$PROFDATA_BIN" merge -sparse "$PROFILE_DIR"/*.profraw -o "$PROFDATA"

echo "==> Text summary:"
"$COV_BIN" report "$BINARY" \
  -instr-profile="$PROFDATA" \
  --ignore-filename-regex='(test/|googletest|\.pio)'

if [[ "${1:-}" != "--no-html" ]]; then
  echo "==> Generating HTML report in $HTML_DIR ..."
  "$COV_BIN" show "$BINARY" \
    -instr-profile="$PROFDATA" \
    -format=html \
    -output-dir="$HTML_DIR" \
    --ignore-filename-regex='(test/|googletest|\.pio)'
  echo "==> Done. Open $HTML_DIR/index.html in your browser."
fi
