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
//  Extract clear (non scrambled) sequence of a transport stream
//
//----------------------------------------------------------------------------

#include "tsPlugin.h"
#include "tsDecimal.h"
#include "tsService.h"
#include "tsSectionDemux.h"
#include "tsPAT.h"
#include "tsPMT.h"
#include "tsSDT.h"
#include "tsTOT.h"
#include "tsFormat.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class ClearPlugin: public ProcessorPlugin, private TableHandlerInterface
    {
    public:
        // Implementation of plugin API
        ClearPlugin (TSP*);
        virtual bool start();
        virtual bool stop() {return true;}
        virtual BitRate getBitrate() {return 0;}
        virtual Status processPacket (TSPacket&, bool&, bool&);

    private:
        bool          _abort;           // Error (service not found, etc)
        Service       _service;         // Service name & id
        bool          _pass_packets;    // Pass packets trigger
        Status        _drop_status;     // Status for dropped packets
        bool          _video_only;      // Check video PIDs only
        bool          _audio_only;      // Check audio PIDs only
        TOT           _last_tot;        // Last received TOT
        PacketCounter _drop_after;      // Number of packets after last clear
        PacketCounter _current_pkt;     // Current TS packet number
        PacketCounter _last_clear_pkt;  // Last clear packet number
        PIDSet        _clear_pids;      // List of PIDs to check for clear packets
        SectionDemux  _demux;           // Section demux

        // Invoked by the demux when a complete table is available.
        virtual void handleTable (SectionDemux&, const BinaryTable&);

        // Process specific tables
        void processPAT (PAT&);
        void processPMT (PMT&);
        void processSDT (SDT&);

        // Inaccessible operations
        ClearPlugin() = delete;
        ClearPlugin(const ClearPlugin&) = delete;
        ClearPlugin& operator=(const ClearPlugin&) = delete;
    };
}

TSPLUGIN_DECLARE_VERSION
TSPLUGIN_DECLARE_PROCESSOR(ts::ClearPlugin)


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::ClearPlugin::ClearPlugin (TSP* tsp_) :
    ProcessorPlugin (tsp_, "Extract clear (non scrambled) sequences of a transport stream.", "[options]"),
    _abort(false),
    _service(),
    _pass_packets(false),
    _drop_status(TSP_OK),
    _video_only(false),
    _audio_only(false),
    _last_tot(Time::Epoch),
    _drop_after(0),
    _current_pkt(0),
    _last_clear_pkt(0),
    _clear_pids(),
    _demux(this)
{
    option ("audio",              'a');
    option ("drop-after-packets", 'd', POSITIVE);
    option ("service",            's', STRING);
    option ("stuffing",            0);
    option ("video",              'v');

    setHelp ("The extraction of clear sequences is based on one \"reference\" service.\n"
             "(see option -s). When a clear packet is found on any audio or video stream of\n"
             "the reference service, all packets in the TS are transmitted. When no clear\n"
             "packet has been found in the last second, all packets in the TS are dropped.\n"
             "\n"
             "Options:\n"
             "\n"
             "  -a\n"
             "  --audio\n"
             "      Check only audio PIDs for clear packets. By default, audio and video\n"
             "      PIDs are checked.\n"
             "\n"
             "  -d value\n"
             "  --drop-after-packets value\n"
             "      Specifies the number of packets after the last clear packet to wait\n"
             "      before stopping the packet transmission. By default, stop 1 second\n"
             "      after the last clear packet (based on current bitrate).\n"
             "\n"
             "  --help\n"
             "      Display this help text.\n"
             "\n"
             "  -s name-or-id\n"
             "  --service name-or-id\n"
             "      Specify the reference service. If the argument is an integer value\n"
             "      (either decimal or hexadecimal), it is interpreted as a service id.\n"
             "      Otherwise, it is interpreted as a service name, as specified in the\n"
             "      SDT. The name is not case sensitive and blanks are ignored. If this\n"
             "      option is not specified, the first service in the PAT is used.\n"
             "\n"
             "  --stuffing\n"
             "      Replace excluded packets with stuffing (null packets) instead\n"
             "      of removing them. Useful to preserve bitrate.\n"
             "\n"
             "  --version\n"
             "      Display the version number.\n"
             "\n"
             "  -v\n"
             "  --video\n"
             "      Check only video PIDs for clear packets. By default, audio and video\n"
             "      PIDs are checked.\n");
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::ClearPlugin::start()
{
    // Get option values
    _service.set (value ("service"));
    _video_only = present ("video");
    _audio_only = present ("audio");
    _drop_status = present ("stuffing") ? TSP_NULL : TSP_DROP;
    _drop_after = intValue<PacketCounter> ("drop-after-packets", 0);

    // Initialize the demux. Filter the TOT to get timestamps.
    // If the service is known by name, filter the SDT, otherwise filter the PAT.
    _demux.reset();
    _demux.addPID(PID_TOT);
    _demux.addPID(PID(_service.hasName() ? PID_SDT : PID_PAT));

    // Reset other states
    _abort = false;
    _pass_packets = false; // initially drop packets
    _last_tot.invalidate();
    _current_pkt = 0;
    _last_clear_pkt = 0;
    _clear_pids.reset();

    return true;
}


//----------------------------------------------------------------------------
// Invoked by the demux when a complete table is available.
//----------------------------------------------------------------------------

void ts::ClearPlugin::handleTable (SectionDemux& demux, const BinaryTable& table)
{
    switch (table.tableId()) {

        case TID_PAT: {
            if (table.sourcePID() == PID_PAT) {
                PAT pat (table);
                if (pat.isValid()) {
                    processPAT (pat);
                }
            }
            break;
        }

        case TID_SDT_ACT: {
            if (table.sourcePID() == PID_SDT) {
                SDT sdt (table);
                if (sdt.isValid()) {
                    processSDT (sdt);
                }
            }
            break;
        }

        case TID_PMT: {
            PMT pmt (table);
            if (pmt.isValid() && _service.hasId (pmt.service_id)) {
                processPMT (pmt);
            }
            break;
        }

        case TID_TOT: {
            if (table.sourcePID() == PID_TOT) {
                // Save last TOT
                _last_tot.deserialize (table);
            }
            break;
        }

        default: {
            break;
        }
    }
}


//----------------------------------------------------------------------------
//  This method processes a Service Description Table (SDT).
//----------------------------------------------------------------------------

void ts::ClearPlugin::processSDT (SDT& sdt)
{
    // Look for the service by name
    uint16_t service_id;
    assert (_service.hasName());
    if (!sdt.findService (_service.getName(), service_id)) {
        tsp->error ("service \"" + _service.getName() + "\" not found in SDT");
        _abort = true;
        return;
    }

    // Remember service id
    _service.setId (service_id);
    tsp->verbose ("found service \"" + _service.getName() + Format ("\", service id is 0x%04X", int (_service.getId())));

    // No longer need to filter the SDT
    _demux.removePID (PID_SDT);

    // Now filter the PAT to get the PMT PID
    _demux.addPID (PID_PAT);
    _service.clearPMTPID();
}


//----------------------------------------------------------------------------
//  This method processes a Program Association Table (PAT).
//----------------------------------------------------------------------------

void ts::ClearPlugin::processPAT (PAT& pat)
{
    if (_service.hasId()) {
        // The service id is known, search it in the PAT
        PAT::ServiceMap::const_iterator it = pat.pmts.find (_service.getId());
        if (it == pat.pmts.end()) {
            // Service not found, error
            tsp->error ("service id %d (0x%04X) not found in PAT", int (_service.getId()), int (_service.getId()));
            _abort = true;
            return;
        }
        // If a previous PMT PID was known, no long filter it
        if (_service.hasPMTPID()) {
            _demux.removePID (_service.getPMTPID());
        }
        // Found PMT PID
        _service.setPMTPID (it->second);
        _demux.addPID (it->second);
    }
    else if (!pat.pmts.empty()) {
        // No service specified, use first one in PAT
        PAT::ServiceMap::iterator it = pat.pmts.begin();
        _service.setId (it->first);
        _service.setPMTPID (it->second);
        _demux.addPID (it->second);
        tsp->verbose ("using service %d (0x%04X)", int (_service.getId()), int (_service.getId()));
    }
    else {
        // No service specified, no service in PAT, error
        tsp->error ("no service in PAT");
        _abort = true;
    }
}


//----------------------------------------------------------------------------
//  This method processes a Program Map Table (PMT).
//----------------------------------------------------------------------------

void ts::ClearPlugin::processPMT (PMT& pmt)
{
    // Collect all audio/video PIDS
    _clear_pids.reset();
    for (PMT::StreamMap::const_iterator it = pmt.streams.begin(); it != pmt.streams.end(); ++it) {
        const PID pid = it->first;
        const PMT::Stream& stream (it->second);
        if ((stream.isAudio() && !_video_only) || (stream.isVideo() && !_audio_only)) {
            _clear_pids.set (pid);
        }
    }
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::ClearPlugin::processPacket (TSPacket& pkt, bool& flush, bool& bitrate_changed)
{
    const PID pid = pkt.getPID();
    bool previous_pass = _pass_packets;

    // Filter interesting sections
    _demux.feedPacket (pkt);

    // If a fatal error occured during section analysis, give up.
    if (_abort) {
        return TSP_END;
    }

    // If this is a clear packet from an audio/video PID of the
    // reference service, let the packets pass.

    if (_clear_pids[pid] && pkt.isClear()) {
        _pass_packets = true;
        _last_clear_pkt = _current_pkt;
    }

    // Make sure we know how long to wait after the last clear packet

    if (_drop_after == 0) {
        // Number of packets in 1 second at current bitrate
        _drop_after = tsp->bitrate() / (PKT_SIZE * 8);
        if (_drop_after == 0) {
            tsp->error ("bitrate unknown or too low, use option --drop-after-packets");
            return TSP_END;
        }
        std::string dec (Decimal (_drop_after));
        tsp->verbose ("will drop %s packets after last clear packet", dec.c_str());
    }

    // If packets are passing but no clear packet recently found, drop packets

    if (_pass_packets && (_current_pkt - _last_clear_pkt) > _drop_after) {
        _pass_packets = false;
    }

    // Report state change in verbose mode

    if (_pass_packets != previous_pass && tsp->verbose()) {
        // State has changed
        std::string state (_pass_packets ? "passing" : "dropping");
        std::string curtime (_last_tot.isValid() && !_last_tot.regions.empty() ?
                             _last_tot.localTime(_last_tot.regions[0]).format (Time::DATE | Time::TIME) :
                             "unknown");
        tsp->log (Severity::Verbose,
                  "now " + state + " all packets, last TOT local time: " + curtime +
                  ", current packet: " + Decimal (_current_pkt));
    }

    // Count TS packets

    _current_pkt++;

    // Pass or drop the packets

    return _pass_packets ? TSP_OK : _drop_status;
}
