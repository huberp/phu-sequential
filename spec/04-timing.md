# ARP Plugin Specification v2 - Timing

## 5. Timing

### 5.1 Base Timing

A lane uses a base timing from a list. The list contains 1/32, 1/16, 1/8, 1/4, 1/2, and 1/1. In addition, the user selects a subdivision: straight, triplet, or dotted. A swing amount of 0 to 100 percent is adjustable. Swing shifts only the start position of steps with an even 1-based index (the 2nd, 4th, 6th, … step; odd when counted 0-based) backward; step 1 is never shifted. Counting restarts at step 1 for each loop. The shift is at most half the step length: 100 percent swing corresponds to a shift of 50 percent of the step length. Note lengths remain unchanged; the grid start positions of all other steps remain unchanged. As a result, a shifted note may extend into the following step; this overlap is allowed (voice management according to Section 9). Swing applies only to base timing and not to the text grid. The base timing and the subdivision determine the grid and the length of each step.

### 5.2 Custom Grid (Text Grid)

Alternatively, the user can enter a grid as text. Each lane has a Timing Source switch (Base Timing or Text Grid) that selects which source defines the lane's grid. The inactive source keeps its settings (base timing parameters or the text grid content). Each lane has one step data array shared by both sources; when the lane switches source, or the active source's step count changes, the step data is transferred position by position (surplus data discarded, missing steps receive defaults), with the same live-change timing as Section 11. The format is a comma-separated list of n/d fractions. An optional suffix t denotes triplet, an optional suffix . denotes dotted; the two suffixes are mutually exclusive. Example: "1/8t, 1/8., 1/4" defines 3 steps. Only musical values are allowed: the numerator is exactly 1, and the denominator is one of 1, 2, 4, 8, 16, 32, 64; each entry may carry an optional suffix t (triplet) or . (dotted). Other fractions (for example 3/8) are invalid. The total loop length is not restricted. A text grid has at most 64 entries. Each entry defines the length and grid position of one step. The loop length of the lane is the sum of all step lengths. When a new text grid is applied, the step data (on/off and velocity values) are transferred position by position to the new steps; surplus data is discarded, and missing steps receive defaults (on, default velocity of the active mode).

### 5.3 Text Grid Validation

Invalid entries are marked red in the UI. A grid with invalid entries is not applied. The lane keeps its last valid grid.

### 5.4 Step Count

A lane with base timing has a configurable step count. The step count is between 1 and 64. The step count is freely selectable per lane. Example: one lane with 16 steps, another lane with 7 steps. The loop length is the step count times the step length. When the step count is increased, the new steps receive the state on and default velocity values. When it is decreased, the data of the removed steps is discarded; a later increase does not restore old data. When a step count or grid change takes effect at the next step start, the lane's phase offset (Section 10) is re-anchored so that the current step index is kept modulo the new step count, and the new loop starts at that step start. No step is skipped or repeated by the re-anchoring itself.
