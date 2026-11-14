# ARP Plugin Specification v2 - Parameters and Automation

## 12. Parameters and Automation

Only finite, named parameters are automatable.

Global parameters: Thru, Consume, Chord Input Channel.

Lane parameters (per lane): Mute, Latch, Input Mode, Follow Index (0 to 15), Follow Channel, Output Channel, Transpose, Gate, Timing Source, Base Timing, Subdivision, Swing Amount, Step Count, Velocity Mode.

Step data (on/off, velocity values per step and mode), the text grid, and the lane color (an opaque RGB value per lane) are non-automatable plugin state (Section 13). The text grid and the lane color are not host parameters.
