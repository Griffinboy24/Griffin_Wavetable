/******************************************************************************
    rspl_stopwatch.h - Header-only conversion of the StopWatch module.
    
    This file combines:
      - StopWatch.h     [&#8203;:contentReference[oaicite:0]{index=0}]
      - StopWatch.hpp   [&#8203;:contentReference[oaicite:1]{index=1}]
      - StopWatch.cpp   [&#8203;:contentReference[oaicite:2]{index=2}]
    
    This instrumentation class provides high-accuracy time interval measurements.
    On Mac OS it uses mach_absolute_time() (and Gestalt for clock speed),
    on other systems it uses CPU cycle counters (via __rdtsc() on MSC or
    __builtin_ia32_rdtsc() on GCC/Clang).
    
    Copyright (c) 2003 Laurent de Soras
    Distributed under the GNU Lesser General Public License, version 2.1 or later.
*******************************************************************************/

#ifndef RSPL_STOPWATCH_H
#define RSPL_STOPWATCH_H

#include <cassert>

#if defined(__MACOS__)
    #include <mach/mach_time.h>
    #include <Gestalt.h>
    #include "def.h"
#endif

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    // __builtin_ia32_rdtsc is available in these compilers.
#endif

namespace rspl {

class StopWatch
{
public:
    StopWatch();

    // Start measurement.
    inline void start();
    // Stop measurement.
    inline void stop();

    // Returns the elapsed clock cycles (or equivalent time units).
    inline Int64 get_clk() const;
    // Returns the average clock per operation: get_clk() divided by (div_1 * div_2).
    inline double get_clk_per_op(long div_1, long div_2 = 1) const;

private:
    Int64 _start_time;
    Int64 _stop_time;
#if defined(__MACOS__)
    int _clk_mul;  // Multiplier computed during construction
#endif

    // Forbidden copy operations
    StopWatch (const StopWatch &other);
    StopWatch & operator = (const StopWatch &other);
    bool operator == (const StopWatch &other);
    bool operator != (const StopWatch &other);
};

//---------------------------------------------------------------------------
// Inline Definitions
//---------------------------------------------------------------------------

inline StopWatch::StopWatch()
: _start_time(0)
, _stop_time(0)
{
#if defined(__MACOS__)
    // Obtain clock speed using Gestalt
    long clk_speed;
    ::OSErr err = ::Gestalt(gestaltProcClkSpeed, &clk_speed);
    (void)err; // ignore error for now

    const long nbr_loops = 100 * 1000L;
    int a = 0;
    
    // Get starting time from UpTime (converted to nanoseconds)
    const Nanoseconds nano_seconds_1 = AbsoluteToNanoseconds(UpTime());
    const double start_time_s =
        nano_seconds_1.hi * 4294967296e-9 + nano_seconds_1.lo * 1e-9;
    start();
    for (long i = 0; i < nbr_loops; ++i)
    {
        a = a % 56 + 34;
    }
    stop();
    const Nanoseconds nano_seconds_2 = AbsoluteToNanoseconds(UpTime());
    const double stop_time_s =
        nano_seconds_2.hi * 4294967296e-9 + nano_seconds_2.lo * 1e-9;
    const double diff_time_s = stop_time_s - start_time_s;
    const double nbr_cycles = diff_time_s * clk_speed;
    const Int64 diff_time = _stop_time - _start_time;
    const double clk_mul = nbr_cycles / diff_time;
    // round_int() is assumed to be available (e.g. from fnc.hpp)
    _clk_mul = round_int(clk_mul + 1e-300 * a);
    _start_time = 0;
    _stop_time  = 0;
#endif
}

#if defined(__MACOS__)
inline void StopWatch::start()
{
    _start_time = mach_absolute_time();
}

inline void StopWatch::stop()
{
    _stop_time = mach_absolute_time();
}
#else
    #if defined(_MSC_VER)
inline void StopWatch::start()
{
    _start_time = __rdtsc();
}

inline void StopWatch::stop()
{
    _stop_time = __rdtsc();
}
    #elif defined(__GNUC__) || defined(__clang__)
inline void StopWatch::start()
{
    _start_time = __builtin_ia32_rdtsc();
}

inline void StopWatch::stop()
{
    _stop_time = __builtin_ia32_rdtsc();
}
    #else
inline void StopWatch::start()
{
    _start_time = 0;
}

inline void StopWatch::stop()
{
    _stop_time = 0;
}
    #endif
#endif

inline Int64 StopWatch::get_clk() const
{
#if defined(__MACOS__)
    return ((_stop_time - _start_time) * _clk_mul);
#else
    return (_stop_time - _start_time);
#endif
}

inline double StopWatch::get_clk_per_op(long div_1, long div_2) const
{
    assert(div_1 > 0);
    assert(div_2 > 0);
    const double nbr_clocks = static_cast<double>(get_clk());
    const double glob_div = static_cast<double>(div_1) * static_cast<double>(div_2);
    return (nbr_clocks / glob_div);
}

} // namespace rspl

#endif // RSPL_STOPWATCH_H
