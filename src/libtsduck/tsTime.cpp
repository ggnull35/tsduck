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

#include "tsTime.h"
#include "tsFormat.h"
#include "tsMemoryUtils.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Epochs
//----------------------------------------------------------------------------

// These constants represent the time epoch and the end of times.
const ts::Time ts::Time::Epoch(0);
const ts::Time ts::Time::Apocalypse(TS_CONST64(0x7FFFFFFFFFFFFFFF));

// This constant is: Julian epoch - Time epoch.
// The Julian epoch is 17 Nov 1858 00:00:00.
// If negative, the Julian epoch cannot be represented as a Time.
const ts::MilliSecond ts::Time::JulianEpochOffset =
#if defined (__windows)
    // Windows epoch is 1 Jan 1601 00:00:00, 94187 days before Julian epoch
    94187 * MilliSecPerDay;
#elif defined (__vms)
    // VMS time uses Julian date as native time
    0;
#else
    // UNIX epoch is 1 Jan 1970 00:00:00, 40587 days after Julian epoch
    -40587 * MilliSecPerDay;
#endif


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::Time::Time (int year, int month, int day, int hour, int minute, int second, int millisecond) :
    _value(ToInt64(year, month, day, hour, minute, second, millisecond))
{
}


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::Time::Time (const ts::Time::Fields& f) :
    _value(ToInt64(f.year, f.month, f.day, f.hour, f.minute, f.second, f.millisecond))
{
}


//----------------------------------------------------------------------------
// Fields constructor
//----------------------------------------------------------------------------

ts::Time::Fields::Fields (int year_, int month_, int day_, int hour_, int minute_, int second_, int millisecond_) :
    year (year_),
    month (month_),
    day (day_),
    hour (hour_),
    minute (minute_),
    second (second_),
    millisecond (millisecond_)
{
}


//----------------------------------------------------------------------------
// Fields comparison
//----------------------------------------------------------------------------

bool ts::Time::Fields::operator== (const Fields& f) const
{
    return year == f.year && month == f.month && day == f.day &&
        hour == f.hour && minute == f.minute && second == f.second &&
        millisecond == f.millisecond;
}

bool ts::Time::Fields::operator!= (const Fields& f) const
{
    return year != f.year || month != f.month || day != f.day ||
        hour != f.hour || minute != f.minute || second != f.second ||
        millisecond != f.millisecond;
}


//----------------------------------------------------------------------------
// Basic string representation
//----------------------------------------------------------------------------

std::string ts::Time::format(int fields) const
{
    std::string s;
    s.reserve(25); // to avoid reallocs
    Fields f(*this);

    if ((fields & YEAR) != 0) {
        s.append(Format("%4d", f.year));
    }
    if ((fields & MONTH) != 0) {
        if ((fields & YEAR) != 0) {
            s.push_back('/');
        }
        s.append(Format("%02d", f.month));
    }
    if ((fields & DAY) != 0) {
        if ((fields & (YEAR | MONTH)) != 0) {
            s.push_back('/');
        }
        s.append(Format("%02d", f.day));
    }
    if ((fields & (YEAR | MONTH | DAY)) != 0 && (fields & (HOUR | MINUTE | SECOND | MILLISECOND)) != 0) {
        s.push_back(' ');
    }
    if ((fields & HOUR) != 0) {
        s.append(Format("%02d", f.hour));
    }
    if ((fields & MINUTE) != 0) {
        if ((fields & HOUR) != 0) {
            s.push_back(':');
        }
        s.append(Format("%02d", f.minute));
    }
    if ((fields & SECOND) != 0) {
        if ((fields & (HOUR | MINUTE)) != 0) {
            s.push_back(':');
        }
        s.append(Format("%02d", f.second));
    }
    if ((fields & MILLISECOND) != 0) {
        if ((fields & (HOUR | MINUTE | SECOND)) != 0) {
            s.push_back('.');
        }
        s.append(Format("%03d", f.millisecond));
    }
    return s;
}


//----------------------------------------------------------------------------
// Convert a local time to an UTC time
//----------------------------------------------------------------------------

ts::Time ts::Time::localToUTC() const
{
#if defined(__windows)

    FileTime local, utc;
    local.i = _value;
    if (::LocalFileTimeToFileTime(&local.ft, &utc.ft) == 0) {
        throw TimeError(::GetLastError());
    }
    return Time(utc.i);

#else

    time_t seconds = _value / (1000 * TICKS_PER_MS);
    ::tm stime;
    TS_ZERO(stime);
    if (::localtime_r(&seconds, &stime) == 0) {
        throw TimeError("gmtime_r error");
    }

#if defined(__sun)
    int gmt_offset = ::gmtoffset(stime.tm_isdst);
#elif defined(__hpux) || defined(_AIX)
    int gmt_offset = ::gmtoffset(seconds);
#else
    int gmt_offset = stime.tm_gmtoff;
#endif

    return Time(_value - int64_t(gmt_offset) * 1000 * TICKS_PER_MS);

#endif
}

//----------------------------------------------------------------------------
// Convert an UTC time to a local time
//----------------------------------------------------------------------------

ts::Time ts::Time::UTCToLocal() const
{
#if defined(__windows)

    FileTime local, utc;
    utc.i = _value;
    if (::FileTimeToLocalFileTime(&utc.ft, &local.ft) == 0) {
        throw TimeError(::GetLastError());
    }
    return Time(local.i);

#else

    time_t seconds = _value / (1000 * TICKS_PER_MS);
    ::tm stime;
    TS_ZERO(stime);
    if (::localtime_r(&seconds, &stime) == 0) {
        throw TimeError("localtime_r error");
    }

#if defined(__sun)
    int gmt_offset = ::gmtoffset(stime.tm_isdst);
#elif defined(__hpux) || defined(_AIX)
    int gmt_offset = ::gmtoffset(seconds);
#else
    int gmt_offset = stime.tm_gmtoff;
#endif

    return Time(_value + int64_t(gmt_offset) * 1000 * TICKS_PER_MS);

#endif
}


//----------------------------------------------------------------------------
// This static routine returns the current UTC time.
//----------------------------------------------------------------------------

ts::Time ts::Time::CurrentUTC()
{
#if defined(__windows)

    FileTime result;
    ::GetSystemTimeAsFileTime(&result.ft);
    return Time(result.i);

#else

    ::timeval result;
    if (::gettimeofday(&result, NULL) < 0) {
        throw TimeError("gettimeofday error", errno);
    }
    return Time(int64_t(result.tv_usec) + 1000000 * int64_t(result.tv_sec));

#endif
}


//----------------------------------------------------------------------------
// This static routine converts a Win32 FILETIME to MilliSecond
//----------------------------------------------------------------------------

#if defined(__windows)
ts::MilliSecond ts::Time::Win32FileTimeToMilliSecond(const ::FILETIME& ft)
{
    FileTime ftime;
    ftime.ft = ft;
    return ftime.i / TICKS_PER_MS;
}
#endif


//----------------------------------------------------------------------------
// This static routine converts a Win32 FILETIME to a UTC time
//----------------------------------------------------------------------------

#if defined(__windows)
ts::Time ts::Time::Win32FileTimeToUTC(const ::FILETIME& ft)
{
    FileTime ftime;
    ftime.ft = ft;
    return Time(ftime.i);
}
#endif


//----------------------------------------------------------------------------
// This static routine converts a UNIX time_t to a UTC time
//----------------------------------------------------------------------------

ts::Time ts::Time::UnixTimeToUTC(const uint32_t t)
{
#if defined(__unix)
    // On UNIX, use native features
    return Time(int64_t(t) * 1000 * TICKS_PER_MS);
#else
    // On non-UNIX systems, use manual calculation.
    // The value t is a number of seconds since Jan 1st 1970.
    return Time(1970, 1, 1, 0, 0, 0) + (1000 * MilliSecond(t));
#endif
}


//----------------------------------------------------------------------------
// These static routines get the current real time clock and add a delay.
//----------------------------------------------------------------------------

#if defined(__unix)

ts::NanoSecond ts::Time::UnixClockNanoSeconds(clockid_t clock, const MilliSecond& delay)
{
    // Get current time using the specified clock.
    // Minimum resolution is a nanosecond, but much more in fact.
    ::timespec result;
    if (::clock_gettime(clock, &result) != 0) {
        throw TimeError("clock_gettime error", errno);
    }

    // Current time in nano-seconds:
    const NanoSecond nanoseconds = NanoSecond(result.tv_nsec) + NanoSecond(result.tv_sec) * NanoSecPerSec;

    // Delay in nano-seconds:
    const NanoSecond nsDelay = (delay < Infinite / NanoSecPerMilliSec) ? delay * NanoSecPerMilliSec : Infinite;

    // Current time + delay in nano-seconds:
    return (nanoseconds < Infinite - nsDelay) ? nanoseconds + nsDelay : Infinite;
}

void ts::Time::GetUnixClock(::timespec& result, clockid_t clock, const MilliSecond& delay)
{
    const NanoSecond nanoseconds = UnixClockNanoSeconds(clock, delay);
    result.tv_nsec = long(nanoseconds % NanoSecPerSec);
    result.tv_sec = time_t(nanoseconds / NanoSecPerSec);
}

#endif


//----------------------------------------------------------------------------
// Static private routine: Convert 7 fields to a 64-bit time value.
//----------------------------------------------------------------------------

int64_t ts::Time::ToInt64(int year, int month, int day, int hour, int minute, int second, int millisecond)
{
#if defined(__windows)

    ::SYSTEMTIME stime;
    FileTime ftime;

    stime.wYear = ::WORD(year);
    stime.wMonth = ::WORD(month);
    stime.wDay = ::WORD(day);
    stime.wHour = ::WORD(hour);
    stime.wMinute = ::WORD(minute);
    stime.wSecond = ::WORD(second);
    stime.wMilliseconds = ::WORD(millisecond);

    if (::SystemTimeToFileTime(&stime, &ftime.ft) == 0) {
        throw TimeError(::GetLastError());
        // return 0; // unreachable code
    }

    return ftime.i;

#else

    // Convert using mktime.
    ::tm stime;
    TS_ZERO(stime);
    stime.tm_year = year - 1900;
    stime.tm_mon = month - 1; // 0..11
    stime.tm_mday = day;
    stime.tm_hour = hour;
    stime.tm_min = minute;
    stime.tm_sec = second;
    stime.tm_isdst = -1; // undefined

    time_t seconds = ::mktime(&stime);

    if (seconds == time_t(-1)) {
        throw TimeError("mktime error");
        return 0;
    }

    // Add the GMT offset since mktime() uses stime as a local time
#if defined(__sun)
    int gmt_offset = ::gmtoffset(stime.tm_isdst);
#elif defined(__hpux) || defined(_AIX)
    int gmt_offset = ::gmtoffset(seconds);
#else
    int gmt_offset = stime.tm_gmtoff;
#endif
    seconds += gmt_offset;

    // Convert to 64-bit time value
    return (int64_t(seconds) * 1000 + int64_t(millisecond)) * TICKS_PER_MS;

#endif
}

//----------------------------------------------------------------------------
// Convert a time into 7 fields
//----------------------------------------------------------------------------

ts::Time::operator Fields() const
{
#if defined (__windows)

    ::SYSTEMTIME st;
    FileTime ft;
    ft.i = _value;
    if (::FileTimeToSystemTime (&ft.ft, &st) == 0) {
        throw TimeError (::GetLastError ());
    }
    return Fields (st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

#else

    time_t seconds (_value / (1000 * TICKS_PER_MS));
    ::tm st;
    if (::gmtime_r (&seconds, &st) == 0) {
        throw TimeError ("gmtime_r error");
    }
    return Fields (st.tm_year + 1900, st.tm_mon + 1, st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, (_value / TICKS_PER_MS) % 1000);

#endif
}


//----------------------------------------------------------------------------
// These methods return the time for the beginning of hour, day, month, year.
//----------------------------------------------------------------------------

ts::Time ts::Time::thisHour() const
{
    ts::Time::Fields f (*this);
    f.minute = f.second = f.millisecond = 0;
    return ts::Time (f);
}

ts::Time ts::Time::thisDay() const
{
    ts::Time::Fields f (*this);
    f.hour = f.minute = f.second = f.millisecond = 0;
    return ts::Time (f);
}

ts::Time ts::Time::thisMonth() const
{
    ts::Time::Fields f (*this);
    f.day = 1;
    f.hour = f.minute = f.second = f.millisecond = 0;
    return ts::Time (f);
}

ts::Time ts::Time::nextMonth() const
{
    ts::Time::Fields f (*this);
    f.day = 1;
    f.hour = f.minute = f.second = f.millisecond = 0;
    if (f.month++ == 12) {
        f.month = 1;
        f.year++;
    }
    return ts::Time (f);
}

ts::Time ts::Time::thisYear() const
{
    ts::Time::Fields f (*this);
    f.month = f.day = 1;
    f.hour = f.minute = f.second = f.millisecond = 0;
    return ts::Time (f);
}

ts::Time ts::Time::nextYear() const
{
    ts::Time::Fields f (*this);
    f.year++;
    f.month = f.day = 1;
    f.hour = f.minute = f.second = f.millisecond = 0;
    return ts::Time (f);
}
