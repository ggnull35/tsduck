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
//  Representation of an Event Information Table (EIT)
//
//----------------------------------------------------------------------------

#include "tsEIT.h"
#include "tsFormat.h"
#include "tsNames.h"
#include "tsRST.h"
#include "tsBCD.h"
#include "tsMJD.h"
#include "tsStringUtils.h"
#include "tsBinaryTable.h"
#include "tsTablesDisplay.h"
#include "tsTablesFactory.h"
#include "tsXMLTables.h"
TSDUCK_SOURCE;
TS_XML_TABLE_FACTORY(ts::EIT, "EIT");
TS_ID_TABLE_RANGE_FACTORY(ts::EIT, ts::TID_EIT_MIN, ts::TID_EIT_MAX);
TS_ID_SECTION_RANGE_DISPLAY(ts::EIT::DisplaySection, ts::TID_EIT_MIN, ts::TID_EIT_MAX);


//----------------------------------------------------------------------------
// Default constructor
//----------------------------------------------------------------------------

ts::EIT::EIT(bool is_actual_,
             bool is_pf_,
             uint8_t eits_index_,
             uint8_t version_,
             bool is_current_,
             uint16_t service_id_,
             uint16_t ts_id_,
             uint16_t onetw_id_) :
    AbstractLongTable(ComputeTableId(is_actual_, is_pf_, eits_index_), "EIT", version_, is_current_),
    service_id(service_id_),
    ts_id(ts_id_),
    onetw_id(onetw_id_),
    segment_last(0),
    last_table_id(_table_id),
    events()
{
    _is_valid = true;
}


//----------------------------------------------------------------------------
// Constructor from a binary table
//----------------------------------------------------------------------------

ts::EIT::EIT(const BinaryTable& table, const DVBCharset* charset) :
    AbstractLongTable(TID_EIT_PF_ACT, "EIT"),  // TID will be updated by deserialize()
    service_id(0),
    ts_id(0),
    onetw_id(0),
    segment_last(0),
    last_table_id(0),
    events()
{
    deserialize(table, charset);
}


//----------------------------------------------------------------------------
// Compute an EIT table id.
//----------------------------------------------------------------------------

ts::TID ts::EIT::ComputeTableId(bool is_actual, bool is_pf, uint8_t eits_index)
{
    if (is_pf) {
        return is_actual ? TID_EIT_PF_ACT : TID_EIT_PF_OTH;
    }
    else {
        return (is_actual ? TID_EIT_S_ACT_MIN : TID_EIT_S_OTH_MIN) + (eits_index & 0x0F);
    }
}


//----------------------------------------------------------------------------
// Check if this is an "actual" EIT.
//----------------------------------------------------------------------------

bool ts::EIT::isActual() const
{
    return _table_id == TID_EIT_PF_ACT || (_table_id >= TID_EIT_S_ACT_MIN &&_table_id <= TID_EIT_S_ACT_MAX);
}


//----------------------------------------------------------------------------
// Set if this is an "actual" EIT.
//----------------------------------------------------------------------------

void ts::EIT::setActual(bool is_actual)
{
    if (isPresentFollowing()) {
        _table_id = is_actual ? TID_EIT_PF_ACT : TID_EIT_PF_OTH;
        last_table_id = _table_id;
    }
    else if (is_actual) {
        _table_id = TID_EIT_S_ACT_MIN + (_table_id & 0x0F);
        last_table_id = TID_EIT_S_ACT_MIN + (last_table_id & 0x0F);
    }
    else {
        _table_id = TID_EIT_S_OTH_MIN + (_table_id & 0x0F);
        last_table_id = TID_EIT_S_ACT_MIN + (last_table_id & 0x0F);
    }
}


//----------------------------------------------------------------------------
// Deserialization
//----------------------------------------------------------------------------

void ts::EIT::deserialize(const BinaryTable& table, const DVBCharset* charset)
{
    // Clear table content
    _is_valid = false;
    service_id = 0;
    ts_id = 0;
    onetw_id = 0;
    segment_last = 0;
    last_table_id = _table_id;
    events.clear();

    if (!table.isValid()) {
        return;
    }

    // Check table id.
    if ((_table_id = table.tableId()) < TID_EIT_MIN || _table_id > TID_EIT_MAX) {
        return;
    }

    // Loop on all sections
    for (size_t si = 0; si < table.sectionCount(); ++si) {

        // Reference to current section
        const Section& sect(*table.sectionAt(si));

        // Abort if not expected table
        if (sect.tableId() != _table_id) {
            return;
        }

        // Get common properties (should be identical in all sections)
        version = sect.version();
        is_current = sect.isCurrent();
        service_id = sect.tableIdExtension();

        // Analyze the section payload:
        const uint8_t* data = sect.payload();
        size_t remain = sect.payloadSize();

        if (remain < 6) {
            return;
        }

        ts_id = GetUInt16(data);
        onetw_id = GetUInt16(data + 2);
        segment_last = data[4];
        last_table_id = data[5];
        data += 6;
        remain -= 6;

        // Get services description
        while (remain >= 12) {
            uint16_t event_id = GetUInt16(data);
            Event& event(events[event_id]);
            DecodeMJD(data + 2, 5, event.start_time);
            const int hour = DecodeBCD(data[7]);
            const int min = DecodeBCD(data[8]);
            const int sec = DecodeBCD(data[9]);
            event.duration = (hour * 3600) + (min * 60) + sec;
            event.running_status = (data[10] >> 5) & 0x07;
            event.CA_controlled = (data[10] >> 4) & 0x01;

            size_t info_length = GetUInt16(data + 10) & 0x0FFF;
            data += 12;
            remain -= 12;

            info_length = std::min(info_length, remain);
            event.descs.add(data, info_length);
            data += info_length;
            remain -= info_length;
        }
    }

    _is_valid = true;
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::EIT::serialize(BinaryTable& table, const DVBCharset* charset) const
{
    // Reinitialize table object
    table.clear();

    // Return an empty table if not valid
    if (!_is_valid) {
        return;
    }

    // Build the sections
    uint8_t payload[MAX_PSI_LONG_SECTION_PAYLOAD_SIZE];
    int section_number = 0;
    uint8_t* data = payload;
    size_t remain = sizeof(payload);

    // The first 6 bytes are identical in all section. Build them once.
    PutUInt16(data, ts_id);
    PutUInt16(data + 2, onetw_id);
    data[4] = segment_last;
    data[5] = last_table_id;
    data += 6;
    remain -= 6;

    // Add all events
    for (EventMap::const_iterator it = events.begin(); it != events.end(); ++it) {

        const uint16_t event_id = it->first;
        const Event& event(it->second);

        // If we cannot at least add the fixed part, open a new section
        if (remain < 12) {
            addSection(table, section_number, payload, data, remain);
        }

        // Insert the characteristics of the event. When the section is
        // not large enough to hold the entire descriptor list, open a
        // new section for the rest of the descriptors. In that case, the
        // common properties of the event must be repeated.
        bool starting = true;
        size_t start_index = 0;

        while (starting || start_index < event.descs.count()) {

            // If we are at the beginning of an event description, make sure that the
            // entire event description fits in the section. If it does not fit, start
            // a new section. Note that huge event descriptions may not fit into one
            // section. In that case, the event description will span two sections later.
            if (starting && 12 + event.descs.binarySize() > remain) {
                addSection(table, section_number, payload, data, remain);
            }

            starting = false;

            // Insert common characteristics of the service
            assert(remain >= 12);
            PutUInt16(data, event_id);
            EncodeMJD(event.start_time, data + 2, 5);
            data[7] = EncodeBCD(int(event.duration / 3600));
            data[8] = EncodeBCD(int((event.duration / 60) % 60));
            data[9] = EncodeBCD(int(event.duration % 60));
            data += 10;
            remain -= 10;

            // Insert descriptors (all or some).
            uint8_t* flags = data;
            start_index = event.descs.lengthSerialize(data, remain, start_index);

            // The following fields are inserted in the 4 "reserved" bits of the descriptor_loop_length.
            flags[0] = (flags[0] & 0x0F) | (event.running_status << 5) | (event.CA_controlled ? 0x10 : 0x00);

            // If not all descriptors were written, the section is full.
            // Open a new one and continue with this service.
            if (start_index < event.descs.count()) {
                addSection(table, section_number, payload, data, remain);
            }
        }
    }

    // Add partial section (if there is one)
    if (data > payload + 6 || table.sectionCount() == 0) {
        addSection(table, section_number, payload, data, remain);
    }
}


//----------------------------------------------------------------------------
// Private method: Add a new section to a table being serialized.
// Session number is incremented. Data and remain are reinitialized.
//----------------------------------------------------------------------------

void ts::EIT::addSection(BinaryTable& table, int& section_number, uint8_t* payload, uint8_t*& data, size_t& remain) const
{
    table.addSection(new Section(_table_id,
                                 true,         // is_private_section
                                 service_id,   // tid_ext
                                 version,
                                 is_current,
                                 uint8_t(section_number),
                                 uint8_t(section_number), //last_section_number
                                 payload,
                                 data - payload)); // payload_size,
 
    // Reinitialize pointers.
    // Restart after constant part of payload (6 bytes).
    remain += data - payload - 6;
    data = payload + 6;
    section_number++;
}


//----------------------------------------------------------------------------
// Event description constructor
//----------------------------------------------------------------------------

ts::EIT::Event::Event() :
    start_time(),
    duration(0),
    running_status(0),
    CA_controlled(false),
    descs()
{
}


//----------------------------------------------------------------------------
// A static method to display an EIT section.
//----------------------------------------------------------------------------

void ts::EIT::DisplaySection(TablesDisplay& display, const ts::Section& section, int indent)
{
    std::ostream& strm(display.out());
    const std::string margin(indent, ' ');
    const uint8_t* data = section.payload();
    size_t size = section.payloadSize();
    const uint16_t sid = section.tableIdExtension();

    strm << margin << Format("Service Id: %d (0x%04X)", int(sid), int(sid)) << std::endl;

    if (size >= 6) {
        uint16_t tsid = GetUInt16(data);
        uint16_t onid = GetUInt16(data + 2);
        uint8_t seg_last = data[4];
        uint8_t last_tid = data[5];
        data += 6; size -= 6;

        strm << margin << Format("TS Id: %d (0x%04X)", int(tsid), int(tsid)) << std::endl
             << margin << Format("Original Network Id: %d (0x%04X)", int(onid), int(onid)) << std::endl
             << margin << Format("Segment last section: %d (0x%02X)", int(seg_last), int(seg_last)) << std::endl
             << margin << Format("Last Table Id: %d (0x%02X), ", int(last_tid), int(last_tid))
             << names::TID(last_tid) << std::endl;
    }

    while (size >= 12) {
        uint16_t evid = GetUInt16(data);
        Time start;
        DecodeMJD(data + 2, 5, start);
        int hour = DecodeBCD(data[7]);
        int min = DecodeBCD(data[8]);
        int sec = DecodeBCD(data[9]);
        uint8_t run = (data[10] >> 5) & 0x07;
        uint8_t ca_mode = (data[10] >> 4) & 0x01;
        size_t loop_length = GetUInt16(data + 10) & 0x0FFF;
        data += 12; size -= 12;
        if (loop_length > size) {
            loop_length = size;
        }
        strm << margin << Format("Event Id: %d (0x%04X)", int(evid), int(evid)) << std::endl
             << margin << "Start UTC: " << start.format(Time::DATE | Time::TIME) << std::endl
             << margin << Format("Duration: %02d:%02d:%02d", hour, min, sec) << std::endl
             << margin << "Running status: " << names::RunningStatus(run) << std::endl
             << margin << "CA mode: " << (ca_mode ? "controlled" : "free") << std::endl;
        display.displayDescriptorList(data, loop_length, indent, section.tableId());
        data += loop_length; size -= loop_length;
    }

    display.displayExtraData(data, size, indent);
}


//----------------------------------------------------------------------------
// XML serialization
//----------------------------------------------------------------------------

ts::XML::Element* ts::EIT::toXML(XML& xml, XML::Element* parent) const
{
    XML::Element* root = _is_valid ? xml.addElement(parent, _xml_name) : 0;
    if (isPresentFollowing()) {
        xml.setAttribute(root, "type", "pf");
    }
    else {
        xml.setIntAttribute(root, "type", _table_id - (isActual() ? TID_EIT_S_ACT_MIN : TID_EIT_S_OTH_MIN));
    }
    xml.setIntAttribute(root, "version", version);
    xml.setBoolAttribute(root, "current", is_current);
    xml.setBoolAttribute(root, "actual", isActual());
    xml.setIntAttribute(root, "service_id", service_id, true);
    xml.setIntAttribute(root, "transport_stream_id", ts_id, true);
    xml.setIntAttribute(root, "original_network_id", onetw_id, true);
    xml.setIntAttribute(root, "segment_last_section_number", segment_last, true);
    xml.setIntAttribute(root, "last_table_id", last_table_id, true);

    for (EventMap::const_iterator it = events.begin(); it != events.end(); ++it) {
        XML::Element* e = xml.addElement(root, "event");
        xml.setIntAttribute(e, "event_id", it->first, true);
        xml.setDateTimeAttribute(e, "start_time", it->second.start_time);
        xml.setTimeAttribute(e, "duration", it->second.duration);
        xml.setEnumAttribute(RST::RunningStatusNames, e, "running_status", it->second.running_status);
        xml.setBoolAttribute(e, "CA_mode", it->second.CA_controlled);
        XMLTables::ToXML(xml, e, it->second.descs);
    }
    return root;
}


//----------------------------------------------------------------------------
// XML deserialization
//----------------------------------------------------------------------------

void ts::EIT::fromXML(XML& xml, const XML::Element* element)
{
    events.clear();
    std::string type;
    bool actual = 0;

    XML::ElementVector children;
    _is_valid =
        checkXMLName(xml, element) &&
        xml.getAttribute(type, element, "type", false, "pf") &&
        xml.getIntAttribute<uint8_t>(version, element, "version", false, 0, 0, 31) &&
        xml.getBoolAttribute(is_current, element, "current", false, true) &&
        xml.getBoolAttribute(actual, element, "actual", false, true) &&
        xml.getIntAttribute<uint16_t>(service_id, element, "service_id", true, 0, 0x0000, 0xFFFF) &&
        xml.getIntAttribute<uint16_t>(ts_id, element, "transport_stream_id", true, 0, 0x0000, 0xFFFF) &&
        xml.getIntAttribute<uint16_t>(onetw_id, element, "original_network_id", true, 0, 0x00, 0xFFFF) &&
        xml.getIntAttribute<uint8_t>(segment_last, element, "segment_last_section_number", true, 0, 0x00, 0xFF) &&
        xml.getIntAttribute<TID>(last_table_id, element, "last_table_id", true, 0, 0x00, 0xFF) &&
        xml.getChildren(children, element, "event");

    // Update table id.
    if (_is_valid) {
        if (SimilarStrings(type, "pf")) {
            // This is an EIT p/f
            _table_id = actual ? TID_EIT_PF_ACT : TID_EIT_PF_OTH;
        }
        else if (ToInteger(_table_id, type)) {
            // This is an EIT schedule
            _table_id += actual ? TID_EIT_S_ACT_MIN : TID_EIT_S_OTH_MIN;
        }
        else {
            xml.reportError(Format("'%s' is not a valid value for attribute 'type' in <%s>, line %d",
                                   type.c_str(), XML::ElementName(element), element->GetLineNum()));
            _is_valid = false;
        }
    }

    // Get all events.
    for (size_t i = 0; _is_valid && i < children.size(); ++i) {
        Event event;
        uint16_t event_id = 0;
        _is_valid =
            xml.getIntAttribute<uint16_t>(event_id, children[i], "event_id", true, 0, 0x0000, 0xFFFF) &&
            xml.getDateTimeAttribute(event.start_time, children[i], "start_time", true) &&
            xml.getTimeAttribute(event.duration, children[i], "duration", true) &&
            xml.getIntEnumAttribute<uint8_t>(event.running_status, RST::RunningStatusNames, children[i], "running_status", false, 0) &&
            xml.getBoolAttribute(event.CA_controlled, children[i], "CA_mode", false, false) &&
            XMLTables::FromDescriptorListXML(event.descs, xml, children[i]);
        if (_is_valid) {
            events[event_id] = event;
        }
    }
}
