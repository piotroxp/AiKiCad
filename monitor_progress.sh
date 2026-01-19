#!/bin/bash
# Monitor repository changes for 30 cycles of 1 minute each

REPO_PATH="/nvme3/KiCad/kicad-source-mirror"
REVIEW_FILE="AI-AGENT-FOR-KICAD-REVIEW.md"
CYCLE=1
MAX_CYCLES=30

cd "$REPO_PATH" || exit 1

# Initial state
LAST_STATUS=$(git status --short 2>/dev/null | md5sum)
LAST_DIFF=$(git diff --stat 2>/dev/null | md5sum)

echo "Starting monitoring - Cycle 1/$MAX_CYCLES"
echo "Timestamp: $(date)"

while [ $CYCLE -le $MAX_CYCLES ]; do
    sleep 60
    
    CURRENT_STATUS=$(git status --short 2>/dev/null | md5sum)
    CURRENT_DIFF=$(git diff --stat 2>/dev/null | md5sum)
    
    if [ "$CURRENT_STATUS" != "$LAST_STATUS" ] || [ "$CURRENT_DIFF" != "$LAST_DIFF" ]; then
        echo ""
        echo "=== CHANGES DETECTED - Cycle $CYCLE/$MAX_CYCLES ==="
        echo "Timestamp: $(date)"
        git status --short
        git diff --stat
        echo ""
    else
        echo "Cycle $CYCLE/$MAX_CYCLES: No changes detected - $(date)"
    fi
    
    LAST_STATUS="$CURRENT_STATUS"
    LAST_DIFF="$CURRENT_DIFF"
    CYCLE=$((CYCLE + 1))
done

echo ""
echo "Monitoring complete - $MAX_CYCLES cycles completed"
echo "Final timestamp: $(date)"
