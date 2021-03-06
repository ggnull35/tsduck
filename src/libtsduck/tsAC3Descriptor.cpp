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
//  Representation of an AC-3_descriptor
//
//----------------------------------------------------------------------------

#include "tsAC3Descriptor.h"
#include "tsFormat.h"
#include "tsHexa.h"
#include "tsNames.h"
#include "tsTablesDisplay.h"
#include "tsTablesFactory.h"
TSDUCK_SOURCE;
TS_ID_DESCRIPTOR_FACTORY(ts::AC3Descriptor, ts::EDID(ts::DID_AC3));
TS_XML_DESCRIPTOR_FACTORY(ts::AC3Descriptor, "AC3_descriptor");
TS_ID_DESCRIPTOR_DISPLAY(ts::AC3Descriptor::DisplayDescriptor, ts::EDID(ts::DID_AC3));


//----------------------------------------------------------------------------
// Default constructor:
//----------------------------------------------------------------------------

ts::AC3Descriptor::AC3Descriptor() :
    AbstractDescriptor(DID_AC3, "AC3_descriptor"),
    component_type(),
    bsid(),
    mainid(),
    asvc(),
    additional_info()
{
    _is_valid = true;
}


//----------------------------------------------------------------------------
// Constructor from a binary descriptor
//----------------------------------------------------------------------------

ts::AC3Descriptor::AC3Descriptor(const Descriptor& desc, const DVBCharset* charset) :
    AbstractDescriptor(DID_AC3, "AC3_descriptor"),
    component_type(),
    bsid(),
    mainid(),
    asvc(),
    additional_info()
{
    deserialize(desc, charset);
}


//----------------------------------------------------------------------------
// Merge inside this object missing information which can be found in other object
//----------------------------------------------------------------------------

void ts::AC3Descriptor::merge(const AC3Descriptor& other)
{
    if (!component_type.set()) {
        component_type = other.component_type;
    }
    if (!bsid.set()) {
        bsid = other.bsid;
    }
    if (!mainid.set()) {
        mainid = other.mainid;
    }
    if (!asvc.set()) {
        asvc = other.asvc;
    }
    if (additional_info.empty()) {
        additional_info = other.additional_info;
    }
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::AC3Descriptor::serialize (Descriptor& desc, const DVBCharset* charset) const
{
    ByteBlockPtr bbp (new ByteBlock (2));
    CheckNonNull (bbp.pointer());

    bbp->appendUInt8 ((component_type.set() ? 0x80 : 0x00) |
                      (bsid.set()           ? 0x40 : 0x00) |
                      (mainid.set()         ? 0x20 : 0x00) |
                      (asvc.set()           ? 0x10 : 0x00));
    if (component_type.set()) {
        bbp->appendUInt8 (component_type.value());
    }
    if (bsid.set()) {
        bbp->appendUInt8 (bsid.value());
    }
    if (mainid.set()) {
        bbp->appendUInt8 (mainid.value());
    }
    if (asvc.set()) {
        bbp->appendUInt8 (asvc.value());
    }
    bbp->append (additional_info);

    (*bbp)[0] = _tag;
    (*bbp)[1] = uint8_t(bbp->size() - 2);
    Descriptor d (bbp, SHARE);
    desc = d;
}


//----------------------------------------------------------------------------
// Deserialization
//----------------------------------------------------------------------------

void ts::AC3Descriptor::deserialize(const Descriptor& desc, const DVBCharset* charset)
{
    _is_valid = desc.isValid() && desc.tag() == _tag && desc.payloadSize() >= 1;

    component_type.reset();
    bsid.reset();
    mainid.reset();
    asvc.reset();
    additional_info.clear();

    if (_is_valid) {
        const uint8_t* data = desc.payload();
        size_t size = desc.payloadSize();
        const uint8_t flags = *data;
        data++; size--;
        if ((flags & 0x80) != 0 && size >= 1) {
            component_type = *data;
            data++; size--;
        }
        if ((flags & 0x40) != 0 && size >= 1) {
            bsid = *data;
            data++; size--;
        }
        if ((flags & 0x20) != 0 && size >= 1) {
            mainid = *data;
            data++; size--;
        }
        if ((flags & 0x10) != 0 && size >= 1) {
            asvc = *data;
            data++; size--;
        }
        additional_info.copy (data, size);
    }
}


//----------------------------------------------------------------------------
// Static method to display a descriptor.
//----------------------------------------------------------------------------

void ts::AC3Descriptor::DisplayDescriptor(TablesDisplay& display, DID did, const uint8_t* data, size_t size, int indent, TID tid, PDS pds)
{
    std::ostream& strm(display.out());
    const std::string margin(indent, ' ');

    if (size >= 1) {
        uint8_t flags = data[0];
        data++; size--;
        if ((flags & 0x80) && size >= 1) { // component_type
            uint8_t type = data[0];
            data++; size--;
            strm << margin << "Component type: " << names::AC3ComponentType(type, names::FIRST) << std::endl;
        }
        if ((flags & 0x40) && size >= 1) { // bsid
            uint8_t bsid = data[0];
            data++; size--;
            strm << margin << Format("AC-3 coding version: %d (0x%02X)", int(bsid), int(bsid)) << std::endl;
        }
        if ((flags & 0x20) && size >= 1) { // mainid
            uint8_t mainid = data[0];
            data++; size--;
            strm << margin << Format("Main audio service id: %d (0x%02X)", int(mainid), int(mainid)) << std::endl;
        }
        if ((flags & 0x10) && size >= 1) { // asvc
            uint8_t asvc = data[0];
            data++; size--;
            strm << margin << Format("Associated to: 0x%02X", int(asvc)) << std::endl;
        }
        if (size > 0) {
            strm << margin << "Additional information:" << std::endl
                 << Hexa(data, size, hexa::HEXA | hexa::ASCII | hexa::OFFSET, indent);
            data += size; size = 0;
        }
    }

    display.displayExtraData(data, size, indent);
}


//----------------------------------------------------------------------------
// XML serialization
//----------------------------------------------------------------------------

ts::XML::Element* ts::AC3Descriptor::toXML(XML& xml, XML::Element* parent) const
{
    XML::Element* root = _is_valid ? xml.addElement(parent, _xml_name) : 0;
    xml.setOptionalIntAttribute(root, "component_type", component_type, true);
    xml.setOptionalIntAttribute(root, "bsid", bsid, true);
    xml.setOptionalIntAttribute(root, "mainid", mainid, true);
    xml.setOptionalIntAttribute(root, "asvc", asvc, true);
    if (!additional_info.empty()) {
        xml.addHexaText(xml.addElement(root, "additional_info"), additional_info);
    }
    return root;
}


//----------------------------------------------------------------------------
// XML deserialization
//----------------------------------------------------------------------------

void ts::AC3Descriptor::fromXML(XML& xml, const XML::Element* element)
{
    _is_valid =
        checkXMLName(xml, element) &&
        xml.getOptionalIntAttribute(component_type, element, "component_type") &&
        xml.getOptionalIntAttribute(bsid, element, "bsid") &&
        xml.getOptionalIntAttribute(mainid, element, "mainid") &&
        xml.getOptionalIntAttribute(asvc, element, "asvc") &&
        xml.getHexaTextChild(additional_info, element, "additional_info", false, 0, MAX_DESCRIPTOR_SIZE - 8);
}
