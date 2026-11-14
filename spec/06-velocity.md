# ARP Plugin Specification v2 - Velocity

## 7. Velocity

Each lane has exactly one velocity mode. The default mode is "incoming velocity". Each step stores its own value per velocity mode; switching the mode restores the step values last set in that mode. No mode switch overwrites the values of another mode.

### 7.1 Modes

Incoming velocity: the note receives the velocity of the input note at the step trigger time; the step values of this mode remain unused. By step: each step has an absolute velocity value from 0 to 127. Delta by step: each step has a delta value from -127 to +127. The delta value is added to the velocity of the input note. Relative: each step has a percentage value from 0 to 200. The note receives the input velocity times the percentage value, divided by 100.

### 7.2 Clamping

All calculated velocity values are clamped to 0 to 127. A note with a calculated velocity of 0 is not sent: that step is silent, the step position still advances, and sounding voices of the lane are unaffected.

### 7.3 Sampling

The input velocity is sampled at the step trigger; with latch (Section 4.6), the velocity of the most recently followed note applies. Velocity changes during a sounding step do not affect the note that has already been triggered.
