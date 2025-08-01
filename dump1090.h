// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// dump1090.h: main program header
//
// Copyright (c) 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// This file incorporates work covered by the following copyright and
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef __DUMP1090_H
#define __DUMP1090_H

// Default version number, if not overriden by the Makefile
#ifndef MODES_DUMP1090_VERSION
# define MODES_DUMP1090_VERSION     "unknown"
#endif

#ifndef MODES_DUMP1090_VARIANT
# define MODES_DUMP1090_VARIANT     "dump1090-unknown"
#endif

// ============================= Include files ==========================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <limits.h>
#include <strings.h>

#include "compat/compat.h"
#include "dsp/generated/starch.h"

// ============================= #defines ===============================

#define MODES_DEFAULT_FREQ         1090000000
#define MODES_DEFAULT_WIDTH        1000
#define MODES_DEFAULT_HEIGHT       700
#define MODES_RTL_BUFFERS          15                         // Number of RTL buffers
#define MODES_RTL_BUF_SIZE         (16*16384)                 // 256k
#define MODES_MAG_BUF_SAMPLES      (MODES_RTL_BUF_SIZE / 2)   // Each sample is 2 bytes
#define MODES_MAG_BUFFERS          12                         // Number of magnitude buffers (should be smaller than RTL_BUFFERS for flowcontrol to work)
#define MODES_LEGACY_AUTO_GAIN     -10                        // old gain value for "use automatic gain"
#define MODES_DEFAULT_GAIN         999999                     // Use default SDR gain
#define MODES_MSG_SQUELCH_DB       4.0                        // Minimum SNR, in dB
#define MODES_MSG_ENCODER_ERRS     3                          // Maximum number of encoding errors

#define MODEAC_MSG_SAMPLES       (25 * 2)                     // include up to the SPI bit
#define MODEAC_MSG_BYTES          2
#define MODEAC_MSG_SQUELCH_LEVEL  0x07FF                      // Average signal strength limit

#define MODES_PREAMBLE_US        8              // microseconds = bits
#define MODES_PREAMBLE_SAMPLES  (MODES_PREAMBLE_US       * 2)
#define MODES_PREAMBLE_SIZE     (MODES_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_LONG_MSG_BYTES     14
#define MODES_SHORT_MSG_BYTES    7
#define MODES_LONG_MSG_BITS     (MODES_LONG_MSG_BYTES    * 8)
#define MODES_SHORT_MSG_BITS    (MODES_SHORT_MSG_BYTES   * 8)
#define MODES_LONG_MSG_SAMPLES  (MODES_LONG_MSG_BITS     * 2)
#define MODES_SHORT_MSG_SAMPLES (MODES_SHORT_MSG_BITS    * 2)
#define MODES_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define MODES_OS_PREAMBLE_SAMPLES  (20)
#define MODES_OS_PREAMBLE_SIZE     (MODES_OS_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_OS_LONG_MSG_SAMPLES  (268)
#define MODES_OS_SHORT_MSG_SAMPLES (135)
#define MODES_OS_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_OS_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define MODES_OUT_BUF_SIZE         (1500)
#define MODES_OUT_FLUSH_SIZE       (MODES_OUT_BUF_SIZE - 256)
#define MODES_OUT_FLUSH_INTERVAL   (60000)

#define MODES_USER_LATLON_VALID (1<<0)

#define INVALID_ALTITUDE (-9999)

/* Where did a bit of data arrive from? In order of increasing priority */
typedef enum {
    SOURCE_INVALID,        /* data is not valid */
    SOURCE_MODE_AC,        /* A/C message */
    SOURCE_MLAT,           /* derived from mlat */
    SOURCE_MODE_S,         /* data from a Mode S message, no full CRC */
    SOURCE_MODE_S_CHECKED, /* data from a Mode S message with full CRC */
    SOURCE_TISB,           /* data from a TIS-B extended squitter message */
    SOURCE_ADSR,           /* data from a ADS-R extended squitter message */
    SOURCE_ADSB,           /* data from a ADS-B extended squitter message */
} datasource_t;

/* What sort of address is this and who sent it?
 * (Earlier values are higher priority)
 */
typedef enum {
    ADDR_ADSB_ICAO,       /* Mode S or ADS-B, ICAO address, transponder sourced */
    ADDR_ADSB_ICAO_NT,    /* ADS-B, ICAO address, non-transponder */
    ADDR_ADSR_ICAO,       /* ADS-R, ICAO address */
    ADDR_TISB_ICAO,       /* TIS-B, ICAO address */

    ADDR_ADSB_OTHER,      /* ADS-B, other address format */
    ADDR_ADSR_OTHER,      /* ADS-R, other address format */
    ADDR_TISB_TRACKFILE,  /* TIS-B, Mode A code + track file number */
    ADDR_TISB_OTHER,      /* TIS-B, other address format */

    ADDR_MODE_A,          /* Mode A */

    ADDR_UNKNOWN          /* unknown address format */
} addrtype_t;

typedef enum {
    UNIT_FEET,
    UNIT_METERS
} altitude_unit_t;

typedef enum {
    UNIT_NAUTICAL_MILES,
    UNIT_STATUTE_MILES,
    UNIT_KILOMETERS,
} interactive_distance_unit_t;

typedef enum {
    ALTITUDE_BARO,
    ALTITUDE_GEOM
} altitude_source_t;

typedef enum {
    AG_INVALID,
    AG_GROUND,
    AG_AIRBORNE,
    AG_UNCERTAIN
} airground_t;

typedef enum {
    SIL_INVALID, SIL_UNKNOWN, SIL_PER_SAMPLE, SIL_PER_HOUR
} sil_type_t;

typedef enum {
    CPR_SURFACE, CPR_AIRBORNE, CPR_COARSE
} cpr_type_t;

typedef enum {
    HEADING_INVALID,          // Not set
    HEADING_GROUND_TRACK,     // Direction of track over ground, degrees clockwise from true north
    HEADING_TRUE,             // Heading, degrees clockwise from true north
    HEADING_MAGNETIC,         // Heading, degrees clockwise from magnetic north
    HEADING_MAGNETIC_OR_TRUE, // HEADING_MAGNETIC or HEADING_TRUE depending on the HRD bit in opstatus
    HEADING_TRACK_OR_HEADING  // GROUND_TRACK / MAGNETIC / TRUE depending on the TAH bit in opstatus
} heading_type_t;

typedef enum {
    COMMB_UNKNOWN,
    COMMB_AMBIGUOUS,
    COMMB_NOT_DECODED,
    COMMB_EMPTY_RESPONSE,
    COMMB_DATALINK_CAPS,
    COMMB_GICB_CAPS,
    COMMB_AIRCRAFT_IDENT,
    COMMB_ACAS_RA,
    COMMB_VERTICAL_INTENT,
    COMMB_TRACK_TURN,
    COMMB_HEADING_SPEED,
    COMMB_MRAR,
    COMMB_AIRBORNE_POSITION
} commb_format_t;

typedef enum {
    NAV_MODE_AUTOPILOT = 1,
    NAV_MODE_VNAV = 2,
    NAV_MODE_ALT_HOLD = 4,
    NAV_MODE_APPROACH = 8,
    NAV_MODE_LNAV = 16,
    NAV_MODE_TCAS = 32
} nav_modes_t;

// Matches encoding of the ES type 28/1 emergency/priority status subfield
typedef enum {
    EMERGENCY_NONE = 0,
    EMERGENCY_GENERAL = 1,
    EMERGENCY_LIFEGUARD = 2,
    EMERGENCY_MINFUEL = 3,
    EMERGENCY_NORDO = 4,
    EMERGENCY_UNLAWFUL = 5,
    EMERGENCY_DOWNED = 6,
    EMERGENCY_RESERVED = 7
} emergency_t;

typedef enum {
    NAV_ALT_INVALID, NAV_ALT_UNKNOWN, NAV_ALT_AIRCRAFT, NAV_ALT_MCP, NAV_ALT_FMS
} nav_altitude_source_t;

// BDS4,4 MRAR - FOM/Source values
typedef enum {
   MRAR_SOURCE_INVALID = 0,
   MRAR_SOURCE_INS = 1,
   MRAR_SOURCE_GNSS = 2,
   MRAR_SOURCE_DMEDME = 3,
   MRAR_SOURCE_VORDME = 4,
   MRAR_SOURCE_RESERVED = 5
} mrar_source_t;

// BDS4,4 and BDS4,5 hazard reports
typedef enum {
   HAZARD_NIL = 0,
   HAZARD_LIGHT = 1,
   HAZARD_MODERATE = 2,
   HAZARD_SEVERE = 3
} hazard_t;

#define MODES_NON_ICAO_ADDRESS       (1<<24) // Set on addresses to indicate they are not ICAO addresses

#define MODES_INTERACTIVE_REFRESH_TIME 250      // Milliseconds
#define MODES_INTERACTIVE_DISPLAY_TTL 60000     // Delete from display after 60 seconds

#define MODES_NET_HEARTBEAT_INTERVAL 60000      // milliseconds

#define MODES_CLIENT_BUF_SIZE  1024
#define MODES_NET_SNDBUF_SIZE (1024*64)
#define MODES_NET_SNDBUF_MAX  (7)

#define HISTORY_SIZE 120
#define HISTORY_INTERVAL 30000

#define MODES_NOTUSED(V) ((void) V)

#define MAX_AMPLITUDE 65535.0
#define MAX_POWER (MAX_AMPLITUDE * MAX_AMPLITUDE)

#define FAUP_DEFAULT_RATE_MULTIPLIER    1.0                  // FA Upload rate multiplier


// Include subheaders after all the #defines are in place

#include "util.h"
#include "anet.h"
#include "net_io.h"
#include "crc.h"
#include "demod_2400.h"
#include "stats.h"
#include "cpr.h"
#include "icao_filter.h"
#include "convert.h"
#include "sdr.h"
#include "fifo.h"
#include "adaptive.h"

//======================== structure declarations =========================

typedef enum {
    SDR_NONE, SDR_IFILE, SDR_RTLSDR, SDR_BLADERF, SDR_HACKRF, SDR_LIMESDR, SDR_SOAPYSDR
} sdr_type_t;

// Program global state
struct _Modes {                             // Internal state
    pthread_t       reader_thread;

    pthread_mutex_t reader_cpu_mutex;                     // mutex protecting reader_cpu_accumulator
    struct timespec reader_cpu_accumulator;               // accumulated CPU time used by the reader thread
    struct timespec reader_cpu_start;                     // start time for the last reader thread CPU measurement

    unsigned        trailing_samples;                     // extra trailing samples in magnitude buffers
    double          sample_rate;                          // actual sample rate in use (in hz)

    uint16_t       *log10lut;        // Magnitude -> log10 lookup table
    atomic_int      exit;            // Exit from the main loop when true (2 = unclean exit)

    // Sample conversion
    int            dc_filter;        // should we apply a DC filter?

    // RTLSDR and some other SDRs
    char *        dev_name;
    float         gain;              // value in dB, or MODES_AUTO_GAIN, or MODES_MAX_GAIN
    int           freq;

    // Networking
    char           aneterr[ANET_ERR_LEN];
    struct net_service *services;    // Active services
    struct client *clients;          // Our clients

    struct net_service *beast_verbatim_service;        // Beast-format output service, verbatim mode
    struct net_service *beast_verbatim_local_service;  // Beast-format output service, verbatim+local mode
    struct net_service *beast_cooked_service;          // Beast-format output service, "cooked" mode

    struct net_writer raw_out;                   // AVR-format output
    struct net_writer beast_verbatim_out;        // Beast-format output, verbatim mode
    struct net_writer beast_verbatim_local_out;  // Beast-format output, verbatim+local mode
    struct net_writer beast_cooked_out;          // Beast-format output, "cooked" mode
    struct net_writer sbs_out;                   // SBS-format output
    struct net_writer stratux_out;               // Stratux-format output
    struct net_writer wiffle_out;                // Wiffle-format output
    struct net_writer fatsv_out;                 // FATSV-format output

#ifdef _WIN32
    WSADATA        wsaData;          // Windows socket initialisation
#endif

    // Configuration
    sdr_type_t sdr_type;             // where are we getting data from?
    int   nfix_crc;                  // Number of crc bit error(s) to correct
    int   check_crc;                 // Only display messages with good CRC
    int   fix_df;                    // Try to correct damage to the DF field, as well as the main message body
    int   enable_df24;               // Enable decoding of DF24..DF31 (Comm-D ELM)
    int   raw;                       // Raw output format
    int   mode_ac;                   // Enable decoding of SSR Modes A & C
    int   mode_ac_auto;              // allow toggling of A/C by Beast commands
    int   net;                       // Enable networking
    int   net_only;                  // Enable just networking
    uint64_t net_heartbeat_interval; // TCP heartbeat interval (milliseconds)
    int   net_output_flush_size;     // Minimum Size of output data
    uint64_t net_output_flush_interval; // Maximum interval (in milliseconds) between outputwrites
    char *net_output_raw_ports;      // List of raw output TCP ports
    char *net_input_raw_ports;       // List of raw input TCP ports
    char *net_output_sbs_ports;      // List of SBS output TCP ports
    char *net_output_stratux_ports;  // List of Stratux output TCP ports
    char *net_input_beast_ports;     // List of Beast input TCP ports
    char *net_output_beast_ports;    // List of Beast output TCP ports
    char* net_output_wiffle_ports;   // List of Wiffle output TCP ports
    char *net_bind_address;          // Bind address
    int   net_sndbuf_size;           // TCP output buffer size (64Kb * 2^n)
    int   net_verbatim;              // if true, Beast output connections default to verbatim mode
    int   forward_mlat;              // allow forwarding of mlat messages to output ports
    int   quiet;                     // Suppress stdout
    uint32_t show_only;              // Only show messages from this ICAO
    int   interactive;               // Interactive mode
    uint64_t interactive_display_ttl;// Interactive mode: TTL display
    int   wiffle_stdout;         // Output modes as wiffle CSV output
    int interactive_display_size;    // Size of TTL display
    int   interactive_show_distance; // Show aircraft distance and bearing instead of lat/lon
    interactive_distance_unit_t interactive_distance_units; // Units for interactive distance display
    char *interactive_callsign_filter; // Filter for interactive display callsigns
    uint64_t stats;                  // Interval (millis) between stats dumps,
    int   stats_range_histo;         // Collect/show a range histogram?
    int   onlyaddr;                  // Print only ICAO addresses
    int   metric;                    // Use metric units
    int   use_gnss;                  // Use GNSS altitudes with H suffix ("HAE", though it isn't always) when available
    int   mlat;                      // Use Beast ascii format for raw data output, i.e. @...; iso *...;
    char *json_dir;                  // Path to json base directory, or NULL not to write json.
    uint64_t json_interval;          // Interval between rewriting the json aircraft file, in milliseconds; also the advertised map refresh interval
    uint64_t json_stats_interval;    // Interval between rewriting the json stats file, in milliseconds
    int   json_location_accuracy;    // Accuracy of location metadata: 0=none, 1=approx, 2=exact
    double faup_rate_multiplier;     // Multiplier to adjust rate of faup1090 messages emitted
    bool faup_upload_unknown_commb;  // faup1090: should we upload Comm-B messages that weren't in a recognized format?

    int   json_aircraft_history_next;
    struct {
        char *content;
        int clen;
    } json_aircraft_history[HISTORY_SIZE];

    // User details
    double fUserLat;                // Users receiver/antenna lat/lon needed for initial surface location
    double fUserLon;                // Users receiver/antenna lat/lon needed for initial surface location
    int    bUserFlags;              // Flags relating to the user details
    double maxRange;                // Absolute maximum decoding range, in *metres*

    // State tracking
    struct aircraft *aircrafts;

    // Statistics
    struct stats stats_current;     // Currently accumulating stats, this is where all stats are initially collected
    struct stats stats_alltime;     // Accumulated stats since the start of the process
    struct stats stats_periodic;    // Accumulated stats since the last periodic stats display (--stats-every)
    struct stats stats_latest;      // Accumulated stats since the end of the last 1-minute period
    struct stats stats_1min[15];    // Accumulated stats for a full 1-minute window; this is a ring buffer maintaining a history of 15 minutes
    int stats_newest_1min;          // Index into stats_1min of the most recent 1-minute window
    struct stats stats_5min;        // Accumulated stats from the last 5 complete 1-minute windows
    struct stats stats_15min;       // Accumulated stats from the last 15 complete 1-minute windows

    // Adaptive gain config
    float adaptive_min_gain_db;
    float adaptive_max_gain_db;

    float adaptive_duty_cycle;

    bool adaptive_burst_control;
    float adaptive_burst_alpha;
    unsigned adaptive_burst_change_delay;
    float adaptive_burst_loud_rate;
    unsigned adaptive_burst_loud_runlength;
    float adaptive_burst_quiet_rate;
    unsigned adaptive_burst_quiet_runlength;

    bool adaptive_range_control;
    float adaptive_range_alpha;
    unsigned adaptive_range_percentile;
    float adaptive_range_target;
    unsigned adaptive_range_change_delay;
    unsigned adaptive_range_scan_delay;
    unsigned adaptive_range_rescan_delay;
};

extern struct _Modes Modes;

// The struct we use to store information about a decoded message.
struct modesMessage {
    // Generic fields
    unsigned char msg[MODES_LONG_MSG_BYTES];      // Binary message.
    unsigned char verbatim[MODES_LONG_MSG_BYTES]; // Binary message, as originally received before correction
    int           msgbits;                        // Number of bits in message
    int           msgtype;                        // Downlink format #
    uint32_t      crc;                            // Message CRC
    int           correctedbits;                  // No. of bits corrected
    uint32_t      addr;                           // Address Announced
    addrtype_t    addrtype;                       // address format / source
    uint64_t      timestampMsg;                   // Timestamp of the message (12MHz clock)
    uint64_t      sysTimestampMsg;                // Timestamp of the message (system time)
    int           remote;                         // If set this message is from a remote station
    double        signalLevel;                    // RSSI, in the range [0..1], as a fraction of full-scale power
    int           score;                          // Scoring from scoreModesMessage, if used
    int           reliable;                       // is this a "reliable" message (uncorrected DF11/DF17/DF18)?

    datasource_t  source;                         // Characterizes the overall message source

    // Raw data, just extracted directly from the message
    // The names reflect the field names in Annex 4
    unsigned IID; // extracted from CRC of DF11s
    unsigned AA;
    unsigned AC;
    unsigned CA;
    unsigned CC;
    unsigned CF;
    unsigned DR;
    unsigned FS;
    unsigned ID;
    unsigned KE;
    unsigned ND;
    unsigned RI;
    unsigned SL;
    unsigned UM;
    unsigned VS;
    unsigned char MB[7];
    unsigned char MD[10];
    unsigned char ME[7];
    unsigned char MV[7];

    // Decoded data
    unsigned altitude_baro_valid : 1;
    unsigned altitude_geom_valid : 1;
    unsigned track_valid : 1;
    unsigned track_rate_valid : 1;
    unsigned heading_valid : 1;
    unsigned roll_valid : 1;
    unsigned gs_valid : 1;
    unsigned ias_valid : 1;
    unsigned tas_valid : 1;
    unsigned mach_valid : 1;
    unsigned baro_rate_valid : 1;
    unsigned geom_rate_valid : 1;
    unsigned squawk_valid : 1;
    unsigned callsign_valid : 1;
    unsigned cpr_valid : 1;
    unsigned cpr_odd : 1;
    unsigned cpr_decoded : 1;
    unsigned cpr_relative : 1;
    unsigned category_valid : 1;
    unsigned geom_delta_valid : 1;
    unsigned from_mlat : 1;
    unsigned from_tisb : 1;
    unsigned spi_valid : 1;
    unsigned spi : 1;
    unsigned alert_valid : 1;
    unsigned alert : 1;
    unsigned emergency_valid : 1;

    unsigned metype; // DF17/18 ME type
    unsigned mesub;  // DF17/18 ME subtype

    commb_format_t commb_format; // Inferred format of a comm-b message

    // valid if altitude_baro_valid:
    int               altitude_baro;       // Altitude in either feet or meters
    altitude_unit_t   altitude_baro_unit;  // the unit used for altitude

    // valid if altitude_geom_valid:
    int               altitude_geom;       // Altitude in either feet or meters
    altitude_unit_t   altitude_geom_unit;  // the unit used for altitude

    // following fields are valid if the corresponding _valid field is set:
    int      geom_delta;        // Difference between geometric and baro alt
    float    heading;           // ground track or heading, degrees (0-359). Reported directly or computed from from EW and NS velocity
    heading_type_t heading_type;// how to interpret 'track_or_heading'
    float    track_rate;        // Rate of change of track, degrees/second
    float    roll;              // Roll, degrees, negative is left roll
    struct {
        // Groundspeed, kts, reported directly or computed from from EW and NS velocity
        // For surface movement, this has different interpretations for v0 and v2; both
        // fields are populated. The tracking layer will update "gs.selected".
        float v0;
        float v2;
        float selected;
    } gs;
    unsigned ias;               // Indicated airspeed, kts
    unsigned tas;               // True airspeed, kts
    double   mach;              // Mach number
    int      baro_rate;         // Rate of change of barometric altitude, feet/minute
    int      geom_rate;         // Rate of change of geometric (GNSS / INS) altitude, feet/minute
    unsigned squawk;            // 13 bits identity (Squawk), encoded as 4 hex digits
    char     callsign[9];       // 8 chars flight number, NUL-terminated
    unsigned category;          // A0 - D7 encoded as a single hex byte
    emergency_t emergency;      // emergency/priority status

    // valid if cpr_valid
    cpr_type_t cpr_type;       // The encoding type used (surface, airborne, coarse TIS-B)
    unsigned   cpr_lat;        // Non decoded latitude.
    unsigned   cpr_lon;        // Non decoded longitude.
    unsigned   cpr_nucp;       // NUCp/NIC value implied by message type

    airground_t airground;     // air/ground state

    // valid if cpr_decoded:
    double decoded_lat;
    double decoded_lon;
    unsigned decoded_nic;
    unsigned decoded_rc;

    // various integrity/accuracy things
    struct {
        unsigned nic_a_valid : 1;
        unsigned nic_b_valid : 1;
        unsigned nic_c_valid : 1;
        unsigned nic_baro_valid : 1;
        unsigned nac_p_valid : 1;
        unsigned nac_v_valid : 1;
        unsigned gva_valid : 1;
        unsigned sda_valid : 1;

        unsigned nic_a : 1;        // if nic_a_valid
        unsigned nic_b : 1;        // if nic_b_valid
        unsigned nic_c : 1;        // if nic_c_valid
        unsigned nic_baro : 1;     // if nic_baro_valid

        unsigned nac_p;        // if nac_p_valid
        unsigned nac_v;        // if nac_v_valid

        unsigned sil;          // if sil_type != SIL_INVALID
        sil_type_t sil_type;

        unsigned gva;          // if gva_valid
        unsigned sda;          // if sda_valid
    } accuracy;

    // Operational Status
    struct {
        unsigned valid : 1;
        unsigned version;

        unsigned om_acas_ra : 1;
        unsigned om_ident : 1;
        unsigned om_atc : 1;
        unsigned om_saf : 1;

        unsigned cc_acas : 1;
        unsigned cc_cdti : 1;
        unsigned cc_1090_in : 1;
        unsigned cc_arv : 1;
        unsigned cc_ts : 1;
        unsigned cc_tc;
        unsigned cc_uat_in : 1;
        unsigned cc_poa : 1;
        unsigned cc_b2_low : 1;
        unsigned cc_lw_valid : 1;

        heading_type_t tah;
        heading_type_t hrd;

        unsigned cc_lw;
        unsigned cc_antenna_offset;
    } opstatus;

    // combined:
    //   Target State & Status (ADS-B V2 only)
    //   Comm-B BDS4,0 Vertical Intent
    struct {
        unsigned heading_valid : 1;
        unsigned fms_altitude_valid : 1;
        unsigned mcp_altitude_valid : 1;
        unsigned qnh_valid : 1;
        unsigned modes_valid : 1;

        float    heading;       // heading, degrees (0-359) (could be magnetic or true heading; magnetic recommended)
        heading_type_t heading_type;
        int      fms_altitude;  // FMS selected altitude
        int      mcp_altitude;  // MCP/FCU selected altitude
        float    qnh;           // altimeter setting (QFE or QNH/QNE), millibars

        nav_altitude_source_t altitude_source;

        nav_modes_t modes;
    } nav;

    // BDS 4,4 MRAR
    unsigned mrar_source_valid : 1;
    unsigned wind_valid : 1;
    unsigned temperature_valid : 1;
    unsigned pressure_valid : 1;
    unsigned turbulence_valid : 1;
    unsigned humidity_valid : 1;

    mrar_source_t mrar_source;
    float wind_speed;    // kts
    float wind_dir;      // degrees
    float temperature;   // degrees C
    float pressure;      // hPa
    hazard_t turbulence; // NIL/LIGHT/MODERATE/SEVERE
    float humidity;      // 0-100 %
};

// This one needs modesMessage:
#include "track.h"
#include "mode_s.h"
#include "comm_b.h"

// ======================== function declarations =========================

#ifdef __cplusplus
extern "C" {
#endif

//
// Functions exported from mode_ac.c
//
int  detectModeA       (uint16_t *m, struct modesMessage *mm);
void decodeModeAMessage(struct modesMessage *mm, int ModeA);
void modeACInit();
int modeAToModeC (unsigned int modeA);
unsigned modeCToModeA (int modeC);

//
// Functions exported from interactive.c
//
void  interactiveInit(void);
void  interactiveShowData(void);
void  interactiveCleanup(void);
void  interactiveNoConnection(void);

// Provided by dump1090.c / view1090.c / faup1090.c
void receiverPositionChanged(float lat, float lon, float alt);

#ifdef __cplusplus
}
#endif

#endif // __DUMP1090_H
