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
//  Representation of an parental_rating_descriptor
//
//----------------------------------------------------------------------------

#include "tsParentalRatingDescriptor.h"
#include "tsFormat.h"
#include "tsHexa.h"
#include "tsNames.h"
#include "tsBinaryTable.h"
#include "tsTablesDisplay.h"
#include "tsTablesFactory.h"
TSDUCK_SOURCE;
TS_XML_DESCRIPTOR_FACTORY(ts::ParentalRatingDescriptor, "parental_rating_descriptor");
TS_ID_DESCRIPTOR_FACTORY(ts::ParentalRatingDescriptor, ts::EDID(ts::DID_PARENTAL_RATING));
TS_ID_DESCRIPTOR_DISPLAY(ts::ParentalRatingDescriptor::DisplayDescriptor, ts::EDID(ts::DID_PARENTAL_RATING));


//----------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------

ts::ParentalRatingDescriptor::Entry::Entry(const char* code, uint8_t rate) :
    country_code(code),
    rating(rate)
{
}

ts::ParentalRatingDescriptor::Entry::Entry(const UString& code, uint8_t rate) :
    country_code(code),
    rating(rate)
{
}

ts::ParentalRatingDescriptor::ParentalRatingDescriptor() :
    AbstractDescriptor(DID_PARENTAL_RATING, "parental_rating_descriptor"),
    entries()
{
    _is_valid = true;
}

ts::ParentalRatingDescriptor::ParentalRatingDescriptor(const Descriptor& desc, const DVBCharset* charset) :
    AbstractDescriptor(DID_PARENTAL_RATING, "parental_rating_descriptor"),
    entries()
{
    deserialize(desc, charset);
}

ts::ParentalRatingDescriptor::ParentalRatingDescriptor(const UString& code, uint8_t rate) :
    AbstractDescriptor(DID_PARENTAL_RATING, "parental_rating_descriptor"),
    entries()
{
    _is_valid = true;
    entries.push_back(Entry(code, rate));
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::ParentalRatingDescriptor::serialize (Descriptor& desc, const DVBCharset* charset) const
{
    ByteBlockPtr bbp(serializeStart());

    for (EntryList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
        if (!SerializeLanguageCode(*bbp, it->country_code, charset)) {
            desc.invalidate();
            return;
        }
        bbp->appendUInt8(it->rating);
    }

    serializeEnd(desc, bbp);
}


//----------------------------------------------------------------------------
// Deserialization
//----------------------------------------------------------------------------

void ts::ParentalRatingDescriptor::deserialize (const Descriptor& desc, const DVBCharset* charset)
{
    _is_valid = desc.isValid() && desc.tag() == _tag && desc.payloadSize() % 4 == 0;
    entries.clear();

    if (_is_valid) {
        const uint8_t* data = desc.payload();
        size_t size = desc.payloadSize();
        while (size >= 4) {
            entries.push_back(Entry(UString::FromDVB(data, 3, charset), data[3]));
            data += 4;
            size -= 4;
        }
    }
}


//----------------------------------------------------------------------------
// Static method to display a descriptor.
//----------------------------------------------------------------------------

void ts::ParentalRatingDescriptor::DisplayDescriptor(TablesDisplay& display, DID did, const uint8_t* data, size_t size, int indent, TID tid, PDS pds)
{
    std::ostream& strm(display.out());
    const std::string margin(indent, ' ');

    while (size >= 4) {
        const uint8_t rating = data[3];
        strm << margin << "Country code: " << UString::FromDVB(data, 3, display.dvbCharset())
             << Format(", rating: 0x%02X ", int(rating));
        if (rating == 0) {
            strm << "(undefined)";
        }
        else if (rating <= 0x0F) {
            strm << "(min. " << int(rating + 3) << " years)";
        }
        else {
            strm << "(broadcaster-defined)";
        }
        strm << std::endl;
        data += 4; size -= 4;
    }

    display.displayExtraData(data, size, indent);
}


//----------------------------------------------------------------------------
// XML serialization
//----------------------------------------------------------------------------

ts::XML::Element* ts::ParentalRatingDescriptor::toXML(XML& xml, XML::Element* parent) const
{
    XML::Element* root = _is_valid ? xml.addElement(parent, _xml_name) : 0;
    for (EntryList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
        XML::Element* e = xml.addElement(root, "country");
        xml.setAttribute(e, "country_code", it->country_code);
        xml.setIntAttribute(e, "rating", it->rating, true);
    }
    return root;
}


//----------------------------------------------------------------------------
// XML deserialization
//----------------------------------------------------------------------------

void ts::ParentalRatingDescriptor::fromXML(XML& xml, const XML::Element* element)
{
    entries.clear();

    XML::ElementVector children;
    _is_valid =
        checkXMLName(xml, element) &&
        xml.getChildren(children, element, "country", 0, MAX_ENTRIES);

    for (size_t i = 0; _is_valid && i < children.size(); ++i) {
        Entry entry;
        _is_valid =
            xml.getAttribute(entry.country_code, children[i], "country_code", true, "", 3, 3) &&
            xml.getIntAttribute<uint8_t>(entry.rating, children[i], "rating", true, 0, 0x00, 0xFF);
        if (_is_valid) {
            entries.push_back(entry);
        }
    }
}
