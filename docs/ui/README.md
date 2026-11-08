# UI sketches

SVG sketches for the editor described in `spec/arp-spec.md` §14. They are layout and behavior sketches, not final visual design.

| File | Shows |
|---|---|
| `plugin-layout.svg` | Whole editor: global bar, 8 stacked lanes in their individual lane colors (lane 5 has 7 steps, lane 7 has 12, the others 16), scrolling, resize handle |
| `lane-chord-follow-incoming.svg` | Blue lane: chord-follow input, base timing with swing, incoming velocity (drag bars disabled) |
| `lane-channel-latch-by-step.svg` | Green lane: channel input, latch on, triplet timing, by-step velocity (0–127) |
| `lane-text-grid-delta.svg` | Purple lane: text grid timing source (proportional step widths), base-timing controls disabled, delta velocity (−127…+127) |
| `lane-muted-relative-invalid-grid.svg` | Pink lane: muted, relative velocity (0–200 %), invalid text-grid entry marked red |

Common elements per lane: mute (M), latch (L), input mode with follow index or channel, output channel, transpose, gate, color picker (lane color of step pads and velocity bars), Timing Source switch (Base Timing / Grid), base timing, subdivision, swing, step count, velocity mode, text-grid field, step pads (click = on/off), velocity drag bars, playback position indicator (amber frame). Lane colors follow the default palette in lane order (blue, green, purple, pink, teal, indigo, lime, brown).
