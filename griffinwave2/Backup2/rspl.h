/******************************************************************************
    rspl.h - Header only version of the rspl library.
    
    This file combines:
      - def.h
      - Int16.h
      - Int32.h
      - Int64.h
      - UInt32.h
      - fnc.h and fnc.hpp
      - Fixed3232.h

    Copyright (c) 2003 Laurent de Soras
    Distributed under the GNU Lesser General Public License, version 2.1 or later.
*******************************************************************************/

#ifndef RSPL_H
#define RSPL_H

#include <climits>
#include <cassert>
#include <cmath>

namespace rspl
{

//--------------------------------------------------------------------------
// def.h content
//--------------------------------------------------------------------------
const double    PI              = 3.1415926535897932384626433832795;
const double    LN2             = 0.69314718055994530941723212145818;
const float     ANTI_DENORMAL_FLT = 1e-20f;

#if defined (_MSC_VER)
    #define rspl_FORCEINLINE __forceinline
#else
    #define rspl_FORCEINLINE inline
#endif


//--------------------------------------------------------------------------
// Int16.h content
//--------------------------------------------------------------------------
#if defined (_MSC_VER)
    typedef __int16           Int16;
#elif (defined (__MWERKS__) || defined (__GNUC__) || defined (__BEOS__)) && SHRT_MAX == 0x7FFF
    typedef short int         Int16;
#else
    #error No signed 16-bit integer type defined for this compiler !
#endif


//--------------------------------------------------------------------------
// Int32.h content
//--------------------------------------------------------------------------
#if defined (_MSC_VER)
    typedef __int32           Int32;
#elif (defined (__MWERKS__) || defined (__GNUC__) || defined (__BEOS__)) && INT_MAX == 0x7FFFFFFFL
    typedef int               Int32;
#else
    #error No signed 32-bit integer type defined for this compiler !
#endif


//--------------------------------------------------------------------------
// Int64.h content
//--------------------------------------------------------------------------
#if defined (_MSC_VER)
    typedef __int64           Int64;
#elif defined (__MWERKS__) || defined (__GNUC__)
    typedef long long         Int64;
#elif defined (__BEOS__)
    typedef int64             Int64;
#else
    #error No 64-bit integer type defined for this compiler !
#endif


//--------------------------------------------------------------------------
// UInt32.h content
//--------------------------------------------------------------------------
#if defined (_MSC_VER)
    typedef unsigned __int32  UInt32;
#elif (defined (__MWERKS__) || defined (__GNUC__) || defined (__BEOS__)) && UINT_MAX == 0xFFFFFFFFUL
    typedef unsigned int      UInt32;
#else
    #error No unsigned 32-bit integer type defined for this compiler !
#endif


//--------------------------------------------------------------------------
// fnc.hpp content (from fnc.h/fnc.hpp)
//--------------------------------------------------------------------------
template <typename T>
rspl_FORCEINLINE T min (T a, T b)
{
    return ((a < b) ? a : b);
}

template <typename T>
rspl_FORCEINLINE T max (T a, T b)
{
    return ((b < a) ? a : b);
}

inline int round_int (double x)
{
    using namespace std;
    return static_cast<int>(floor(x + 0.5));
}

inline long round_long (double x)
{
    using namespace std;
    return static_cast<long>(floor(x + 0.5));
}

template <typename T>
rspl_FORCEINLINE T shift_bidi (T x, int s)
{
    if (s > 0)
    {
        x <<= s;
    }
    else if (s < 0)
    {
        assert(x >= 0); // >> has an undefined behaviour for x < 0.
        x >>= -s;
    }
    return x;
}


//--------------------------------------------------------------------------
// Fixed3232.h content
//--------------------------------------------------------------------------
union Fixed3232
{
    Int64 _all;

    class
    {
    public:
#if defined (__POWERPC__)    // Big endian
        Int32   _msw;
        UInt32  _lsw;
#else                       // Little endian
        UInt32  _lsw;
        Int32   _msw;
#endif
    } _part;
};

} // namespace rspl

#endif // RSPL_H
