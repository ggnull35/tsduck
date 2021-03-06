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
//  IP v4 address class
//
//----------------------------------------------------------------------------

#include "tsIPAddress.h"
#include "tsToInteger.h"
#include "tsFormat.h"
#include "tsMemoryUtils.h"
TSDUCK_SOURCE;

// Local host address
const ts::IPAddress ts::IPAddress::LocalHost(127, 0, 0, 1);


//----------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------

ts::IPAddress::IPAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) :
    _addr((uint32_t(b1) << 24) | (uint32_t(b2) << 16) | (uint32_t(b3) << 8) | uint32_t(b4))
{
}

ts::IPAddress::IPAddress (const ::sockaddr& s) :
    _addr (AnyAddress)
{
    if (s.sa_family == AF_INET) {
        assert (sizeof(::sockaddr) == sizeof(::sockaddr_in));
        const ::sockaddr_in* sp = reinterpret_cast<const ::sockaddr_in*> (&s);
        _addr = ntohl (sp->sin_addr.s_addr);
    }
}

ts::IPAddress::IPAddress (const ::sockaddr_in& s) :
    _addr (AnyAddress)
{
    if (s.sin_family == AF_INET) {
        _addr = ntohl (s.sin_addr.s_addr);
    }
}


//----------------------------------------------------------------------------
// Copy into socket structures
//----------------------------------------------------------------------------

void ts::IPAddress::copy (::sockaddr& s, uint16_t port) const
{
    TS_ZERO (s);
    assert (sizeof(::sockaddr) == sizeof(::sockaddr_in));
    ::sockaddr_in* sp = reinterpret_cast< ::sockaddr_in*> (&s);
    sp->sin_family = AF_INET;
    sp->sin_addr.s_addr = htonl (_addr);
    sp->sin_port = htons (port);
}

void ts::IPAddress::copy (::sockaddr_in& s, uint16_t port) const
{
    TS_ZERO (s);
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl (_addr);
    s.sin_port = htons (port);
}


//----------------------------------------------------------------------------
// Set address
//----------------------------------------------------------------------------

void ts::IPAddress::setAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    _addr = (uint32_t(b1) << 24) | (uint32_t(b2) << 16) | (uint32_t(b3) << 8) | uint32_t(b4);
}


//----------------------------------------------------------------------------
// Decode a string or hostname which is resolved.
// Return true on success, false on error.
//----------------------------------------------------------------------------

bool ts::IPAddress::resolve (const std::string& name, ReportInterface& report)
{
    _addr = AnyAddress;

    ::addrinfo hints;
    TS_ZERO (hints);
    hints.ai_family = AF_INET;
    ::addrinfo* res = 0;

    const int status = ::getaddrinfo(name.c_str(), (char*)0, &hints, &res);

    if (status != 0) {
#if defined (__windows)
        const SocketErrorCode code = LastSocketErrorCode();
        report.error(name + ": " + SocketErrorCodeMessage(code));
#else
        const ErrorCode code = LastErrorCode();
        std::string errmsg;
        if (status == EAI_SYSTEM) {
            errmsg = ErrorCodeMessage(code);
        }
        else {
            errmsg = gai_strerror(status);
        }
        report.error(name + ": " + errmsg);
    #endif
        return false;
    }

    // Look for an IPv4 address. All returned addresses should be IPv4 since
    // we specfied the family in hints, but check to be sure.

    ::addrinfo* ai = res;
    while (ai != 0 && (ai->ai_family != AF_INET || ai->ai_addr == 0 || ai->ai_addr->sa_family != AF_INET)) {
        ai = ai->ai_next;
    }
    if (ai != 0) {
        assert (sizeof(::sockaddr) == sizeof(::sockaddr_in));
        const ::sockaddr_in* sp = reinterpret_cast<const ::sockaddr_in*> (ai->ai_addr);
        _addr = ntohl (sp->sin_addr.s_addr);
    }
    else {
        report.error ("no IPv4 address found for " + name);
    }
    ::freeaddrinfo (res);
    return ai != 0;
}


//----------------------------------------------------------------------------
// Convert to a string object
//----------------------------------------------------------------------------

ts::IPAddress::operator std::string () const
{
    return Format ("%d.%d.%d.%d",
                   int ((_addr >> 24) & 0xFF),
                   int ((_addr >> 16) & 0xFF),
                   int ((_addr >> 8) & 0xFF),
                   int (_addr & 0xFF));
}
