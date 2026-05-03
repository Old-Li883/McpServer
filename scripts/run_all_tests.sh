#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CPP_FAILED=0
PY_FAILED=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[TEST]${NC} $*"; }
warn() { echo -e "${YELLOW}[TEST]${NC} $*"; }
fail() { echo -e "${RED}[TEST]${NC} $*"; }

# ========== C++ Tests ==========
log "===== Building & Running C++ Tests ====="

if [ ! -d "$PROJECT_ROOT/build" ]; then
    log "Build directory not found, running cmake configure..."
    cmake -B "$PROJECT_ROOT/build" -S "$PROJECT_ROOT"
fi

log "Building C++ project and tests..."
if ! cmake --build "$PROJECT_ROOT/build" 2>&1; then
    fail "C++ build failed!"
    CPP_FAILED=1
else
    log "Running C++ tests (ctest)..."
    if ! (cd "$PROJECT_ROOT/build" && ctest --output-on-failure 2>&1); then
        fail "C++ tests failed!"
        CPP_FAILED=1
    else
        log "C++ tests passed."
    fi
fi

# ========== Python Tests ==========
log "===== Running Python Tests ====="

if ! command -v python3 &>/dev/null; then
    warn "python3 not found, skipping Python tests."
elif [ ! -d "$PROJECT_ROOT/venv" ]; then
    warn "Python venv not found, skipping Python tests. Create it with: python3 -m venv venv && source venv/bin/activate && pip install -e '.[dev]'"
else
    source "$PROJECT_ROOT/venv/bin/activate"

    if ! python3 -m pytest "$PROJECT_ROOT/tests" "$PROJECT_ROOT/agent/llm/tests" \
         -v --tb=short 2>&1; then
        fail "Python tests failed!"
        PY_FAILED=1
    else
        log "Python tests passed."
    fi

    deactivate 2>/dev/null || true
fi

# ========== Summary ==========
echo ""
echo "========================================"
if [ $CPP_FAILED -eq 0 ] && [ $PY_FAILED -eq 0 ]; then
    log "All tests passed!"
    exit 0
else
    fail "Some tests failed:"
    [ $CPP_FAILED -ne 0 ] && fail "  - C++ tests"
    [ $PY_FAILED -ne 0 ]  && fail "  - Python tests"
    exit 1
fi
