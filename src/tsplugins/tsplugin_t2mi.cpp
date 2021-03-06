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
//  Extract T2-MI (DVB-T2 Modulator Interface) packets.
//  See ETSI TS 102 775
//
//----------------------------------------------------------------------------

#include "tsPlugin.h"
#include "tsT2MIDemux.h"
#include "tsT2MIDescriptor.h"
#include "tsT2MIPacket.h"
#include "tsFormat.h"
#include "tsNames.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class T2MIPlugin:
        public ProcessorPlugin,
        private T2MIHandlerInterface
    {
    public:
        // Implementation of plugin API
        T2MIPlugin(TSP*);
        virtual bool start() override;
        virtual bool stop() override;
        virtual BitRate getBitrate() override {return 0;}
        virtual Status processPacket(TSPacket&, bool&, bool&) override;

    private:
        // Plugin private fields.
        bool          _extract;         // Extract encapsulated TS.
        bool          _log;             // Log T2-MI packets.
        PID           _pid;             // PID carrying the T2-MI encapsulation.
        uint8_t       _plp;             // The PLP to extract in _pid.
        bool          _plp_valid;       // False if PLP not yet known.
        PacketCounter _t2mi_count;      // Number of input T2-MI packets.
        PacketCounter _ts_count;        // Number of extracted TS packets.
        T2MIDemux     _demux;           // Demux for PSI parsing.
        std::deque<TSPacket> _ts_queue; // Queue of demuxed TS packets.

        // Inherited methods.
        virtual void handleT2MINewPID(T2MIDemux& demux, const PMT& pmt, PID pid, const T2MIDescriptor& desc) override;
        virtual void handleT2MIPacket(T2MIDemux& demux, const T2MIPacket& pkt) override;
        virtual void handleTSPacket(T2MIDemux& demux, const T2MIPacket& t2mi, const TSPacket& ts) override;

        // Inaccessible operations
        T2MIPlugin() = delete;
        T2MIPlugin(const T2MIPlugin&) = delete;
        T2MIPlugin& operator=(const T2MIPlugin&) = delete;
    };
}

TSPLUGIN_DECLARE_VERSION
TSPLUGIN_DECLARE_PROCESSOR(ts::T2MIPlugin)


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::T2MIPlugin::T2MIPlugin(TSP* tsp_) :
    ProcessorPlugin(tsp_, "Extract T2-MI (DVB-T2 Modulator Interface) packets.", "[options]"),
    T2MIHandlerInterface(),
    _extract(false),
    _log(false),
    _pid(PID_NULL),
    _plp(0),
    _plp_valid(false),
    _t2mi_count(0),
    _ts_count(0),
    _demux(this),
    _ts_queue()
{
    option("extract", 'e');
    option("log",     'l');
    option("pid",     'p', PIDVAL);
    option("plp",      0,  UINT8);

    setHelp("Options:\n"
            "\n"
            "  -e\n"
            "  --extract\n"
            "      Extract encapsulated TS packets from one PLP of a T2-MI stream.\n"
            "      The transport stream is completely replaced by the extracted stream.\n"
            "      This is the default if neither --extract nor --log is specified.\n"
            "\n"
            "  -l\n"
            "  --log\n"
            "      Log all T2-MI packets using one single summary line per packet.\n"
            "\n"
            "  --help\n"
            "      Display this help text.\n"
            "\n"
            "  -p value\n"
            "  --pid value\n"
            "      Specify the PID carrying the T2-MI encapsulation. By default, use the\n"
            "      first component with a T2MI_descriptor in a service.\n"
            "\n"
            "  --plp value\n"
            "      Specify the PLP (Physical Layer Pipe) to extract from the T2-MI\n"
            "      encapsulation. By default, use the first PLP which is found.\n"
            "      Ignored if --extract is not used.\n"
            "\n"
            "  --version\n"
            "      Display the version number.\n");
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::T2MIPlugin::start()
{
    // Get command line arguments
    _extract = present("extract");
    _log = present("log");
    getIntValue<PID>(_pid, "pid", PID_NULL);
    getIntValue<uint8_t>(_plp, "plp");
    _plp_valid = present("plp");

    // Extract is the default operation.
    if (!_extract && !_log) {
        _extract = true;
    }

    // Initialize the demux.
    _demux.reset();
    if (_pid != PID_NULL) {
        _demux.addPID(_pid);
    }

    // Reset the packet output.
    _ts_queue.clear();
    _t2mi_count = 0;
    _ts_count = 0;
    return true;
}


//----------------------------------------------------------------------------
// Stop method
//----------------------------------------------------------------------------

bool ts::T2MIPlugin::stop()
{
    if (_extract) {
        tsp->verbose("extracted " + Decimal(_ts_count) + " TS packets from " + Decimal(_t2mi_count) + " T2-MI packets");
    }
    return true;
}


//----------------------------------------------------------------------------
// Process new T2-MI PID.
//----------------------------------------------------------------------------

void ts::T2MIPlugin::handleT2MINewPID(T2MIDemux& demux, const PMT& pmt, PID pid, const T2MIDescriptor& desc)
{
    // Found a new PID carrying T2-MI. Use it by default.
    if (_pid == PID_NULL && pid != PID_NULL) {
        tsp->verbose("using PID 0x%04X (%d) to extract T2-MI stream", int(pid), int(pid));
        _pid = pid;
        _demux.addPID(_pid);
    }
}


//----------------------------------------------------------------------------
// Process a T2-MI packet.
//----------------------------------------------------------------------------

void ts::T2MIPlugin::handleT2MIPacket(T2MIDemux& demux, const T2MIPacket& pkt)
{
    // Log T2-MI packets.
    if (_log) {
        UString line(Format("PID 0x%04X (%d), packet type: ", int(pkt.getSourcePID()), int(pkt.getSourcePID())));
        line += names::T2MIPacketType(pkt.packetType(), names::HEXA_FIRST);
        line += Format(", size: %" FMT_SIZE_T "d bytes, packet count: %d, superframe index: %d, frame index: %d",
                       pkt.size(), int(pkt.packetCount()), int(pkt.superframeIndex()), int(pkt.frameIndex()));
        if (pkt.plpValid()) {
            line += Format(", PLP: 0x%02X (%d)", int(pkt.plp()), int(pkt.plp()));
        }
        tsp->info(line);
    }

    // Select PLP when extraction is requested.
    if (_extract && pkt.plpValid()) {
        if (!_plp_valid) {
            // The PLP was not yet specified, use this one by default.
            _plp = pkt.plp();
            _plp_valid = true;
            tsp->verbose("extracting PLP 0x%02X (%d)", int(_plp), int(_plp));
        }
        if (pkt.plp() == _plp) {
            // Count input T2-MI packets.
            _t2mi_count++;
        }
    }
}


//----------------------------------------------------------------------------
// Process an extracted TS packet from the T2-MI stream.
//----------------------------------------------------------------------------

void ts::T2MIPlugin::handleTSPacket(T2MIDemux& demux, const T2MIPacket& t2mi, const TSPacket& ts)
{
    // Nothing to do if no TS extraction is requested.
    if (!_extract) {
        return;
    }

    // PLP must be known since handleT2MIPacket() is always called before handleTSPacket().
    assert(_plp_valid);

    // Keep packet from the filtered PLP only.
    if (t2mi.plp() == _plp) {

        // Enqueue the TS packet.
        _ts_queue.push_back(ts);

        // We do not really care about queue size because an overflow is not possible.
        // This plugin deletes all input packets and replacez them with demux'ed packets.
        // And the number of input TS packets is always higher than the number of output
        // packets because of T2-MI encapsulation and other PID's.
    }
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::T2MIPlugin::processPacket(TSPacket& pkt, bool& flush, bool& bitrate_changed)
{
    // Feed the T2-MI demux.
    _demux.feedPacket(pkt);

    if (!_extract) {
        // Without TS extraction, we simply pass all packets, unchanged.
        return TSP_OK;
    }
    else if (_ts_queue.empty()) {
        // No extracted packet to output, drop current packet.
        return TSP_DROP;
    }
    else {
        // Replace the current packet with the next demux'ed TS packet.
        pkt = _ts_queue.front();
        _ts_queue.pop_front();
        _ts_count++;
        return TSP_OK;
    }
}
