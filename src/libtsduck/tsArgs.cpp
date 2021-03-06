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

#include "tsArgs.h"
#include "tsDecimal.h"
#include "tsFormat.h"
#include "tsSysUtils.h"
#include "tsVersion.h"
TSDUCK_SOURCE;

// Unlimited number of occurences
const size_t ts::Args::UNLIMITED_COUNT = std::numeric_limits<size_t>::max();

// Unlimited value.
const int64_t ts::Args::UNLIMITED_VALUE = std::numeric_limits<int64_t>::max();

// List of characters which are allowed thousands separators in integer values
const char* const ts::Args::THOUSANDS_SEPARATORS = ",. ";


//----------------------------------------------------------------------------
// Constructor for IOption
//----------------------------------------------------------------------------

ts::Args::IOption::IOption(const char* name_,
                           char        short_name_,
                           ArgType     type_,
                           size_t      min_occur_,
                           size_t      max_occur_,
                           int64_t     min_value_,
                           int64_t     max_value_,
                           bool        optional_) :

    name        (name_ == 0 ? "" : name_),
    short_name  (short_name_),
    type        (type_),
    min_occur   (min_occur_),
    max_occur   (max_occur_),
    min_value   (min_value_),
    max_value   (max_value_),
    optional    (optional_),
    predefined  (false),
    enumeration (),
    values      ()
{
    // Provide default max_occur
    if (max_occur == 0) {
        max_occur = name.empty() ? UNLIMITED_COUNT : 1;
    }
    // Handle invalid values
    if (max_occur < min_occur) {
        throw ArgsError ("invalid occurences for " + display());
    }
    // Parameters are values by definition
    if (name.empty() && type == NONE) {
        type = STRING;
    }
    // Normalize all integer types to INTEGER
    switch (type) {
        case NONE:
        case STRING:
            min_value = 0;
            max_value = 0;
            break;
        case INTEGER:
            if (max_value < min_value) {
                throw ArgsError ("invalid value range for " + display());
            }
            break;
        case UNSIGNED:
            min_value = 0;
            max_value = std::numeric_limits<int64_t>::max();
            type = INTEGER;
            break;
        case POSITIVE:
            min_value = 1;
            max_value = std::numeric_limits<int64_t>::max();
            type = INTEGER;
            break;
        case UINT8:
            min_value = 0;
            max_value = 0xFF;
            type = INTEGER;
            break;
        case UINT16:
            min_value = 0;
            max_value = 0xFFFF;
            type = INTEGER;
            break;
        case UINT32:
            min_value = 0;
            max_value = 0xFFFFFFFF;
            type = INTEGER;
            break;
        case PIDVAL:
            min_value = 0;
            max_value = 0x1FFF;
            type = INTEGER;
            break;
        default:
            throw ArgsError(Format("invalid option type %d",int(type)));
    }
}


//----------------------------------------------------------------------------
// Constructor for IOption
//----------------------------------------------------------------------------

ts::Args::IOption::IOption (const char*        name_,
                              char               short_name_,
                              const Enumeration& enumeration_,
                              size_t             min_occur_,
                              size_t             max_occur_,
                              bool               optional_) :

    name        (name_ == 0 ? "" : name_),
    short_name  (short_name_),
    type        (INTEGER),
    min_occur   (min_occur_),
    max_occur   (max_occur_),
    min_value   (std::numeric_limits<int>::min()),
    max_value   (std::numeric_limits<int>::max()),
    optional    (optional_),
    predefined  (false),
    enumeration (enumeration_),
    values      ()
{
    // Provide default max_occur
    if (max_occur == 0) {
        max_occur = name.empty() ? UNLIMITED_COUNT : 1;
    }
    // Handle invalid values
    if (max_occur < min_occur) {
        throw ArgsError ("invalid occurences for " + display());
    }
}


//----------------------------------------------------------------------------
// Displayable name for IOption
//----------------------------------------------------------------------------

std::string ts::Args::IOption::display() const
{
    std::string plural (min_occur > 1 ? "s" : "");
    if (name.empty()) {
        return "parameter" + plural;
    }
    else {
        std::string n;
        if (short_name != 0) {
            n = " (-";
            n += short_name;
            n += ')';
        }
        return "option" + plural + " --" + name + n;
    }
}


//----------------------------------------------------------------------------
// Constructor for Args
//----------------------------------------------------------------------------

ts::Args::Args(const std::string& description, const std::string& syntax, const std::string& help, int flags) :
    _subreport(0),
    _iopts(),
    _description(description),
    _shell(),
    _syntax(syntax),
    _help(help),
    _app_name(""),
    _args(),
    _is_valid(false),
    _flags(flags)
{
    // Add predefined option
    option("help");
    option("version", 0, VersionFormatEnum, 0, 1, true);
    search("help")->predefined = true;
    search("version")->predefined = true;
}


//----------------------------------------------------------------------------
// Add an option definition
//----------------------------------------------------------------------------

ts::Args& ts::Args::option(const char* name,
                           char        short_name,
                           ArgType     type,
                           size_t      min_occur,
                           size_t      max_occur,
                           int64_t     min_value,
                           int64_t     max_value,
                           bool        optional)
{
    IOption opt(name, short_name, type, min_occur, max_occur, min_value, max_value, optional);
    _iopts.erase(opt.name);
    _iopts.insert(std::make_pair(opt.name, opt));
    return *this;
}


//----------------------------------------------------------------------------
// Add an option definition
//----------------------------------------------------------------------------

ts::Args& ts::Args::option (const char*        name,
                                char               short_name,
                                const Enumeration& enumeration,
                                size_t             min_occur,
                                size_t             max_occur,
                                bool               optional)
{
    IOption opt (name, short_name, enumeration, min_occur, max_occur, optional);
    _iopts.erase (opt.name);
    _iopts.insert (std::make_pair (opt.name, opt));
    return *this;
}


//----------------------------------------------------------------------------
// Copy all option definitions from another Args object. Return this object.
// If override is true, override duplicated options.
//----------------------------------------------------------------------------

ts::Args& ts::Args::copyOptions (const Args& other, const bool override)
{
    for (IOptionMap::const_iterator it = other._iopts.begin(); it != other._iopts.end(); ++it) {
        if (override || _iopts.find (it->second.name) == _iopts.end()) {
            _iopts.erase (it->second.name);
            _iopts.insert (std::make_pair (it->second.name, it->second));
        }
    }
    return *this;
}


//----------------------------------------------------------------------------
// Redirect report logging. Redirection cancelled if zero.
//----------------------------------------------------------------------------

void ts::Args::redirectReport (ReportInterface* rep)
{
    _subreport = rep;
    if (rep != 0 && rep->debugLevel() > this->debugLevel()) {
        this->setDebugLevel(rep->debugLevel());
    }
}


//----------------------------------------------------------------------------
// Display an error message, as if it was produced during command line analysis.
// Mark this instance as error if severity <= Severity::Error.
// Immediately abort application is severity == Severity::Fatal.
// Inherited from ReportInterface.
//----------------------------------------------------------------------------

void ts::Args::writeLog(int severity, const std::string& message)
{
    if ((_flags & NO_ERROR_DISPLAY) == 0) {
        if (_subreport != 0) {
            _subreport->log(severity, message);
        }
        else if (severity <= _max_severity) {
            if (severity < Severity::Info) {
                std::cerr << _app_name << ": ";
            }
            std::cerr << message << std::endl;
        }
    }
    _is_valid = _is_valid && severity > Severity::Error;
    if (severity <= Severity::Fatal) {
        ::exit(EXIT_FAILURE);
    }
}


//----------------------------------------------------------------------------
// Exit application when errors were reported.
//----------------------------------------------------------------------------

void ts::Args::exitOnError (bool force)
{
    if (!_is_valid && (force || (_flags & NO_EXIT_ON_ERROR) == 0)) {
        ::exit (EXIT_FAILURE);
    }
}


//----------------------------------------------------------------------------
// Locate an option description. Return 0 if not found
//----------------------------------------------------------------------------

ts::Args::IOption* ts::Args::search (char c)
{
    for (IOptionMap::iterator it = _iopts.begin(); it != _iopts.end(); ++it) {
        if (it->second.short_name == c) {
            return &it->second;
        }
    }
    error (Format ("unknown option -%c", c));
    return 0;
}


//----------------------------------------------------------------------------
// Locate an option description. Return 0 if not found
//----------------------------------------------------------------------------

ts::Args::IOption* ts::Args::search (const std::string& name)
{
    IOption* previous = 0;

    for (IOptionMap::iterator it = _iopts.begin(); it != _iopts.end(); ++it) {
        if (it->second.name == name) {
            // found an exact match
            return &it->second;
        }
        else if (!name.empty() && it->second.name.find (name) == 0) {
            // found an abbreviated version
            if (previous == 0) {
                // remember this one and continue searching
                previous = &it->second;
            }
            else {
                // another one already found, ambiguous option
                error ("ambiguous option --" + name + " (--" + previous->name + ", --" + it->second.name + ")");
                return 0;
            }
        }
    }

    if (previous != 0) {
        // exactly one abbreviation was found
        return previous;
    }
    else if (name.empty()) {
        error ("no parameter allowed, use options only");
        return 0;
    }
    else {
        error ("unknown option --" + name);
        return 0;
    }
}


//----------------------------------------------------------------------------
// Locate an IOption based on its complete long name.
// Throw ArgsError if option does not exist (application internal error)
//----------------------------------------------------------------------------

const ts::Args::IOption& ts::Args::getIOption(const char* name) const
{
    const std::string name1(name == 0 ? "" : name);
    IOptionMap::const_iterator it = _iopts.find(name1);
    if (it != _iopts.end()) {
        return it->second;
    }
    else {
        throw ArgsError(_app_name + ": application internal error, option " + name1 + " undefined");
    }
}


//----------------------------------------------------------------------------
// Check if option is present
//----------------------------------------------------------------------------

bool ts::Args::present (const char* name) const
{
    return !getIOption(name).values.empty();
}


//----------------------------------------------------------------------------
// Check the number of occurences of the option.
//----------------------------------------------------------------------------

size_t ts::Args::count (const char* name) const
{
    return getIOption(name).values.size();
}


//----------------------------------------------------------------------------
// Get the value of an option. The index designates the occurence of
// the option. If the option is not present, or not with this
// occurence, defValue is returned.
//----------------------------------------------------------------------------

std::string ts::Args::value (const char* name, const char* defValue, size_t index) const
{
    const IOption& opt (getIOption (name));
    return index >= opt.values.size() || !opt.values[index].set() ? defValue : opt.values[index].value();
}

void ts::Args::getValue (std::string& value_, const char* name, const char* defValue, size_t index) const
{
    value_ = value (name, defValue, index);
}


//----------------------------------------------------------------------------
// Return all occurences of this option in a vector
//----------------------------------------------------------------------------

void ts::Args::getValues (StringVector& values, const char* name) const
{
    const IOption& opt (getIOption(name));

    values.clear();
    values.reserve (opt.values.size());

    for (ArgValueVector::const_iterator it = opt.values.begin(); it != opt.values.end(); ++it) {
        if (it->set()) {
            values.push_back (it->value());
        }
    }
}


//----------------------------------------------------------------------------
// Get all occurences of this option and interpret them as PID values
//----------------------------------------------------------------------------

void ts::Args::getPIDSet (PIDSet& values, const char* name, bool defValue) const
{
    PID pid;
    const IOption& opt(getIOption(name));

    if (!opt.values.empty()) {
        values.reset();
        for (ArgValueVector::const_iterator it = opt.values.begin(); it != opt.values.end(); ++it) {
            if (it->set() && ToInteger(pid, it->value(), THOUSANDS_SEPARATORS)) {
                values.set(pid);
            }
        }
    }
    else if (defValue) {
        values.set();
    }
    else {
        values.reset();
    }
}


//----------------------------------------------------------------------------
// Load arguments and analyze them.
//----------------------------------------------------------------------------

bool ts::Args::analyze(const std::string& app_name, const StringVector& arguments)
{
    _app_name = app_name;
    _args = arguments;
    return analyze();
}

bool ts::Args::analyze(int argc, char* argv[])
{
    _app_name = argc > 0 ? BaseName(argv[0], TS_EXECUTABLE_SUFFIX) : "";
    if (argc < 2) {
        _args.clear();
    }
    else {
        AssignContainer(_args, argc - 1, argv + 1);
    }
    return analyze();
}

bool ts::Args::analyze(const char* app_name, const char* arg1, ...)
{
    _app_name = app_name;
    _args.clear();
    if (arg1 != 0) {
        _args.push_back (arg1);
        va_list ap;
        va_start(ap, arg1);
        const char* argn;
        while ((argn = va_arg(ap, const char*)) != 0) {
            _args.push_back(argn);
        }
        va_end(ap);
    }
    return analyze();
}


//----------------------------------------------------------------------------
// Common code: analyze the command line.
//----------------------------------------------------------------------------

bool ts::Args::analyze()
{
    // Clear previous values
    for (IOptionMap::iterator it = _iopts.begin(); it != _iopts.end(); ++it) {
        it->second.values.clear();
    }

    // Assume valid command
    _is_valid = true;

    // Process argument list
    const size_t NPOS = std::string::npos;
    size_t next_arg = 0;            // Index of next arg to process
    size_t short_opt_arg = NPOS;    // Index of arg containing short options
    size_t short_opt_index = NPOS;  // Short option index in _args[short_opt_arg]
    bool force_parameters = false;  // Force all items to be parameters

    while (short_opt_arg != NPOS || next_arg < _args.size()) {

        IOption* opt = 0;
        ArgValue val;

        // Locate option name and value

        if (short_opt_arg != NPOS) {
            // Analysing several short options in a string
            opt = search (_args[short_opt_arg][short_opt_index++]);
            if (short_opt_index >= _args[short_opt_arg].length()) {
                // Reached end of short option string
                short_opt_arg = NPOS;
                short_opt_index = NPOS;
            }
        }
        else if (force_parameters ||_args[next_arg].empty() || _args[next_arg][0] != '-') {
            // Arg is a parameter
            if ((opt = search ("")) == 0) {
                ++next_arg;
            }
            force_parameters = (_flags & GATHER_PARAMETERS) != 0;
        }
        else if (_args[next_arg].length() == 1) {
            // Arg is '-', next arg is a parameter, even if it starts with '-'
            ++next_arg;
            if ((opt = search ("")) == 0) {
                ++next_arg;
            }
        }
        else if (_args[next_arg][1] == '-') {
            // Arg starts with '--', this is a long option
            size_t equal = _args[next_arg].find ('=');
            if (equal != NPOS) {
                // Value is in the same arg: --option=value
                opt = search (_args[next_arg].substr (2, equal - 2));
                val = _args[next_arg].substr (equal + 1);
            }
            else {
                // Simple form: --option
                opt = search (_args[next_arg].substr (2));
            }
            ++next_arg;
        }
        else {
            // Arg starts with one single '-'
            opt = search (_args[next_arg][1]);
            if (_args[next_arg].length() > 2) {
                // More short options or value in arg
                short_opt_arg = next_arg;
                short_opt_index = 2;
            }
            ++next_arg;
        }

        // If IOption not found, error already reported
        if (opt == 0) {
            continue;
        }

        // If no value required, simply add an empty string
        if (opt->type == NONE) {
            if (val.set()) {
                // In the case --option=value
                error ("no value allowed for "+ opt->display());
            }
            opt->values.push_back (val);
            continue;
        }

        // Get the value string from short option, if present
        if (short_opt_arg != NPOS) {
            assert (!val.set());
            // Get the value from the rest of the short option string
            val = _args[short_opt_arg].substr (short_opt_index);
            short_opt_arg = NPOS;
            short_opt_index = NPOS;
        }

        // Check presence of mandatory values in next arg if not already found
        if (!val.set() && !opt->optional) {
            if (next_arg >= _args.size()) {
                error ("missing value for " + opt->display());
                continue;
            }
            else {
                val = _args[next_arg++];
            }
        }

        // Validate values
        if (val.set() && opt->type == INTEGER) {
            int64_t ival = 0;
            if (!opt->enumeration.empty()) {
                // Enumeration value expected, get corresponding integer value (not case sensitive)
                int i = opt->enumeration.value (val.value(), false);
                if (i != Enumeration::UNKNOWN) {
                    // Replace with actual integer value
                    val = Format ("%d", i);
                }
                else {
                    error("invalid value " + val.value() + " for " + opt->display() +
                          ", use one of " + opt->enumeration.nameList());
                    continue;
                }
            }
            else if (!ToInteger(ival, val.value(), THOUSANDS_SEPARATORS)) {
                error("invalid integer value " + val.value() + " for " + opt->display());
                continue;
            }
            else if (ival < opt->min_value) {
                error("value for " + opt->display() + " must be >= " + Decimal(opt->min_value));
                continue;
            }
            else if (ival > opt->max_value) {
                error("value for " + opt->display() + " must be <= " + Decimal(opt->max_value));
                continue;
            }
        }

        // Push value. For optional parameters without value, an unset variable is pushed.
        opt->values.push_back (val);
    }

    // Process --help predefined option
    if (present("help") && search("help")->predefined) {
        std::string text("\n" + _description + "\n\n" + "Usage: ");
        if (!_shell.empty()) {
            text += _shell + " ";
        }
        text += _app_name + " " + _syntax + "\n\n" + _help;
        info(text);
        if ((_flags & NO_EXIT_ON_HELP) == 0) {
            ::exit(EXIT_SUCCESS);
        }
        return _is_valid = false;
    }

    // Process --version predefined option
    if (present("version") && search("version")->predefined) {
        info(GetVersion(VersionFormat(intValue<int>("version", VERSION_LONG)), _app_name));
        if ((_flags & NO_EXIT_ON_VERSION) == 0) {
            ::exit(EXIT_SUCCESS);
        }
        return _is_valid = false;
    }

    // Look for parameters/options number of occurences.
    // Don't do that if command already proven wrong
    if (_is_valid) {
        for (IOptionMap::iterator it = _iopts.begin(); it != _iopts.end(); ++it) {
            const IOption& op (it->second);
            if (op.values.size() < op.min_occur) {
                error ("missing " + op.display() + (op.min_occur < 2 ? "" : Format (", %" FMT_SIZE_T "u required", op.min_occur)));
            }
            else if (op.values.size() > op.max_occur) {
                error ("too many " + op.display() + (op.max_occur < 2 ? "" : Format (", %" FMT_SIZE_T "u maximum", op.max_occur)));
            }
        }
    }

    // In case of error, exit
    exitOnError();

    return _is_valid;
}
