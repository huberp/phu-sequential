# ARP Plugin Specification v2 - Testability

## 18. Testability

Unit tests are mandatory. The test framework is Catch2. The reusable modules are testable without a host and without JUCE (Section 16.1). Timing math, grid parser, velocity calculation, index determination, and voice management have their own tests. Edge cases of the specification (index greater than chord, chord re-sorting, velocity clamping, overlap, retrigger policy, transport reset, playhead discontinuity handling, cross-lane voice retrigger and ownership transfer, per-note pass-through tracking, Note-Off guarantee, voice limit, state version migration) are test cases.
