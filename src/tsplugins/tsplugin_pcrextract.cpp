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
//  Extract PCR's from TS packets.
//
//----------------------------------------------------------------------------

#include "tsPlugin.h"
TSDUCK_SOURCE;

#define DEFAULT_SEPARATOR ";"


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class PCRExtractPlugin: public ProcessorPlugin
    {
    public:
        // Implementation of plugin API
        PCRExtractPlugin (TSP*);
        virtual bool start();
        virtual bool stop();
        virtual BitRate getBitrate() {return 0;}
        virtual Status processPacket (TSPacket&, bool&, bool&);

    private:
        // Description of one PID
        struct PIDContext;
        typedef std::map<PID,PIDContext> PIDContextMap;

        // PCRExtractPlugin private members
        std::string   _separator;      // Field separator
        bool          _noheader;       // Suppress header
        bool          _good_pts_only;  // Keep "good" PTS only
        bool          _get_pcr;        // Get PCR
        bool          _get_opcr;       // Get OPCR
        bool          _get_pts;        // Get PTS
        bool          _get_dts;        // Get DTS
        std::string   _output_name;    // Output file name (NULL means stderr)
        std::ofstream _output_stream;  // Output stream file
        std::ostream* _output;         // Reference to actual output stream file
        PacketCounter _packet_count;   // Global packets count
        PIDContextMap _stats;          // Per-PID statistics

        // Description of one PID
        struct PIDContext
        {
            PacketCounter packet_count;
            PacketCounter pcr_count;
            PacketCounter opcr_count;
            PacketCounter pts_count;
            PacketCounter dts_count;
            uint64_t      first_pcr;
            uint64_t      first_opcr;
            uint64_t      first_pts;
            uint64_t      last_good_pts;
            uint64_t      first_dts;

            // Constructor
            PIDContext() :
                packet_count(0),
                pcr_count(0),
                opcr_count(0),
                pts_count(0),
                dts_count(0),
                first_pcr(0),
                first_opcr(0),
                first_pts(0),
                last_good_pts(0),
                first_dts(0)
            {
            }
        };

        // Inaccessible operations
        PCRExtractPlugin() = delete;
        PCRExtractPlugin(const PCRExtractPlugin&) = delete;
        PCRExtractPlugin& operator=(const PCRExtractPlugin&) = delete;
    };
}

TSPLUGIN_DECLARE_VERSION
TSPLUGIN_DECLARE_PROCESSOR(ts::PCRExtractPlugin)


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::PCRExtractPlugin::PCRExtractPlugin (TSP* tsp_) :
    ProcessorPlugin(tsp_, "Extracts PCR, OPCR, PTS, DTS from TS packet for analysis.", "[options]"),
    _separator(),
    _noheader(false),
    _good_pts_only(false),
    _get_pcr(false),
    _get_opcr(false),
    _get_pts(false),
    _get_dts(false),
    _output_name(),
    _output_stream(),
    _output(0),
    _packet_count(0),
    _stats()
{
    option ("dts",           'd');
    option ("good-pts-only", 'g');
    option ("noheader",      'n');
    option ("opcr",           0);
    option ("output-file",   'o', STRING);
    option ("pcr",            0);
    option ("pts",           'p');
    option ("separator",     's', STRING);

    setHelp ("Options:\n"
             "\n"
             "  -d\n"
             "  --dts\n"
             "      Report Decoding Time Stamps (DTS). By default, if none of --pcr, --opcr,\n"
             "      --pts, --dts is specified, report them all.\n"
             "\n"
             "  -g\n"
             "  --good-pts-only\n"
             "      Keep only \"good\" PTS, ie. PTS which have a higher value than the\n"
             "      previous good PTS. This eliminates PTS from out-of-sequence B-frames.\n"
             "\n"
             "  --help\n"
             "      Display this help text.\n"
             "\n"
             "  -n\n"
             "  --noheader\n"
             "      Do not output initial header line.\n"
             "\n"
             "  --opcr\n"
             "      Report Original Program Clock References (OPCR). By default, if none of\n"
             "      --pcr, --opcr, --pts, --dts is specified, report them all.\n"
             "\n"
             "  -o filename\n"
             "  --output-file filename\n"
             "      Output file name (standard error by default).\n"
             "\n"
             "  --pcr\n"
             "      Report Program Clock References (PCR). By default, if none of --pcr,\n"
             "      --opcr, --pts, --dts is specified, report them all.\n"
             "\n"
             "  -p\n"
             "  --pts\n"
             "      Report Presentation Time Stamps (PTS). By default, if none of --pcr,\n"
             "      --opcr, --pts, --dts is specified, report them all.\n"
             "\n"
             "  -s string\n"
             "  --separator string\n"
             "      Field separator string in output (default: '" DEFAULT_SEPARATOR "').\n"
             "\n"
             "  --version\n"
             "      Display the version number.\n");
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::PCRExtractPlugin::start()
{
    _separator = value ("separator", DEFAULT_SEPARATOR);
    _noheader = present ("noheader");
    _output_name = value ("output-file");
    _good_pts_only = present ("good-pts-only");
    _get_pts = present ("dts");
    _get_dts = present ("pts");
    _get_pcr = present ("pcr");
    _get_opcr = present ("opcr");
    if (!_get_pts && !_get_dts && !_get_pcr && !_get_opcr) {
        // Report them all by default
        _get_pts = _get_dts = _get_pcr = _get_opcr = true;
    }

    // Create the output file if there is one
    if (_output_name.empty()) {
        _output = &std::cerr;
    }
    else {
        _output = &_output_stream;
        _output_stream.open (_output_name.c_str());
        if (!_output_stream) {
            tsp->error ("cannot create file %s", _output_name.c_str());
            return false;
        }
    }

    // Reset state
    _packet_count = 0;
    _stats.clear();

    // Output header
    if (!_noheader) {
        *_output << "PID" << _separator
                 << "Packet index in TS" << _separator
                 << "Packet index in PID" << _separator
                 << "Type" << _separator
                 << "Count in PID" << _separator
                 << "Value" << _separator
                 << "Value offset in PID" << _separator
                 << "Offset from PCR" << std::endl;
    }
    return true;
}


//----------------------------------------------------------------------------
// Stop method
//----------------------------------------------------------------------------

bool ts::PCRExtractPlugin::stop()
{
    if (!_output_name.empty()) {
        _output_stream.close();
    }
    return true;
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::PCRExtractPlugin::processPacket (TSPacket& pkt, bool& flush, bool& bitrate_changed)
{
    const PID pid = pkt.getPID();
    PIDContext& pc (_stats[pid]);
    const bool has_pcr = pkt.hasPCR();
    const bool has_opcr = pkt.hasOPCR();
    const bool has_pts = pkt.hasPTS();
    const bool has_dts = pkt.hasDTS();
    const uint64_t pcr = pkt.getPCR();

    if (has_pcr) {
        if (pc.pcr_count++ == 0) {
            pc.first_pcr = pcr;
        }
        if (_get_pcr) {
            *_output << pid << _separator
                     << _packet_count << _separator
                     << pc.packet_count << _separator
                     << "PCR" << _separator
                     << pc.pcr_count << _separator
                     << pcr << _separator
                     << (pcr - pc.first_pcr) << _separator
                     << std::endl;
        }
    }

    if (has_opcr) {
        const uint64_t opcr = pkt.getOPCR();
        if (pc.opcr_count++ == 0) {
            pc.first_opcr = opcr;
        }
        if (_get_opcr) {
            *_output << pid << _separator
                     << _packet_count << _separator
                     << pc.packet_count << _separator
                     << "OPCR" << _separator
                     << pc.opcr_count << _separator
                     << opcr << _separator
                     << (opcr - pc.first_opcr) << _separator;
            if (has_pcr) {
                *_output << (int64_t (opcr) - int64_t (pcr));
            }
            *_output << std::endl;
        }
    }

    if (has_pts) {
        const uint64_t pts = pkt.getPTS();
        if (pc.pts_count++ == 0) {
            pc.first_pts = pc.last_good_pts = pts;
        }
        // Check if this is a "good" PTS, ie. greater than the last good PTS
        // (or wrapping around the max PTS value 2**33)
        const bool good_pts = SequencedPTS (pc.last_good_pts, pts);
        if (good_pts) {
            pc.last_good_pts = pts;
        }
        if (_get_pts && (good_pts || !_good_pts_only)) {
            *_output << pid << _separator
                     << _packet_count << _separator
                     << pc.packet_count << _separator
                     << "PTS" << _separator
                     << pc.pts_count << _separator
                     << pts << _separator
                     << (pts - pc.first_pts) << _separator;
            if (has_pcr) {
                *_output << (int64_t (pts) - int64_t (pcr / SYSTEM_CLOCK_SUBFACTOR));
            }
            *_output << std::endl;
        }
    }

    if (has_dts) {
        const uint64_t dts = pkt.getDTS();
        if (pc.dts_count++ == 0) {
            pc.first_dts = dts;
        }
        if (_get_dts) {
            *_output << pid << _separator
                     << _packet_count << _separator
                     << pc.packet_count << _separator
                     << "DTS" << _separator
                     << pc.dts_count << _separator
                     << dts << _separator
                     << (dts - pc.first_dts) << _separator;
            if (has_pcr) {
                *_output << (int64_t (dts) - int64_t (pcr / SYSTEM_CLOCK_SUBFACTOR));
            }
            *_output << std::endl;
        }
    }

    _packet_count++;
    pc.packet_count++;
    return TSP_OK;
}
