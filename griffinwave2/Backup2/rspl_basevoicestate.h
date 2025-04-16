

#ifndef RSPL_BASEVOICESTATE_H
#define RSPL_BASEVOICESTATE_H

#include "rspl.h"
#include <cassert>
#include <cmath>

namespace rspl
{

    class BaseVoiceState
    {
    public:
        enum { NBR_BITS_PER_OCT = 16 };
        enum { FADE_LEN = 64 };

        BaseVoiceState();
        BaseVoiceState& operator=(const BaseVoiceState& other);

        void compute_step(long pitch);

        Fixed3232     _pos;          // 32.32 position
        Fixed3232     _step;         // 32.32 step per sample
        const float* _table_ptr;    // pointer to frame start in current MIP level
        long          _table_len;    // total level length (unused for mask now)
        int           _table;        // current MIP table
        bool          _ovrspl_flag;  // true if oversampled path

        UInt32        _cycle_len;    // power?of?two cycle length (2048  table)
        UInt32        _cycle_mask;   // _cycle_len - 1

        UInt32        _frame_idx;    // 0..255
        UInt32        _frame_offset; // samples from start of level 0 to frame start

    private:
        BaseVoiceState(const BaseVoiceState&);
        bool operator==(const BaseVoiceState&);
        bool operator!=(const BaseVoiceState&);
    };

    /*-------------------------------------------------------------------------*/

    inline BaseVoiceState::BaseVoiceState()
        : _pos(), _step(), _table_ptr(0), _table_len(0), _table(0), _ovrspl_flag(true),
        _cycle_len(0), _cycle_mask(0), _frame_idx(0), _frame_offset(0)
    {
        _pos._all = 0;
        _step._all = static_cast<Int64>(0x80000000UL);
    }

    inline BaseVoiceState& BaseVoiceState::operator=(const BaseVoiceState& other)
    {
        _pos = other._pos;
        _step = other._step;
        _table_ptr = other._table_ptr;
        _table_len = other._table_len;
        _table = other._table;
        _ovrspl_flag = other._ovrspl_flag;
        _cycle_len = other._cycle_len;
        _cycle_mask = other._cycle_mask;
        _frame_idx = other._frame_idx;
        _frame_offset = other._frame_offset;
        return *this;
    }

    inline void BaseVoiceState::compute_step(long pitch)
    {
        int shift = (pitch < 0)
            ? -1 - ((~pitch) >> NBR_BITS_PER_OCT)
            : (pitch >> NBR_BITS_PER_OCT) - _table;
        if (!_ovrspl_flag) ++shift;

        const int mask = (1 << NBR_BITS_PER_OCT) - 1;
        const int frac = pitch & mask;
        _step._all = static_cast<Int64>(floor(
            exp(frac * (LN2 / double(1 << NBR_BITS_PER_OCT))) * (1UL << 31)));
        _step._all = shift_bidi(_step._all, shift);
    }

} // namespace rspl
#endif // RSPL_BASEVOICESTATE_H
