#!/bin/bash

echo "========================================"
echo "AR Filter Head Angle Diagnostic Test"
echo "========================================"
echo ""
echo "INSTRUCTIONS:"
echo "1. App will start and show video"
echo "2. Position your head as instructed below"
echo "3. Press '2' to activate beanie filter"
echo "4. Wait ~3 seconds while it logs data"
echo "5. Press CTRL+C to stop"
echo "6. Repeat for each test position"
echo ""
echo "We will test 3 positions:"
echo "  - STRAIGHT ahead"
echo "  - Turn RIGHT (45 degrees)"
echo "  - Turn LEFT (45 degrees)"
echo ""

run_test() {
  local position=$1
  local logfile=$2
  
  echo ""
  echo "========================================"
  echo "TEST: Looking $position"
  echo "========================================"
  echo "Position your head $position, then press '2' to activate filter"
  echo "After ~3 seconds, press CTRL+C"
  echo ""
  read -p "Press ENTER when ready..."
  
  ./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt 2>&1 | grep "\[ANCHOR DEBUG\]" > "$logfile" &
  local pid=$!
  
  echo "App running (PID $pid). Press '2' to activate filter, then CTRL+C after ~3 seconds"
  wait $pid 2>/dev/null
  
  echo "✓ Test complete. Captured $(wc -l < "$logfile") log lines"
}

# Run tests
run_test "STRAIGHT" "/tmp/segmecam_straight.log"
run_test "RIGHT (45°)" "/tmp/segmecam_right.log"
run_test "LEFT (45°)" "/tmp/segmecam_left.log"

echo ""
echo "========================================"
echo "All tests complete!"
echo "========================================"
echo ""
echo "Comparing first line from each test:"
echo ""

echo "=== STRAIGHT ==="
head -1 /tmp/segmecam_straight.log

echo ""
echo "=== RIGHT ==="
head -1 /tmp/segmecam_right.log

echo ""
echo "=== LEFT ==="
head -1 /tmp/segmecam_left.log

echo ""
echo "Full logs at:"
echo "  /tmp/segmecam_straight.log"
echo "  /tmp/segmecam_right.log"
echo "  /tmp/segmecam_left.log"

