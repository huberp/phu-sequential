# UI sketches

SVG sketches for the editor described in `spec/arp-spec.md` §14. They are layout and behavior sketches, not final visual design.

| File | Shows |
|---|---|
| `plugin-layout.svg` | Whole editor: global bar, 8 stacked lanes, scrolling, resize handle |
| `lane-chord-follow-incoming.svg` | Chord-follow input, base timing with swing, incoming velocity (drag bars disabled) |
| `lane-channel-latch-by-step.svg` | Channel input, latch on, triplet timing, by-step velocity (0–127) |
| `lane-text-grid-delta.svg` | Text grid timing source (proportional step widths), base-timing controls disabled, delta velocity (−127…+127) |
| `lane-muted-relative-invalid-grid.svg` | Muted lane, relative velocity (0–200 %), invalid text-grid entry marked red |

Common elements per lane: mute (M), latch (L), input mode with follow index or channel, output channel, transpose, gate, Timing Source switch (Base Timing / Grid), base timing, subdivision, swing, step count, velocity mode, text-grid field, step pads (click = on/off), velocity drag bars, playback position indicator (amber frame).
