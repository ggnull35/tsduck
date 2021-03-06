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
//  List of MPEG PSI/SI descriptors
//
//----------------------------------------------------------------------------

#include "tsDescriptorList.h"
#include "tsAbstractDescriptor.h"
#include "tsStringUtils.h"
#include "tsPrivateDataSpecifierDescriptor.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Comparison
//----------------------------------------------------------------------------

bool ts::DescriptorList::operator== (const DescriptorList& other) const
{
    if (_list.size() != other._list.size()) {
        return false;
    }
    for (size_t i = 0; i < _list.size(); ++i) {
        const DescriptorPtr& desc1 (_list[i].desc);
        const DescriptorPtr& desc2 (other._list[i].desc);
        if (desc1.isNull() || desc2.isNull() || *desc1 != *desc2) {
            return false;
        }
    }
    return true;
}


//----------------------------------------------------------------------------
// Add one descriptor at end of list
//----------------------------------------------------------------------------

void ts::DescriptorList::add (const DescriptorPtr& desc)
{
    PDS pds;

    // Determine which PDS to associate with the descriptor
    if (desc->tag() == DID_PRIV_DATA_SPECIF) {
        // This descriptor defines a new "private data specifier".
        // The PDS is the only thing in the descriptor payload.
        pds = desc->payloadSize() < 4 ? 0 : GetUInt32 (desc->payload());
    }
    else if (_list.empty()) {
        // First descriptor in the list
        pds = 0;
    }
    else {
        // Use same PDS as previous descriptor
        pds = _list[_list.size()-1].pds;
    }

    // Add the descriptor in the list
    _list.push_back (Element (desc, pds));
}


//----------------------------------------------------------------------------
// Add one descriptor at end of list
//----------------------------------------------------------------------------

void ts::DescriptorList::add(const AbstractDescriptor& desc)
{
    DescriptorPtr pd(new Descriptor);
    CheckNonNull(pd.pointer());
    desc.serialize(*pd);
    if (pd->isValid()) {
        add(pd);
    }
}


//----------------------------------------------------------------------------
// Add descriptors from a memory area
//----------------------------------------------------------------------------

void ts::DescriptorList::add(const void* data, size_t size)
{
    const uint8_t* desc = reinterpret_cast<const uint8_t*>(data);
    size_t length;

    while (size >= 2 && (length = size_t(desc[1]) + 2) <= size) {
        add(DescriptorPtr(new Descriptor(desc, length)));
        desc += length;
        size -= length;
    }
}


//----------------------------------------------------------------------------
// Prepare removal of a private_data_specifier descriptor.
//----------------------------------------------------------------------------

bool ts::DescriptorList::prepareRemovePDS (const ElementVector::iterator& it)
{
    // Eliminate invalid cases
    if (it == _list.end() || it->desc->tag() != DID_PRIV_DATA_SPECIF) {
        return false;
    }

    // Search for private descriptors ahead.
    ElementVector::iterator end;
    for (end = it + 1; end != _list.end(); ++end) {
        DID tag = end->desc->tag();
        if (tag >= 0x80) {
            // This is a private descriptor, the private_data_specifier descriptor
            // is necessary and cannot be removed.
            return false;
        }
        if (tag == DID_PRIV_DATA_SPECIF) {
            // Found another private_data_specifier descriptor with no private
            // descriptor between the two => the first one can be removed.
            break;
        }
    }

    // Update the current PDS after removed private_data_specifier descriptor
    uint32_t previous_pds = it == _list.begin() ? 0 : (it-1)->pds;
    while (--end != it) {
        end->pds = previous_pds;
    }

    return true;
}


//----------------------------------------------------------------------------
// Add a private_data_specifier if necessary at end of list
//----------------------------------------------------------------------------

void ts::DescriptorList::addPrivateDataSpecifier (PDS pds)
{
    if (pds != 0 && (_list.size() == 0 || _list[_list.size() - 1].pds != pds)) {
        add (PrivateDataSpecifierDescriptor (pds));
    }
}


//----------------------------------------------------------------------------
// Remove all private descriptors without preceding
// private_data_specifier_descriptor.
// Return the number of removed descriptors.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::removeInvalidPrivateDescriptors()
{
    size_t count = 0;

    for (size_t n = 0; n < _list.size(); n++) {
        if (_list[n].pds == 0 && !_list[n].desc.isNull() && _list[n].desc->isValid() && _list[n].desc->tag() >= 0x80) {
            _list.erase (_list.begin() + n);
            count++;
        }
    }

    return count;
}


//----------------------------------------------------------------------------
// Remove the descriptor at the specified index in the list.
//----------------------------------------------------------------------------

bool ts::DescriptorList::removeByIndex (size_t index)
{
    // Check index validity
    if (index >= _list.size()) {
        return false;
    }

    // Private_data_specifier descriptor can be removed under certain conditions only
    if (_list[index].desc->tag() == DID_PRIV_DATA_SPECIF && !prepareRemovePDS (_list.begin() + index)) {
        return false;
    }

    // Remove the specified descriptor
    _list.erase (_list.begin() + index);
    return true;
}


//----------------------------------------------------------------------------
// Remove all descriptors with the specified tag.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::removeByTag (DID tag, PDS pds)
{
    const bool check_pds = pds != 0 && tag >= 0x80;
    size_t removed_count = 0;

    for (ElementVector::iterator it = _list.begin(); it != _list.end(); ) {
        const DID itag = it->desc->tag();
        if (itag == tag && (!check_pds || it->pds == pds) && (itag != DID_PRIV_DATA_SPECIF || prepareRemovePDS (it))) {
            it = _list.erase (it);
            ++removed_count;
        }
        else {
            ++it;
        }
    }

    return removed_count;
}


//----------------------------------------------------------------------------
// Total number of bytes that is required to serialize the list of descriptors.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::binarySize() const
{
    size_t size = 0;

    for (int i = 0; i < int (_list.size()); ++i) {
        size += _list[i].desc->size();
    }

    return size;
}


//----------------------------------------------------------------------------
// Serialize the content of the descriptor list.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::serialize(uint8_t*& addr, size_t& size, size_t start) const
{
    size_t i;

    for (i = start; i < _list.size() && _list[i].desc->size() <= size; ++i) {
        // Flawfinder: ignore: memcpy()
        ::memcpy(addr, _list[i].desc->content(), _list[i].desc->size());
        addr += _list[i].desc->size();
        size -= _list[i].desc->size();
    }

    return i;
}


//----------------------------------------------------------------------------
// Same as Serialize, but prepend a 2-byte length field
// before the descriptor list. The 2-byte length field
// has 4 reserved bits (all '1') and 12 bits for the length
// of the descriptor list.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::lengthSerialize (uint8_t*& addr, size_t& size, size_t start) const
{
    assert (size >= 2);

    // Reserve space for descriptor list length
    uint8_t* length_addr (addr);
    addr += 2;
    size -= 2;

    // Insert descriptor list
    size_t result (serialize (addr, size, start));

    // Update length
    size_t length (addr - length_addr - 2);
    PutUInt16 (length_addr, uint16_t (length | 0xF000));

    return result;
}


//----------------------------------------------------------------------------
// Search a descriptor with the specified tag, starting at the
// specified index.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::search (DID tag, size_t start_index, PDS pds) const
{
    bool check_pds = pds != 0 && tag >= 0x80;
    size_t index = start_index;

    while (index < _list.size() && (_list[index].desc->tag() != tag || (check_pds && _list[index].pds != pds))) {
        index++;
    }

    return index;
}


//----------------------------------------------------------------------------
// Search a language descriptor for the specified language, starting at
// the specified index. Return the index of the descriptor in the list
// or count() if no such descriptor is found.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::searchLanguage (const std::string& language, size_t start_index) const
{
    for (size_t index = start_index; index < _list.size(); index++) {
        if (_list[index].desc->tag() == DID_LANGUAGE) {
            // Got a language descriptor
            const uint8_t* desc = _list[index].desc->payload();
            size_t size = _list[index].desc->payloadSize();
            // The language code uses 3 bytes after the size
            if (size >= 3 && SimilarStrings (language, desc, 3)) {
                return index;
            }
        }
    }

    return count(); // not found
}


//----------------------------------------------------------------------------
// Search any kind of subtitle descriptor, starting at the specified
// index. Return the index of the descriptor in the list.
// Return count() if no such descriptor is found.
//
// If the specified language is non-empty, look only for a subtitle
// descriptor matching the specified language. In this case, if some
// kind of subtitle descriptor exists in the list but none matches the
// language, return count()+1.
//----------------------------------------------------------------------------

size_t ts::DescriptorList::searchSubtitle (const std::string& language, size_t start_index) const
{
    // Value to return if not found
    size_t not_found = count();

    for (size_t index = start_index; index < _list.size(); index++) {

        const DID tag =_list[index].desc->tag();
        const uint8_t* desc = _list[index].desc->payload();
        size_t size = _list[index].desc->payloadSize();

        if (tag == DID_SUBTITLING) {
            // DVB Subtitling Descriptor, always contain subtitles
            if (language.empty()) {
                return index;
            }
            else {
                not_found = count() + 1;
                while (size >= 8) {
                    if (SimilarStrings (language, desc, 3)) {
                        return index;
                    }
                    desc += 8;
                    size -= 8;
                }
            }
        }
        else if (tag == DID_TELETEXT) {
            // DVB Teletext Descriptor, may contain subtitles
            while (size >= 5) {
                // Get teletext type:
                //   0x02: Teletext subtitles
                //   0x05: Teletext subtitles for hearing impaired
                uint8_t tel_type = desc[3] >> 3;
                if (tel_type == 0x02 || tel_type == 0x05) {
                    // This is a teletext containing subtitles
                    if (language.empty()) {
                        return index;
                    }
                    else {
                        not_found = count() + 1;
                        if (SimilarStrings (language, desc, 3)) {
                            return index;
                        }
                    }
                }
                desc += 5;
                size -= 5;
            }
        }
    }

    return not_found;
}
