# ARP Plugin Specification v2 - Overview and Plugin Format

## 1. Overview

The plugin is an arpeggiator sequencer implemented as a VST3 MIDI effect. The plugin receives MIDI notes as input, sorts them, and feeds them into exactly 8 independent sequencer lanes. Each lane generates its own sequence of output notes from its input note. The plugin has no audio path. The plugin processes MIDI events exclusively.

## 2. Plugin Format and Category

The target format is VST3. The plugin is implemented as a MIDI effect (event input, event output, no audio path). The plugin uses the VST3 category for MIDI effects. The plugin supports state saving according to Section 13.
