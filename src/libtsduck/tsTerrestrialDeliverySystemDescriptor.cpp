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
//  Representation of a terrestrial_delivery_system_descriptor
//
//----------------------------------------------------------------------------

#include "tsTerrestrialDeliverySystemDescriptor.h"
#include "tsTunerParametersDVBT.h"
#include "tsDecimal.h"
#include "tsTablesDisplay.h"
#include "tsTablesFactory.h"
TSDUCK_SOURCE;
TS_XML_DESCRIPTOR_FACTORY(ts::TerrestrialDeliverySystemDescriptor, "terrestrial_delivery_system_descriptor");
TS_ID_DESCRIPTOR_FACTORY(ts::TerrestrialDeliverySystemDescriptor, ts::EDID(ts::DID_TERREST_DELIVERY));
TS_ID_DESCRIPTOR_DISPLAY(ts::TerrestrialDeliverySystemDescriptor::DisplayDescriptor, ts::EDID(ts::DID_TERREST_DELIVERY));


//----------------------------------------------------------------------------
// Default constructor:
//----------------------------------------------------------------------------

ts::TerrestrialDeliverySystemDescriptor::TerrestrialDeliverySystemDescriptor() :
    AbstractDeliverySystemDescriptor(DID_TERREST_DELIVERY, DS_DVB_T, "terrestrial_delivery_system_descriptor"),
    centre_frequency(0),
    bandwidth(0),
    high_priority(true),
    no_time_slicing(true),
    no_mpe_fec(true),
    constellation(0),
    hierarchy(0),
    code_rate_hp(0),
    code_rate_lp(0),
    guard_interval(0),
    transmission_mode(0),
    other_frequency(false)
{
    _is_valid = true;
}


//----------------------------------------------------------------------------
// Constructor from a binary descriptor
//----------------------------------------------------------------------------

ts::TerrestrialDeliverySystemDescriptor::TerrestrialDeliverySystemDescriptor(const Descriptor& desc, const DVBCharset* charset) :
    AbstractDeliverySystemDescriptor(DID_TERREST_DELIVERY, DS_DVB_T, "terrestrial_delivery_system_descriptor"),
    centre_frequency(0),
    bandwidth(0),
    high_priority(true),
    no_time_slicing(true),
    no_mpe_fec(true),
    constellation(0),
    hierarchy(0),
    code_rate_hp(0),
    code_rate_lp(0),
    guard_interval(0),
    transmission_mode(0),
    other_frequency(false)
{
    deserialize(desc, charset);
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::TerrestrialDeliverySystemDescriptor::serialize(Descriptor& desc, const DVBCharset* charset) const
{
    uint8_t data[13];
    data[0] = _tag;
    data[1] = 11;
    PutUInt32(data + 2, centre_frequency);
    data[6] = (bandwidth << 5) |
              (uint8_t(high_priority) << 4) |
              (uint8_t(no_time_slicing) << 3) |
              (uint8_t(no_mpe_fec) << 2) |
              0x03;
    data[7] = (constellation << 6) |
              ((hierarchy & 0x07) << 3) |
              (code_rate_hp & 0x07);
    data[8] = (code_rate_lp << 5) |
              ((guard_interval & 0x03) << 3) |
              ((transmission_mode & 0x03) << 1) |
              (uint8_t(other_frequency));
    data[9] = data[10] = data[11] = data[12] = 0xFF;

    Descriptor d(data, sizeof(data));
    desc = d;
}


//----------------------------------------------------------------------------
// Deserialization
//----------------------------------------------------------------------------

void ts::TerrestrialDeliverySystemDescriptor::deserialize(const Descriptor& desc, const DVBCharset* charset)
{
    _is_valid = desc.isValid() && desc.tag() == _tag && desc.payloadSize() >= 7;

    if (_is_valid) {
        const uint8_t* data = desc.payload();
        centre_frequency = GetUInt32(data);
        bandwidth = (data[4] >> 5) & 0x07;
        high_priority = (data[4] & 0x10) != 0;
        no_time_slicing = (data[4] & 0x08) != 0;
        no_mpe_fec = (data[4] & 0x04) != 0;
        constellation = (data[5] >> 6) & 0x03;
        hierarchy = (data[5] >> 3) & 0x07;
        code_rate_hp = data[5] & 0x07;
        code_rate_lp = (data[6] >> 5) & 0x07;
        guard_interval = (data[6] >> 3) & 0x03;
        transmission_mode = (data[6] >> 1) & 0x03;
        other_frequency = (data[6] & 0x01) != 0;
    }
}


//----------------------------------------------------------------------------
// Static method to display a descriptor.
//----------------------------------------------------------------------------

void ts::TerrestrialDeliverySystemDescriptor::DisplayDescriptor(TablesDisplay& display, DID did, const uint8_t* data, size_t size, int indent, TID tid, PDS pds)
{
    std::ostream& strm(display.out());
    const std::string margin(indent, ' ');

    if (size >= 11) {
        uint32_t cfreq = GetUInt32(data);
        uint8_t bwidth = data[4] >> 5;
        uint8_t prio = (data[4] >> 4) & 0x01;
        uint8_t tslice = (data[4] >> 3) & 0x01;
        uint8_t mpe_fec = (data[4] >> 2) & 0x01;
        uint8_t constel = data[5] >> 6;
        uint8_t hierarchy = (data[5] >> 3) & 0x07;
        uint8_t rate_hp = data[5] & 0x07;
        uint8_t rate_lp = data[6] >> 5;
        uint8_t guard = (data[6] >> 3) & 0x03;
        uint8_t transm = (data[6] >> 1) & 0x03;
        bool other_freq = (data[6] & 0x01) != 0;
        data += 11; size -= 11;

        strm << margin << "Centre frequency: " << Decimal(10 * uint64_t(cfreq)) << " Hz, Bandwidth: ";
        switch (bwidth) {
            case 0:  strm << "8 MHz"; break;
            case 1:  strm << "7 MHz"; break;
            case 2:  strm << "6 MHz"; break;
            case 3:  strm << "5 MHz"; break;
            default: strm << "code " << int(bwidth) << " (reserved)"; break;
        }
        strm << std::endl
             << margin << "Priority: " << (prio ? "high" : "low")
             << ", Time slicing: " << (tslice ? "unused" : "used")
             << ", MPE-FEC: " << (mpe_fec ? "unused" : "used")
             << std::endl
             << margin << "Constellation pattern: ";
        switch (constel) {
            case 0:  strm << "QPSK"; break;
            case 1:  strm << "16-QAM"; break;
            case 2:  strm << "64-QAM"; break;
            case 3:  strm << "reserved"; break;
            default: assert(false);
        }
        strm << std::endl << margin << "Hierarchy: ";
        assert(hierarchy < 8);
        switch (hierarchy & 0x03) {
            case 0:  strm << "non-hierarchical"; break;
            case 1:  strm << "alpha = 1"; break;
            case 2:  strm << "alpha = 2"; break;
            case 3:  strm << "alpha = 4"; break;
            default: assert(false);
        }
        strm << ", " << ((hierarchy & 0x04) ? "in-depth" : "native")
             << " interleaver" << std::endl
             << margin << "Code rate: high prio: ";
        switch (rate_hp) {
            case 0:  strm << "1/2"; break;
            case 1:  strm << "2/3"; break;
            case 2:  strm << "3/4"; break;
            case 3:  strm << "5/6"; break;
            case 4:  strm << "7/8"; break;
            default: strm << "code " << int(rate_hp) << " (reserved)"; break;
        }
        strm << ", low prio: ";
        switch (rate_lp) {
            case 0:  strm << "1/2"; break;
            case 1:  strm << "2/3"; break;
            case 2:  strm << "3/4"; break;
            case 3:  strm << "5/6"; break;
            case 4:  strm << "7/8"; break;
            default: strm << "code " << int(rate_lp) << " (reserved)"; break;
        }
        strm << std::endl << margin << "Guard interval: ";
        switch (guard) {
            case 0:  strm << "1/32"; break;
            case 1:  strm << "1/16"; break;
            case 2:  strm << "1/8"; break;
            case 3:  strm << "1/4"; break;
            default: assert(false);
        }
        strm << std::endl << margin << "OFDM transmission mode: ";
        switch (transm) {
            case 0:  strm << "2k"; break;
            case 1:  strm << "8k"; break;
            case 2:  strm << "4k"; break;
            case 3:  strm << "reserved"; break;
            default: assert(false);
        }
        strm << ", other frequencies: " << YesNo(other_freq) << std::endl;
    }

    display.displayExtraData(data, size, indent);
}


//----------------------------------------------------------------------------
// Enumerations in XML data.
//----------------------------------------------------------------------------

namespace {
    const ts::Enumeration BandwidthNames(
        "8MHz", 0,
        "7MHz", 1,
        "6MHz", 2,
        "5MHz", 3,
        TS_NULL
    );
    const ts::Enumeration PriorityNames(
        "HP", 1,
        "LP", 0,
        TS_NULL
    );
    const ts::Enumeration ConstellationNames(
        "QPSK",   0,
        "16-QAM", 1,
        "64-QAM", 2,
        TS_NULL
    );
    const ts::Enumeration CodeRateNames(
        "1/2", 0,
        "2/3", 1,
        "3/4", 2,
        "5/6", 3,
        "7/8", 4,
        TS_NULL
    );
    const ts::Enumeration GuardIntervalNames(
        "1/32", 0,
        "1/16", 1,
        "1/8",  2,
        "1/4",  3,
        TS_NULL
    );
    const ts::Enumeration TransmissionModeNames(
        "2k", 0,
        "8k", 1,
        "4k", 2,
        TS_NULL
    );
}


//----------------------------------------------------------------------------
// XML serialization
//----------------------------------------------------------------------------

ts::XML::Element* ts::TerrestrialDeliverySystemDescriptor::toXML(XML& xml, XML::Element* parent) const
{
    XML::Element* root = _is_valid ? xml.addElement(parent, _xml_name) : 0;
    xml.setIntAttribute(root, "centre_frequency", 10 * uint64_t(centre_frequency), false);
    xml.setIntEnumAttribute(BandwidthNames, root, "bandwidth", bandwidth);
    xml.setIntEnumAttribute(PriorityNames, root, "priority", int(high_priority));
    xml.setBoolAttribute(root, "no_time_slicing", no_time_slicing);
    xml.setBoolAttribute(root, "no_MPE_FEC", no_mpe_fec);
    xml.setIntEnumAttribute(ConstellationNames, root, "constellation", constellation);
    xml.setIntAttribute(root, "hierarchy_information", hierarchy);
    xml.setIntEnumAttribute(CodeRateNames, root, "code_rate_HP_stream", code_rate_hp);
    xml.setIntEnumAttribute(CodeRateNames, root, "code_rate_LP_stream", code_rate_lp);
    xml.setIntEnumAttribute(GuardIntervalNames, root, "guard_interval", guard_interval);
    xml.setIntEnumAttribute(TransmissionModeNames, root, "transmission_mode", transmission_mode);
    xml.setBoolAttribute(root, "other_frequency", other_frequency);
    return root;
}


//----------------------------------------------------------------------------
// XML deserialization
//----------------------------------------------------------------------------

void ts::TerrestrialDeliverySystemDescriptor::fromXML(XML& xml, const XML::Element* element)
{
    uint64_t frequency = 0;
    _is_valid =
        checkXMLName(xml, element) &&
        xml.getIntAttribute<uint64_t>(frequency, element, "centre_frequency", true) &&
        xml.getIntEnumAttribute(bandwidth, BandwidthNames, element, "bandwidth", true) &&
        xml.getIntEnumAttribute(high_priority, PriorityNames, element, "priority", true) &&
        xml.getBoolAttribute(no_time_slicing, element, "no_time_slicing", true) &&
        xml.getBoolAttribute(no_mpe_fec, element, "no_MPE_FEC", true) &&
        xml.getIntEnumAttribute(constellation, ConstellationNames, element, "constellation", true) &&
        xml.getIntAttribute<uint8_t>(hierarchy, element, "hierarchy_information", true) &&
        xml.getIntEnumAttribute(code_rate_hp, CodeRateNames, element, "code_rate_HP_stream", true) &&
        xml.getIntEnumAttribute(code_rate_lp, CodeRateNames, element, "code_rate_LP_stream", true) &&
        xml.getIntEnumAttribute(guard_interval, GuardIntervalNames, element, "guard_interval", true) &&
        xml.getIntEnumAttribute(transmission_mode, TransmissionModeNames, element, "transmission_mode", true) &&
        xml.getBoolAttribute(other_frequency, element, "other_frequency", true);
    if (_is_valid) {
        centre_frequency = uint32_t(frequency / 10);
    }
}
