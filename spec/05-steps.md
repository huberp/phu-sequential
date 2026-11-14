# ARP Plugin Specification v2 - Steps

## 6. Steps

Each step has an on/off switch. An off step produces no note and is silent for its duration. An on step plays a note. Each lane has a gate parameter in percent. The note length is the step length times the gate value, divided by 100. The gate value is between 1 and 200. The default is 100 percent. With gate above 100 percent, notes overlap; overlap is allowed. A lane is polyphonic. The gate parameter is configurable per lane and automatable.
