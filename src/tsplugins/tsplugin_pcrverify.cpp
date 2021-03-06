//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2017, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------
//
//  Transport stream processor shared library:
//  Verify PCR values
//
//----------------------------------------------------------------------------

#include "tsPlugin.h"
#include "tsDecimal.h"
#include "tsFormat.h"
#include "tsTime.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class PCRVerifyPlugin: public ProcessorPlugin
    {
    public:
        // Implementation of plugin API
        PCRVerifyPlugin (TSP*);
        virtual bool start();
        virtual bool stop();
        virtual BitRate getBitrate() {return 0;}
        virtual Status processPacket (TSPacket&, bool&, bool&);

    private:
        // Description of one PID
        struct PIDContext
        {
            uint64_t      last_pcr_value;   // Last PCR value in this PID
            PacketCounter last_pcr_packet;  // Packet index containing last PCR

            // Constructor
            PIDContext() :
                last_pcr_value(0),
                last_pcr_packet(0)
            {
            }
        };

        // PCRVerifyPlugin private members
        bool          _absolute;         // Use PCR absolute value, not micro-second
        BitRate       _bitrate;          // Expected bitrate (0 if unknown)
        int64_t       _jitter_max;       // Max jitter in PCR units
        bool          _time_stamp;       // Display time stamps
        PIDSet        _pid_list;         // Array of pid values to filter
        PacketCounter _packet_count;     // Global packets count
        PacketCounter _nb_pcr_ok;        // Number of PCR without jitter
        PacketCounter _nb_pcr_nok;       // Number of PCR with jitter
        PacketCounter _nb_pcr_unchecked; // Number of unchecked PCR (no previous ref)
        PIDContext    _stats[PID_MAX];   // Per-PID statistics

        // PCR units per micro-second
        static const int64_t PCR_PER_MICRO_SEC = int64_t (SYSTEM_CLOCK_FREQ) / MicroSecPerSec;
        static const int64_t DEFAULT_JITTER_MAX_US = 1000; // 1000 us = 1 ms
        static const int64_t DEFAULT_JITTER_MAX = DEFAULT_JITTER_MAX_US * PCR_PER_MICRO_SEC;

        // Inaccessible operations
        PCRVerifyPlugin() = delete;
        PCRVerifyPlugin(const PCRVerifyPlugin&) = delete;
        PCRVerifyPlugin& operator=(const PCRVerifyPlugin&) = delete;
    };
}

TSPLUGIN_DECLARE_VERSION
TSPLUGIN_DECLARE_PROCESSOR(ts::PCRVerifyPlugin)

#if defined(TS_NEED_STATIC_CONST_DEFINITIONS)
const int64_t ts::PCRVerifyPlugin::PCR_PER_MICRO_SEC;
const int64_t ts::PCRVerifyPlugin::DEFAULT_JITTER_MAX_US;
const int64_t ts::PCRVerifyPlugin::DEFAULT_JITTER_MAX;
#endif


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::PCRVerifyPlugin::PCRVerifyPlugin (TSP* tsp_) :
    ProcessorPlugin(tsp_, "Verify PCR's from TS packets.", "[options]"),
    _absolute(false),
    _bitrate(0),
    _jitter_max(0),
    _time_stamp(false),
    _pid_list(),
    _packet_count(0),
    _nb_pcr_ok(0),
    _nb_pcr_nok(0),
    _nb_pcr_unchecked(0),
    _stats()
{
    option ("absolute",   'a');
    option ("bitrate",    'b', POSITIVE);
    option ("jitter-max", 'j', UNSIGNED);
    option ("pid",        'p', PIDVAL, 0, UNLIMITED_COUNT);
    option ("time-stamp", 't');

    setHelp ("Options:\n"
             "\n"
             "  -a\n"
             "  --absolute\n"
             "      Use absolute values in PCR unit. By default, use micro-second equivalent\n"
             "      values (one micro-second = 27 PCR units).\n"
             "\n"
             "  -b value\n"
             "  --bitrate value\n"
             "      Verify the PCR's according to this transport bitrate. By default,\n"
             "      use the input bitrate as reported by the input device.\n"
             "\n"
             "  --help\n"
             "      Display this help text.\n"
             "\n"
             "  -j value\n"
             "  --jitter-max value\n"
             "      Maximum allowed jitter. PCR's with a higher jitter are reported, others\n"
             "      are ignored. If --absolute, the specified value is in PCR units,\n"
             "      otherwise it is in micro-seconds. The default is " + Decimal (DEFAULT_JITTER_MAX) + " PCR units\n"
             "      or " + Decimal (DEFAULT_JITTER_MAX_US) + " micro-seconds.\n"
             "\n"
             "  -p value\n"
             "  --pid value\n"
             "      PID filter: select packets with this PID value.\n"
             "      Several -p or --pid options may be specified.\n"
             "      Without -p or --pid option, PCR's from all PID's are used.\n"
             "\n"
             "  -t\n"
             "  --time-stamp\n"
             "      Display time of each event.\n"
             "\n"
             "  --version\n"
             "      Display the version number.\n");
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::PCRVerifyPlugin::start()
{
    _absolute = present ("absolute");
    _jitter_max = intValue<int64_t> ("jitter-max", _absolute ? DEFAULT_JITTER_MAX : DEFAULT_JITTER_MAX_US);
    _bitrate = intValue<BitRate> ("bitrate", 0);
    _time_stamp = present ("time-stamp");
    getPIDSet (_pid_list, "pid", true);

    if (!_absolute) {
        // Convert _jitter_max from micro-second to PCR units
        _jitter_max *= PCR_PER_MICRO_SEC;
    }

    // Reset state
    _packet_count = 0;
    _nb_pcr_ok = 0;
    _nb_pcr_nok = 0;
    _nb_pcr_unchecked = 0;

    for (size_t i = 0; i < PID_MAX; ++i) {
        _stats[i].last_pcr_value = 0;
        _stats[i].last_pcr_packet = 0;
    }

    return true;
}


//----------------------------------------------------------------------------
// Stop method
//----------------------------------------------------------------------------

bool ts::PCRVerifyPlugin::stop()
{
    // Display PCR summary
    tsp->log (Severity::Info,
              Decimal (_nb_pcr_ok) + " PCR OK, " +
              Decimal (_nb_pcr_nok) + " with jitter > " +
              Decimal (_jitter_max) +
              " (" + Decimal (_jitter_max / PCR_PER_MICRO_SEC) + " micro-seconds), " +
              Decimal (_nb_pcr_unchecked) + " unchecked");

    return true;
}




//----------------------------------------------------------------------------
// Compute the PCR jitter
//----------------------------------------------------------------------------

namespace {

    using namespace ts;

    int64_t jitter (int64_t pcr1,     // first PCR value
                  int64_t pkt1,     // packet index of PCR1 in TS
                  int64_t pcr2,     // seconde PCR value
                  int64_t pkt2,     // packet index of PCR2 in TS
                  int64_t bitrate)  // TS bitrate in bits/sec
    {
        // Cannot compute jitter if bitrate is unknown
        if (bitrate == 0) {
            return 0;
        }

        // Formulas:
        //
        //   epcr2 = expected pcr2 based on bitrate
        //
        //   SysClock = 27 MHz = 27,000,000 = SYSTEM_CLOCK_FREQ
        //   epcr2 = pcr1 + (seconds * SysClock)
        //   seconds = bits / bitrate
        //   bits = (pkt2 - pkt1) * PKT_SIZE * 8
        //   pcr-jitter = pcr2 - epcr2
        //       = pcr2 - pcr1 - (seconds * SysClock)
        //       = pcr2 - pcr1 - (bits * SysClock / bitrate)
        //       = pcr2 - pcr1 - ((pkt2 - pkt1) * PKT_SIZE * 8 * SysClock / bitrate)
        //       = (bitate * (pcr2 - pcr1) - (pkt2 - pkt1) * PKT_SIZE * 8 * SysClock) / bitrate

        return (bitrate * (pcr2 - pcr1) - (pkt2 - pkt1) * PKT_SIZE * 8 * SYSTEM_CLOCK_FREQ) / bitrate;
    }
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::PCRVerifyPlugin::processPacket (TSPacket& pkt, bool& flush, bool& bitrate_changed)
{
    const size_t pid = pkt.getPID();

    // Check if this PID shall be filtered and packet has a PCR
    if (_pid_list[pid] && pkt.hasPCR()) {

        const uint64_t pcr = pkt.getPCR();
        PIDContext& pc (_stats[pid]);

        // Compare PCR with previous one (if there is one)
        if (pc.last_pcr_value == 0) {
            _nb_pcr_unchecked++;
        }
        else {
            // Current bitrate:
            int64_t bitrate = int64_t (_bitrate != 0 ? _bitrate : tsp->bitrate());
            // PCR jitter:
            int64_t jit = jitter (int64_t (pc.last_pcr_value),
                                int64_t (pc.last_pcr_packet),
                                int64_t (pcr),
                                int64_t (_packet_count),
                                bitrate);
            // Absolute value of PCR jitter:
            int64_t ajit = jit >= 0 ? jit : -jit;
            if (ajit <= _jitter_max) {
                _nb_pcr_ok++;
            }
            else {
                _nb_pcr_nok++;
                // Jitter in bits at current bitrate
                int64_t bit_jit = (ajit * bitrate) / SYSTEM_CLOCK_FREQ;
                tsp->info ((_time_stamp ? (Time::CurrentLocalTime().format (Time::DATE | Time::TIME) + ", ") : "") +
                           Format ("PID %d (0x%04X), PCR jitter: ", int (pid), int (pid)) +
                           Decimal (jit) + " = " +
                           Decimal (ajit / PCR_PER_MICRO_SEC) + " micro-seconds = " +
                           Decimal (bit_jit / (PKT_SIZE * 8)) + " packets + " +
                           Decimal ((bit_jit / 8) % PKT_SIZE) + " bytes + " +
                           Decimal (bit_jit % 8) + " bits");
            }
        }

        // Remember PCR position
        pc.last_pcr_value = pcr;
        pc.last_pcr_packet = _packet_count;
    }

    // Count packets on TS
    _packet_count++;

    return TSP_OK;
}
