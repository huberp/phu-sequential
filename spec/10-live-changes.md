# ARP Plugin Specification v2 - Live Changes

## 11. Live Changes

All changes while the sequencer is running are glitch-free. Changes to step count, grid, or Timing Source take effect at the next step start of the respective lane (phase re-anchoring according to Section 5.4). Changes to step on/off and velocity values take effect immediately. Changes to the input mode, Follow Index, and Follow Channel take effect at the next step start (Section 4.4), except that a release of the source stops the lane immediately. Changes to Mute and Latch take effect immediately (Section 4.6). A change of the velocity mode affects newly triggered notes. Changes to transpose, gate, swing, and output channel affect newly triggered notes; notes already sounding keep their pitch, their velocity, and their scheduled Note-Off time, so that the Note-Off guarantee (Section 9.3) is upheld.
