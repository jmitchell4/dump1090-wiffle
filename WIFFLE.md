# Wiffle CSV 

The name `wiffle` was chosen due to an AI prompt to ask for a playful but fast CSV output.  

The Wiffle CSV output is a normalized CSV output to align a fork of `dump1090` and `dump978`.  This output is provided both via an optional stdout as well as a TCP service. 

The CSV delimiter is a comma, with no space between fields. 

| Column | Field | Description | Example |
|---|---|---|---|
| 1 | Link Type | Indicator of 1090 vs UAT | `1090`, `uat-`, `uat+` | 
| 2 | System Time | System time of message as ISO string format | `2025-08-05T01:09:46.775Z` |
| 3 | System Time Epoch | System time as epoch milliseconds | `1754356186775` |
| 4 | ICAO Address | ICAO address of the squit | `ac578c` |
| 5 | Message Type | Either indicator of the address type (1090) or the address qualifier (UAT) | `ADSB0`, `TISB3` |
| 6 | DF (1090) / Downlink/Uplink (UAT) | Either DF (1090) or downlink/uplink (UAT) | `DF17`, `UP2` |
| 7 | RSSI | RSSI signal level | `-23.0` |
| 8 | Raw Squitter | Original squitter in hexadecimal | `02a183974a3177` |

Example output: 

    1090,2025-08-05T01:09:46.775Z,1754356186775,ac578c,ADSB0,DF0,-23.0,02a183974a3177

## Detailed Fields 

### Link Type 

* 1090 = 1090 link 
* uat- = UAT downlink 
* uat+ = UAT uplink 

### Message Type 

**1090** 

* 0,1 = ADSB
* 2,5 = ADSR
* 3,6,7 = TISB
* otherwise, UNKN (unknown)

**UAT** 

* 0,1 = ADSB
* 4 = VEH (vehicle)
* 5 = FIXED (fixed beacon)
* 2,3 = TISB
* 6 = ADSR
* 7 = RES (reserved)
* otherwise, UNKN (unknown)

### Downlink/Uplink (UAT) 

* 0 = DS (downlink short)
* 1 = DL (downlink long)
* 2 = UP (uplink)
* 3 = MET (metadata)
* otherwise, UKN (unknown)

