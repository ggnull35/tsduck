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
//  Smartcard devices control utility
//
//----------------------------------------------------------------------------

#include "tsArgs.h"
#include "tsHexa.h"
#include "tsFormat.h"
#include "tsPCSC.h"
TSDUCK_SOURCE;

using namespace ts;


//----------------------------------------------------------------------------
//  Command line options
//----------------------------------------------------------------------------

struct Options: public Args
{
    Options(int argc, char *argv[]);

    bool        verbose;       // Verbose output
    std::string reader;        // Optional reader name
    ::DWORD     timeout_ms;    // Timeout in milliseconds
    ::DWORD     reset_action;  // Type of reset to apply
};

Options::Options(int argc, char *argv[]) :
    Args("Smartcard Listing Utility.", "[options] [reader-name]"),
    verbose(false),
    reader(),
    timeout_ms(0),
    reset_action(0)
{
    option ("",            0, Args::STRING, 0, 1);
    option ("cold-reset", 'c');
    option ("eject",      'e');
    option ("timeout",    't', Args::UNSIGNED);
    option ("verbose",    'v');
    option ("warm-reset", 'w');

    setHelp ("Parameters:\n"
             "  The optional reader-name parameter indicates the smartcard reader device\n"
             "  name to list or reset. Without any option or parameter, the command lists\n"
             "  all smartcard reader devices in the system.\n"
             "\n"
             "Options:\n"
             "\n"
             "  -c\n"
             "  --cold-reset\n"
             "      Perfom a cold reset on the smartcard.\n"
             "\n"
             "  -e\n"
             "  --eject\n"
             "      Eject the smartcard.\n"
             "\n"
             "  --help\n"
             "      Display this help text.\n"
             "\n"
             "  -t value\n"
             "  --timeout value\n"
             "      Timeout in milliseconds. The default is 1000 ms.\n"
             "\n"
             "  -v\n"
             "  --verbose\n"
             "      Produce verbose output.\n"
             "\n"
             "  --version\n"
             "      Display the version number.\n"
             "\n"
             "  -w\n"
             "  --warm-reset\n"
             "      Perfom a warm reset on the smartcard.\n");

    analyze (argc, argv);

    reader = value ("");
    verbose = present ("verbose");
    timeout_ms = intValue ("timeout", ::DWORD (1000));

    if (present ("eject")) {
        reset_action = SCARD_EJECT_CARD;
    }
    else if (present ("cold-reset")) {
        reset_action = SCARD_UNPOWER_CARD;
    }
    else if (present ("warm-reset")) {
        reset_action = SCARD_RESET_CARD;
    }
    else {
        reset_action = SCARD_LEAVE_CARD;
    }
}


//----------------------------------------------------------------------------
//  Check PC/SC status, display an error message when necessary.
//  Return false on error.
//----------------------------------------------------------------------------

bool Check (::LONG sc_status, Options& opt, const std::string& cause)
{
    if (sc_status == SCARD_S_SUCCESS) {
        return true;
    }
    else {
        opt.error(cause + Format(": PC/SC error 0x%08X: ", int(sc_status)) + pcsc::StrError(sc_status));
        return false;
    }
}


//----------------------------------------------------------------------------
//  Return a comma, except a colon the first time
//----------------------------------------------------------------------------

inline char sep (int& count)
{
    return count++ == 0 ? ':' : ',';
}


//----------------------------------------------------------------------------
//  List one smartcard
//----------------------------------------------------------------------------

void List (Options& opt, const pcsc::ReaderState& st)
{
    std::cout << st.reader;

    if (opt.verbose) {
        int count = 0;
        if (st.event_state & SCARD_STATE_UNAVAILABLE) {
            std::cout << sep (count) << " unavailable state";
        }
        if (st.event_state & SCARD_STATE_EMPTY) {
            std::cout << sep (count) << " empty";
        }
        if (st.event_state & SCARD_STATE_PRESENT) {
            std::cout << sep (count) << " smartcard present";
        }
        if (st.event_state & SCARD_STATE_EXCLUSIVE) {
            std::cout << sep (count) << " exclusive access";
        }
        if (st.event_state & SCARD_STATE_INUSE) {
            std::cout << sep (count) << " in use";
        }
        if (st.event_state & SCARD_STATE_MUTE) {
            std::cout << sep (count) << " mute";
        }
        if (!st.atr.empty()) {
            std::cout << std::endl << "    ATR: " << Hexa (st.atr, hexa::SINGLE_LINE);
        }
    }
    std::cout << std::endl;
}


//----------------------------------------------------------------------------
//  Reset a smartcard
//----------------------------------------------------------------------------

bool Reset (Options& opt, ::SCARDCONTEXT pcsc_context, const std::string& reader)
{
    if (opt.verbose) {
        std::cout << "resetting " << reader << std::endl;
    }

    ::SCARDHANDLE handle;
    ::DWORD protocol;
    ::LONG sc_status = ::SCardConnect (pcsc_context,
                                       reader.c_str(),
                                       SCARD_SHARE_SHARED,
                                       SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1 | SCARD_PROTOCOL_RAW,
                                       &handle,
                                       &protocol);

    if (!Check (sc_status, opt, reader)) {
        return false;
    }

    sc_status = ::SCardDisconnect (handle, opt.reset_action);

    return Check (sc_status, opt, reader);
}


//----------------------------------------------------------------------------
//  Program entry point
//----------------------------------------------------------------------------

int main (int argc, char *argv[])
{
    int status = EXIT_SUCCESS;
    Options opt (argc, argv);

    // Establish communication with PC/SC

    ::SCARDCONTEXT pcsc_context;
    ::LONG sc_status = ::SCardEstablishContext (SCARD_SCOPE_SYSTEM, 0, 0, &pcsc_context);

    if (!Check (sc_status, opt, "SCardEstablishContext")) {
        return EXIT_FAILURE;
    }

    // Get a list of all smartcard readers

    pcsc::ReaderStateVector states;
    sc_status = pcsc::GetStates (pcsc_context, states, opt.timeout_ms);

    if (!Check (sc_status, opt, "get smartcard readers list")) {
        ::SCardReleaseContext (pcsc_context);
        return EXIT_FAILURE;
    }

    // Loop on all smartcard readers

    bool reader_found = false;

    for (pcsc::ReaderStateVector::const_iterator it = states.begin(); it != states.end(); ++it) {
        if (opt.reader.empty() || opt.reader == it->reader) {
            reader_found = true;
            if (opt.reset_action != SCARD_LEAVE_CARD) {
                // Reset the smartcard if one is present
                if ((it->event_state & SCARD_STATE_PRESENT) && !Reset (opt, pcsc_context, it->reader)) {
                    status = EXIT_FAILURE;
                }
            }
            else {
                // Default action: list the smartcard
                List (opt, *it);
            }
        }
    }

    // If one reader was specified on the command line, check that is was found

    if (!opt.reader.empty() && !reader_found) {
        opt.error ("smartcard reader \"" + opt.reader + "\" not found");
        status = EXIT_FAILURE;
    }

    // Release communication with PC/SC

    sc_status = ::SCardReleaseContext (pcsc_context);
    if (!Check (sc_status, opt, "SCardReleaseContext")) {
        status = EXIT_FAILURE;
    }

    return status;
}
