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
//!
//!  @file
//!  Representation of a message_descriptor
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsAbstractDescriptor.h"

namespace ts {
    //!
    //! Representation of a message_descriptor.
    //! @see ETSI 300 468, 6.4.7.
    //!
    class TSDUCKDLL MessageDescriptor : public AbstractDescriptor
    {
    public:
        // MessageDescriptor public members:
        uint8_t message_id;     //!< Message identifier.
        UString language_code;  //!< ISO-639 language code, 3 characters.
        UString message;        //!< Message content.

        //!
        //! Default constructor.
        //!
        MessageDescriptor();

        //!
        //! Constructor.
        //! @param [in] id Message id.
        //! @param [in] lang ISO-639 language code, 3 characters.
        //! @param [in] text Message content.
        //!
        MessageDescriptor(uint8_t id, const UString& lang, const UString& text);

        //!
        //! Constructor from a binary descriptor.
        //! @param [in] bin A binary descriptor to deserialize.
        //! @param [in] charset If not zero, character set to use without explicit table code.
        //!
        MessageDescriptor(const Descriptor& bin, const DVBCharset* charset = 0);

        // Inherited methods
        virtual void serialize(Descriptor&, const DVBCharset* = 0) const override;
        virtual void deserialize(const Descriptor&, const DVBCharset* = 0) override;
        virtual XML::Element* toXML(XML&, XML::Element*) const override;
        virtual void fromXML(XML&, const XML::Element*) override;

        //!
        //! Static method to display a descriptor.
        //! @param [in,out] display Display engine.
        //! @param [in] did Descriptor id.
        //! @param [in] payload Address of the descriptor payload.
        //! @param [in] size Size in bytes of the descriptor payload.
        //! @param [in] indent Indentation width.
        //! @param [in] tid Table id of table containing the descriptors.
        //! @param [in] pds Private Data Specifier. Used to interpret private descriptors.
        //!
        static void DisplayDescriptor(TablesDisplay& display, DID did, const uint8_t* payload, size_t size, int indent, TID tid, PDS pds);
    };
}
