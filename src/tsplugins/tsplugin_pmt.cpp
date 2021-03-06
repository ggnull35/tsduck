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
//  Various transformations on the PMT.
//
//----------------------------------------------------------------------------

#include "tsPlugin.h"
#include "tsSectionDemux.h"
#include "tsCyclingPacketizer.h"
#include "tsService.h"
#include "tsTables.h"
#include "tsAudioLanguageOptions.h"
#include "tsFormat.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class PMTPlugin: public ProcessorPlugin, private TableHandlerInterface
    {
    public:
        // Implementation of plugin API
        PMTPlugin (TSP*);
        virtual bool start();
        virtual bool stop() {return true;}
        virtual BitRate getBitrate() {return 0;}
        virtual Status processPacket (TSPacket&, bool&, bool&);

    private:
        // Description of a new component to add
        struct NewPID {
            PID     pid;
            uint8_t stream_type;

            // Constructor.
            NewPID(PID pid_ = PID_NULL, uint8_t stype_ = 0) :
                pid(pid_),
                stream_type(stype_)
            {
            }
        };

        // PMTPlugin instance fields
        bool                _abort;             // Error (service not found, etc)
        bool                _ready;             // Ready to perform transformation
        Service             _service;           // Service of PMT to modify
        std::vector<PID>    _removed_pid;       // Set of PIDs to remove from PMT
        std::vector<DID>    _removed_desc;      // Set of descriptor tags to remove
        std::list<NewPID>   _added_pid;         // List of PID to add
        std::map<PID,PID>   _moved_pid;         // List of renamed PID's in PMT (key=old, value=new)
        bool                _set_servid;        // Set a new service id
        uint16_t            _new_servid;        // New service id
        bool                _set_pcrpid;        // Set a new PCR PID
        PID                 _new_pcrpid;        // New PCR PID
        bool                _incr_version;      // Increment table version
        bool                _set_version;       // Set a new table version
        uint8_t             _new_version;       // New table version
        PDS                 _pds;               // Private data specifier for removed descriptors
        bool                _add_stream_id;     // Add stream_identifier_descriptor on all components
        bool                _ac3_atsc2dvb;      // Modify AC-3 signaling from ATSC to DVB method
        bool                _eac3_atsc2dvb;     // Modify Enhanced-AC-3 signaling from ATSC to DVB method
        bool                _cleanup_priv_desc; // Remove private desc without preceding PDS desc
        AudioLanguageOptionsVector _languages;  // Audio languages to set
        SectionDemux        _demux;             // Section demux
        CyclingPacketizer   _pzer;              // Packetizer for modified PMT

        // Invoked by the demux when a complete table is available.
        virtual void handleTable (SectionDemux&, const BinaryTable&);

        // Inaccessible operations
        PMTPlugin() = delete;
        PMTPlugin(const PMTPlugin&) = delete;
        PMTPlugin& operator=(const PMTPlugin&) = delete;
    };
}

TSPLUGIN_DECLARE_VERSION
TSPLUGIN_DECLARE_PROCESSOR(ts::PMTPlugin)


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::PMTPlugin::PMTPlugin (TSP* tsp_) :
    ProcessorPlugin (tsp_, "Perform various transformations on the PMT", "[options]"),
    _abort(false),
    _ready(false),
    _service(),
    _removed_pid(),
    _removed_desc(),
    _added_pid(),
    _moved_pid(),
    _set_servid(false),
    _new_servid(0),
    _set_pcrpid(false),
    _new_pcrpid(PID_NULL),
    _incr_version(false),
    _set_version(false),
    _new_version(0),
    _pds(0),
    _add_stream_id(false),
    _ac3_atsc2dvb(false),
    _eac3_atsc2dvb(false),
    _cleanup_priv_desc(false),
    _languages(),
    _demux(this),
    _pzer()
{
    option ("ac3-atsc2dvb",                0);
    option ("add-pid",                    'a', STRING, 0, UNLIMITED_COUNT);
    option ("add-stream-identifier",       0);
    option ("audio-language",              0,  STRING, 0, UNLIMITED_COUNT);
    option ("cleanup-private-descriptors", 0);
    option ("eac3-atsc2dvb",               0);
    option ("increment-version",           0);
    option ("new-service-id",             'i', UINT16);
    option ("move-pid",                   'm', STRING, 0, UNLIMITED_COUNT);
    option ("pds",                         0,  UINT32);
    option ("pmt-pid",                    'p', PIDVAL);
    option ("pcr-pid",                     0,  PIDVAL);
    option ("remove-descriptor",           0,  UINT8,  0, UNLIMITED_COUNT);
    option ("remove-pid",                 'r', PIDVAL, 0, UNLIMITED_COUNT);
    option ("service",                    's', STRING);
    option ("new-version",                'v', INTEGER, 0, 1, 0, 31);

    setHelp ("Options:\n"
             "\n"
             "  --ac3-atsc2dvb\n"
             "      Change the description of AC-3 audio streams from ATSC to DVB method.\n"
             "      In details, this means that all components with stream_type 0x81 are\n"
             "      modified with stream_type 0x06 (PES private data) and an AC-3_descriptor\n"
             "      is added on this component (if none was already there).\n"
             "\n"
             "  -a pid/stream_type\n"
             "  --add-pid pid/stream_type\n"
             "      Add the specified PID / stream-type component in the PMT. Several\n"
             "      --add-pid options may be specified to add several components.\n"
             "\n"
             "  --add-stream-identifier\n"
             "      Add a stream_identifier_descriptor on all components. The component_tag\n"
             "      are uniquely allocated inside the service. Existing stream_identifier\n"
             "      descriptors are left unmodified.\n"
             "\n"
             "  --audio-language " + AudioLanguageOptions::GetSyntaxString() + "\n"
             "      Specifies the language for an audio stream in the PMT. Several options\n"
             "      can be specified to set the languages of several audio streams.\n" +
             AudioLanguageOptions::GetHelpString() +
             "\n"
             "  --cleanup-private-descriptors\n"
             "      Remove all private descriptors without preceding private_data_specifier\n"
             "      descriptor.\n"
             "\n"
             "  --eac3-atsc2dvb\n"
             "      Change the description of Enhanced-AC-3 (aka AC-3+ or DD+) audio streams\n"
             "      from ATSC to DVB method. In details, this means that all components with\n"
             "      stream_type 0x87 are modified with stream_type 0x06 (PES private data)\n"
             "      and an enhanced_AC-3_descriptor is added on this component (if none was\n"
             "      already there).\n"
             "\n"
             "  --help\n"
             "      Display this help text.\n"
             "\n"
             "  --increment-version\n"
             "      Increment the version number of the PMT.\n"
             "\n"
             "  -i value\n"
             "  --new-service-id value\n"
             "      Change the service id in the PMT.\n"
             "\n"
             "  -m old-pid/new-pid\n"
             "  --move-pid old-pid/new-pid\n"
             "      Change the PID value of a component in the PMT. Several --move-pid\n"
             "      options may be specified to move several components.\n"
             "\n"
             "  --pds value\n"
             "      With option --remove-descriptor, specify the private data specifier\n"
             "      which applies to the descriptor tag values above 0x80.\n"
             "\n"
             "  -p value\n"
             "  --pmt-pid value\n"
             "      Specify the PID carrying the PMT to modify. All PMT's in this PID will be\n"
             "      modified. Options --pmt-pid and --service are mutually exclusive. If\n"
             "      neither are specified, the first service in the PAT is used.\n"
             "\n"
             "  --pcr-pid value\n"
             "      Change the PCR PID value in the PMT.\n"
             "\n"
             "  --remove-descriptor value\n"
             "      Remove from the PMT all descriptors with the specified tag. Several\n"
             "      --remove-descriptor options may be specified to remove several types of\n"
             "      descriptors. See also option --pds.\n"
             "\n"
             "  -r value\n"
             "  --remove-pid value\n"
             "      Remove the component with the specified PID from the PMT. Several\n"
             "      --remove-pid options may be specified to remove several components.\n"
             "\n"
             "  -s name-or-id\n"
             "  --service name-or-id\n"
             "      Specify the service the PMT of which must be modified. If the argument is\n"
             "      an integer value (either decimal or hexadecimal), it is interpreted as a\n"
             "      service id. Otherwise, it is interpreted as a service name, as specified\n"
             "      in the SDT. The name is not case sensitive and blanks are ignored.\n"
             "      Options --pmt-pid and --service are mutually exclusive. If neither are\n"
             "      specified, the first service in the PAT is used.\n"
             "\n"
             "  -v value\n"
             "  --new-version value\n"
             "      Specify a new value for the version of the PMT.\n"
             "\n"
             "  --version\n"
             "      Display the version number.\n");
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::PMTPlugin::start()
{
    _abort = false;
    _ready = false;
    _service.clear();
    _added_pid.clear();
    _moved_pid.clear();
    _demux.reset();
    _pzer.reset();

    // Get option values
    _set_servid = present ("new-service-id");
    _new_servid = intValue<uint16_t> ("new-service-id");
    _set_pcrpid = present ("pcr-pid");
    _new_pcrpid = intValue<PID> ("pcr-pid");
    _incr_version = present ("increment-version");
    _set_version = present ("new-version");
    _new_version = intValue<uint8_t> ("new-version");
    _pds = intValue<PDS> ("pds");
    _ac3_atsc2dvb = present ("ac3-atsc2dvb");
    _eac3_atsc2dvb = present ("eac3-atsc2dvb");
    _add_stream_id = present ("add-stream-identifier");
    _cleanup_priv_desc = present ("cleanup-private-descriptors");
    getIntValues (_removed_pid, "remove-pid");
    getIntValues (_removed_desc, "remove-descriptor");

    // Get list of components to add
    const size_t add_count = count ("add-pid");
    for (size_t n = 0; n < add_count; n++) {
        const std::string s (value ("add-pid", "", n));
        int pid, stype;
        char unused;
        if (::sscanf(s.c_str(), "%i/%i%c", &pid, &stype, &unused) == 2 &&
            pid >= 0 && pid < PID_MAX && stype >= 0 && stype <= 0xFF) {
            _added_pid.push_back(NewPID(PID(pid), uint8_t(stype)));
        }
        else {
            error("invalid \"PID/stream-type\" value \"" + s + "\"");
            return false;
        }
    }

    // Get list of components to move
    const size_t move_count = count ("move-pid");
    for (size_t n = 0; n < move_count; n++) {
        std::string s (value ("move-pid", "", n));
        int opid, npid;
        char unused;
        if (::sscanf (s.c_str(), "%i/%i%c", &opid, &npid, &unused) != 2 || opid < 0 || opid >= PID_MAX || npid < 0 || npid >= PID_MAX) {
            error ("invalid \"old-PID/new-PID\" value \"" + s + "\"");
            return false;
        }
        _moved_pid [PID (opid)] = PID (npid);
    }

    // Get audio languages to set
    if (!_languages.getFromArgs (*this, "audio-language")) {
        return false;
    }

    // Get PMT PID or service description
    if (present ("pmt-pid") && present ("service")) {
        error ("options --pmt-pid and --service are mutually exclusive");
        return false;
    }
    if (present ("pmt-pid")) {
        // A PMT PID is specified, we are now ready to modify all PMT's in this PID
        _service.setPMTPID (intValue<PID> ("pmt-pid"));
    }
    else if (present ("service")) {
        _service.set (value ("service"));
    }

    // Determine which PID we need to process
    if (_service.hasPMTPID()) {
        // PMT PID directly known
        _demux.addPID (_service.getPMTPID());
        _pzer.setPID (_service.getPMTPID());
        _ready = true;
    }
    else if (_service.hasName()) {
        // Need to filter the SDT to get the service id
        _demux.addPID (PID_SDT);
    }
    else {
        // Need to filter the PAT to get the PMT PID
        _demux.addPID (PID_PAT);
    }

    return true;
}


//----------------------------------------------------------------------------
// Invoked by the demux when a complete table is available.
//----------------------------------------------------------------------------

void ts::PMTPlugin::handleTable (SectionDemux& demux, const BinaryTable& table)
{
    switch (table.tableId()) {

        case TID_SDT_ACT: {
            if (table.sourcePID() == PID_SDT) {
                // Process an SDT: We are looking for service id
                SDT sdt (table);
                if (!sdt.isValid()) {
                    return;
                }
                // Look for the service by name
                if (!sdt.findService (_service)) {
                    tsp->error ("service \"" + _service.getName() + "\" not found in SDT");
                    _abort = true;
                    return;
                }
                tsp->verbose ("found service \"" + _service.getName() + Format ("\", service id is 0x%04X", int (_service.getId())));
                // No longer need to filter the SDT
                _demux.removePID (PID_SDT);
                // Now filter the PAT to get the PMT PID
                _demux.addPID (PID_PAT);
            }
            break;
        }

        case TID_PAT: {
            if (table.sourcePID() == PID_PAT) {
                // Process a PAT: We are looking for the PMT PID
                PAT pat (table);
                if (!pat.isValid()) {
                    return;
                }
                if (_service.hasId()) {
                    // The service id is known, search it in the PAT
                    PAT::ServiceMap::const_iterator it = pat.pmts.find (_service.getId());
                    if (it == pat.pmts.end()) {
                        // Service not found, error
                        tsp->error ("service id %d (0x%04X) not found in PAT", int (_service.getId()), int (_service.getId()));
                        _abort = true;
                        return;
                    }
                    _service.setPMTPID (it->second);
                }
                else if (!pat.pmts.empty()) {
                    // No service specified, use first one in PAT
                    PAT::ServiceMap::iterator it = pat.pmts.begin();
                    _service.setId (it->first);
                    _service.setPMTPID (it->second);
                    tsp->verbose ("using service %d (0x%04X)", int (_service.getId()), int (_service.getId()));
                }
                else {
                    // No service specified, no service in PAT, error
                    tsp->error ("no service in PAT");
                    _abort = true;
                    return;
                }
                // Found PMT PID, now ready to process PMT
                _demux.addPID (_service.getPMTPID());
                _pzer.setPID (_service.getPMTPID());
                _ready = true;
                // No longer need to filter the PAT
                _demux.removePID (PID_PAT);
            }
            break;
        }

        case TID_PMT: {
            // If not yet ready, skip it
            if (!_ready) {
                return;
            }
            // If a service id is specified, filter it
            if (_service.hasId() && !_service.hasId (table.tableIdExtension())) {
                return;
            }
            // Decode the PMT
            PMT pmt (table);
            if (!pmt.isValid()) {
                return;
            }
            // Modify service id
            if (_set_servid) {
                pmt.service_id = _new_servid;
            }
            // Modify table version
            if (_incr_version) {
                pmt.version = (pmt.version + 1) & SVERSION_MASK;
            }
            else if (_set_version) {
                pmt.version = _new_version;
            }
            // Modify PCR PID
            if (_set_pcrpid) {
                pmt.pcr_pid = _new_pcrpid;
            }
            // Modify audio languages
            _languages.apply (pmt, *tsp);
            // Remove components
            for (std::vector<PID>::const_iterator it = _removed_pid.begin(); it != _removed_pid.end(); ++it) {
                pmt.streams.erase (*it);
            }
            // Add new components
            for (std::list<NewPID>::const_iterator it = _added_pid.begin(); it != _added_pid.end(); ++it) {
                PMT::Stream& ps (pmt.streams[it->pid]);
                ps.stream_type = it->stream_type;
            }
            // Change the PID of components
            for (std::map<PID,PID>::const_iterator it = _moved_pid.begin(); it != _moved_pid.end(); ++it) {
                // Check if component exists
                if (it->first != it->second && pmt.streams.find (it->first) != pmt.streams.end()) {
                    pmt.streams[it->second] = pmt.streams[it->first];
                    pmt.streams.erase (it->first);
                }
            }
            // Remove descriptors
            for (std::vector<DID>::const_iterator it = _removed_desc.begin(); it != _removed_desc.end(); ++it) {
                pmt.descs.removeByTag (*it, _pds);
                for (PMT::StreamMap::iterator smi = pmt.streams.begin(); smi != pmt.streams.end(); ++smi) {
                    smi->second.descs.removeByTag (*it, _pds);
                }
            }
            // Modify AC-3 signaling from ATSC to DVB method
            if (_ac3_atsc2dvb) {
                for (PMT::StreamMap::iterator smi = pmt.streams.begin(); smi != pmt.streams.end(); ++smi) {
                    if (smi->second.stream_type == ST_AC3_AUDIO) {
                        smi->second.stream_type = ST_PES_PRIV;
                        if (smi->second.descs.search (DID_AC3) == smi->second.descs.count()) {
                            // No AC-3_descriptor present in this component, add one.
                            smi->second.descs.add (AC3Descriptor());
                        }
                    }
                }
            }
            // Modify Enhanced-AC-3 signaling from ATSC to DVB method
            if (_eac3_atsc2dvb) {
                for (PMT::StreamMap::iterator smi = pmt.streams.begin(); smi != pmt.streams.end(); ++smi) {
                    if (smi->second.stream_type == ST_EAC3_AUDIO) {
                        smi->second.stream_type = ST_PES_PRIV;
                        if (smi->second.descs.search (DID_ENHANCED_AC3) == smi->second.descs.count()) {
                            // No enhanced_AC-3_descriptor present in this component, add one.
                            smi->second.descs.add (EnhancedAC3Descriptor());
                        }
                    }
                }
            }
            // Remove private descriptors without preceding PDS descriptor
            if (_cleanup_priv_desc) {
                pmt.descs.removeInvalidPrivateDescriptors();
                for (PMT::StreamMap::iterator smi = pmt.streams.begin(); smi != pmt.streams.end(); ++smi) {
                    smi->second.descs.removeInvalidPrivateDescriptors();
                }
            }
            // Add stream_identifier_descriptor on all components.
            if (_add_stream_id) {
                // First, look for existing descriptors, collect component tags.
                std::bitset<256> ctags;
                for (PMT::StreamMap::iterator smi = pmt.streams.begin(); smi != pmt.streams.end(); ++smi) {
                    const DescriptorList& dlist (smi->second.descs);
                    for (size_t i = dlist.search (DID_STREAM_ID); i < dlist.count(); i = dlist.search (DID_STREAM_ID, i + 1)) {
                        const StreamIdentifierDescriptor sid (*dlist[i]);
                        if (sid.isValid()) {
                            ctags.set (sid.component_tag);
                        }
                    }
                }
                // Then, add a stream_identifier_descriptor on all components
                for (PMT::StreamMap::iterator smi = pmt.streams.begin(); smi != pmt.streams.end(); ++smi) {
                    DescriptorList& dlist (smi->second.descs);
                    // Skip components already containing a stream_identifier_descriptor
                    if (dlist.search (DID_STREAM_ID) < dlist.count()) {
                        continue;
                    }
                    // Allocate a new component tag
                    StreamIdentifierDescriptor sid;
                    for (size_t i = 0; i < ctags.size(); i++) {
                        if (! ctags.test (i)) {
                            sid.component_tag = uint8_t (i);
                            ctags.set (i);
                            break;
                        }
                    }
                    // Add the stream_identifier_descriptor in the component
                    dlist.add (sid);
                }
            }
            // Place modified PMT in the packetizer
            tsp->verbose ("PMT version %d modified", int (pmt.version));
            _pzer.removeSections (TID_PMT, pmt.service_id);
            _pzer.addTable (pmt);
            break;
        }

        default: {
            break;
        }
    }
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::PMTPlugin::processPacket (TSPacket& pkt, bool& flush, bool& bitrate_changed)
{
    // Filter interesting sections
    _demux.feedPacket (pkt);

    // If a fatal error occured during section analysis, give up.
    if (_abort) {
        return TSP_END;
    }

    // While not ready (ie. don't know which PID to modify), drop all packets
    // to avoid transmitting partial unmodified table.
    if (!_ready) {
        return TSP_DROP;
    }

    // Replace packets in PMT PID using packetizer
    if (_service.hasPMTPID (pkt.getPID())) {
        _pzer.getNextPacket (pkt);
    }
    return TSP_OK;
}
