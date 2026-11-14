# ARP Plugin Specification v2 - Bypass Behavior

## 21. Bypass Behavior

On host bypass, the plugin passes all input events through to the output unchanged. On entering bypass, all notes passed through so far (Section 3.3) and all sounding lane notes immediately receive Note-Offs, and the pass-through tracking is cleared; afterwards everything is passed raw. All lanes stop immediately; latch ends (Section 4.6). When bypass ends, each lane takes over the position derived from the host playhead (Section 10). No reset to step 1 takes place.
