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
//  Representation of a supplementary_audio_descriptor
//
//----------------------------------------------------------------------------

#include "tsSupplementaryAudioDescriptor.h"
#include "tsFormat.h"
#include "tsHexa.h"
#include "tsNames.h"
#include "tsTablesDisplay.h"
#include "tsTablesFactory.h"
TSDUCK_SOURCE;
TS_XML_DESCRIPTOR_FACTORY(ts::SupplementaryAudioDescriptor, "supplementary_audio_descriptor");
TS_ID_DESCRIPTOR_FACTORY(ts::SupplementaryAudioDescriptor, ts::EDID(ts::DID_EXTENSION, ts::EDID_SUPPL_AUDIO));
TS_ID_DESCRIPTOR_DISPLAY(ts::SupplementaryAudioDescriptor::DisplayDescriptor, ts::EDID(ts::DID_EXTENSION, ts::EDID_SUPPL_AUDIO));


//----------------------------------------------------------------------------
// Constructor.
//----------------------------------------------------------------------------

ts::SupplementaryAudioDescriptor::SupplementaryAudioDescriptor() :
    AbstractDescriptor(DID_EXTENSION, "supplementary_audio_descriptor"),
    mix_type(0),
    editorial_classification(0),
    language_code(),
    private_data()
{
    _is_valid = true;
}

ts::SupplementaryAudioDescriptor::SupplementaryAudioDescriptor(const Descriptor& bin, const DVBCharset* charset) :
    AbstractDescriptor(DID_EXTENSION, "supplementary_audio_descriptor"),
    mix_type(0),
    editorial_classification(0),
    language_code(),
    private_data()
{
    deserialize(bin, charset);
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::SupplementaryAudioDescriptor::serialize(Descriptor& desc, const DVBCharset* charset) const
{
    ByteBlockPtr bbp(serializeStart());

    bbp->appendUInt8(EDID_SUPPL_AUDIO);
    bbp->appendUInt8((mix_type << 7) |
                     ((editorial_classification & 0x1F) << 2) |
                     0x02 |
                     (language_code.empty() ? 0x00 : 0x01));
    if (!language_code.empty() && !SerializeLanguageCode(*bbp, language_code, charset)) {
        desc.invalidate();
        return;
    }
    bbp->append(private_data);

    serializeEnd(desc, bbp);
}


//----------------------------------------------------------------------------
// Deserialization
//----------------------------------------------------------------------------

void ts::SupplementaryAudioDescriptor::deserialize(const Descriptor& desc, const DVBCharset* charset)
{
    language_code.clear();
    private_data.clear();

    const uint8_t* data = desc.payload();
    size_t size = desc.payloadSize();

    if (!(_is_valid = desc.isValid() && desc.tag() == _tag && size >= 2 && data[0] == EDID_SUPPL_AUDIO)) {
        return;
    }

    mix_type = (data[1] >> 7) & 0x01;
    editorial_classification = (data[1] >> 2) & 0x1F;
    const bool has_lang = (data[1] & 0x01) != 0;
    data += 2; size -= 2;

    if (has_lang) {
        if (size < 3) {
            _is_valid = false;
            return;
        }
        language_code = UString::FromDVB(data, 3, charset);
        data += 3; size -= 3;
    }

    private_data.copy(data, size);
}


//----------------------------------------------------------------------------
// XML serialization
//----------------------------------------------------------------------------

ts::XML::Element* ts::SupplementaryAudioDescriptor::toXML(XML& xml, XML::Element* parent) const
{
    XML::Element* root = _is_valid ? xml.addElement(parent, _xml_name) : 0;
    xml.setIntAttribute(root, "mix_type", mix_type);
    xml.setIntAttribute(root, "editorial_classification", editorial_classification, true);
    if (!language_code.empty()) {
        xml.setAttribute(root, "language_code", language_code);
    }
    if (!private_data.empty()) {
        xml.addHexaText(xml.addElement(root, "private_data"), private_data);
    }
    return root;
}


//----------------------------------------------------------------------------
// XML deserialization
//----------------------------------------------------------------------------

void ts::SupplementaryAudioDescriptor::fromXML(XML& xml, const XML::Element* element)
{
    _is_valid =
        checkXMLName(xml, element) &&
        xml.getIntAttribute<uint8_t>(mix_type, element, "mix_type", true, 0, 0, 1) &&
        xml.getIntAttribute<uint8_t>(editorial_classification, element, "editorial_classification", true, 0, 0x00, 0x1F) &&
        xml.getAttribute(language_code, element, "language_code", false, "", 3, 3) &&
        xml.getHexaTextChild(private_data, element, "private_data", false, 0, MAX_DESCRIPTOR_SIZE - 7);
}


//----------------------------------------------------------------------------
// Static method to display a descriptor.
//----------------------------------------------------------------------------

void ts::SupplementaryAudioDescriptor::DisplayDescriptor(TablesDisplay& display, DID did, const uint8_t* data, size_t size, int indent, TID tid, PDS pds)
{
    // Important: With extension descriptors, the DisplayDescriptor() function is called
    // with extension payload. Meaning that data points after descriptor_tag_extension.
    // See ts::TablesDisplay::displayDescriptorData()

    std::ostream& strm(display.out());
    const std::string margin(indent, ' ');

    if (size >= 1) {
        const uint8_t mix_type = (data[0] >> 7) & 0x01;
        const uint8_t editorial = (data[0] >> 2) & 0x1F;
        const uint8_t lang_present = data[0] & 0x01;
        data++; size--;
        strm << margin << "Mix type: ";
        switch (mix_type) {
            case 0:  strm << "supplementary stream"; break;
            case 1:  strm << "complete and independent stream"; break;
            default: assert(false);
        }
        strm << std::endl << margin << "Editorial classification: ";
        switch (editorial) {
            case 0x00: strm << "main audio"; break;
            case 0x01: strm << "audio description for the visually impaired"; break;
            case 0x02: strm << "clean audio for the hearing impaired"; break;
            case 0x03: strm << "spoken subtitles for the visually impaired"; break;
            default:   strm << Format("reserved value 0x%02X", editorial); break;
        }
        strm << std::endl;
        if (lang_present && size >= 3) {
            strm << margin << "Language: " << UString::FromDVB(data, 3, display.dvbCharset()) << std::endl;
            data += 3; size -= 3;
        }
        if (size > 0) {
            strm << margin << "Private data:" << std::endl
                 << Hexa(data, size, hexa::HEXA | hexa::ASCII | hexa::OFFSET, indent);
            data += size; size = 0;
        }
    }

    display.displayExtraData(data, size, indent);
}
