# ARP Plugin Specification v2 - Robustness and Real-Time

## 17. Robustness and Real-Time

The audio thread performs no allocations, locks, or blocking calls. Parameter changes travel from the UI thread to the audio thread via atomic values or lock-free queues. The core stores time exclusively in beats (PPQ), never in samples and never as wall-clock time; conversion to samples happens exclusively at the output boundary for buffer offsets (Sections 10, 16.2). The core validates all input values; invalid values are rejected or clamped. RAII applies everywhere; raw new/delete are not permitted. The core throws no exceptions in the audio path.
