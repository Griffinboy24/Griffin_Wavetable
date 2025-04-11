/******************************************************************************
    rspl_basevoicestate.h - Header-only conversion of the BaseVoiceState module.
    
    This file combines:
      - BaseVoiceState.cpp   [&#8203;:contentReference[oaicite:0]{index=0}]
      - BaseVoiceState.h     [&#8203;:contentReference[oaicite:1]{index=1}]

    Copyright (c) 2003 Laurent de Soras
    Distributed under the GNU Lesser General Public License, version 2.1 or later.
*******************************************************************************/

#ifndef RSPL_BASEVOICESTATE_H
#define RSPL_BASEVOICESTATE_H

// The common definitions and types (Fixed3232, def.h, fnc functions, etc)
// are assumed to be provided by rspl.h.
#include "rspl.h"
#include <cassert>
#include <cmath>

namespace rspl
{

class BaseVoiceState
{
public:
    enum { NBR_BITS_PER_OCT = 16 };
    enum { FADE_LEN         = 64 };

    BaseVoiceState();
    BaseVoiceState & operator=(const BaseVoiceState &other);

    void compute_step(long pitch);

    // Public members as per original design:
    Fixed3232     _pos;           // Position in the current MIP-map level
    Fixed3232     _step;          // Step in the current MIP-map level
    const float * _table_ptr;
    long          _table_len;
    int           _table;
    bool          _ovrspl_flag;

private:
    // Forbidden member functions
    BaseVoiceState(const BaseVoiceState &other);
    bool operator==(const BaseVoiceState &other);
    bool operator!=(const BaseVoiceState &other);
};

//-------------------------------------------------------------------------
// Inline implementations
//-------------------------------------------------------------------------

inline BaseVoiceState::BaseVoiceState()
: _pos(), _step(), _table_ptr(0), _table_len(0), _table(0), _ovrspl_flag(true)
{
    _pos._all  = 0;
    _step._all = static_cast<Int64>(0x80000000UL);
}

inline BaseVoiceState & BaseVoiceState::operator=(const BaseVoiceState &other)
{
    assert(&other != 0);
    _pos         = other._pos;
    _step        = other._step;
    _table_ptr   = other._table_ptr;
    _table_len   = other._table_len;
    _table       = other._table;
    _ovrspl_flag = other._ovrspl_flag;
    return *this;
}

inline void BaseVoiceState::compute_step(long pitch)
{
    // _table must be non-negative.
    assert(_table >= 0);

    int shift;
    if (pitch < 0)
    {
        // e.g. -1   => -1, -0x10000 => -1, -0x10001 => -2, etc.
        shift = -1 - ((~pitch) >> NBR_BITS_PER_OCT);
    }
    else
    {
        shift = (pitch >> NBR_BITS_PER_OCT) - _table;
    }

    if (!_ovrspl_flag)
    {
        ++shift;
    }

    const int mask = (1 << NBR_BITS_PER_OCT) - 1;
    const int pitch_frac = static_cast<int>(pitch) & mask;
    // Compute step: exponential scaling with the pitch fractional part.
    _step._all = static_cast<Int64>(floor(
                      exp(pitch_frac * (LN2 / static_cast<double>(1 << NBR_BITS_PER_OCT)))
                      * (1UL << 31)
                  ));
    assert(_step._all >= static_cast<Int64>(1UL << 31));
    _step._all = shift_bidi(_step._all, shift);
}

} // namespace rspl

#endif // RSPL_BASEVOICESTATE_H
