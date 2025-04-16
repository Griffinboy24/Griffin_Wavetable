/******************************************************************************
    rspl_basevoicestate.h – Header?only BaseVoiceState
    MODIFIED 2025?04?16: adds per?voice cycle_len / cycle_mask for single?cycle
******************************************************************************/

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

        /* public data ----------------------------------------------------- */
        Fixed3232     _pos;          // 32.32 position
        Fixed3232     _step;         // 32.32 step
        const float* _table_ptr;    // pointer to cycle start
        long          _table_len;    // length of full MIP level
        int           _table;        // current table index
        bool          _ovrspl_flag;  // true if oversample path used

        /* NEW: single?cycle parameters */
        UInt32        _cycle_len;    // power?of?two cycle length
        UInt32        _cycle_mask;   // _cycle_len?1, for wrapping

    private:
        BaseVoiceState(const BaseVoiceState& other);            // forbidden
        bool operator==(const BaseVoiceState& other);           // forbidden
        bool operator!=(const BaseVoiceState& other);           // forbidden
    };

    /*----------------------------- impl -----------------------------------*/

    inline BaseVoiceState::BaseVoiceState()
        : _pos(), _step(), _table_ptr(0), _table_len(0), _table(0), _ovrspl_flag(true),
        _cycle_len(0), _cycle_mask(0)
    {
        _pos._all = 0;
        _step._all = static_cast<Int64>(0x80000000UL);
    }

    inline BaseVoiceState& BaseVoiceState::operator=(const BaseVoiceState& other)
    {
        assert(&other != 0);
        _pos = other._pos;
        _step = other._step;
        _table_ptr = other._table_ptr;
        _table_len = other._table_len;
        _table = other._table;
        _ovrspl_flag = other._ovrspl_flag;
        _cycle_len = other._cycle_len;
        _cycle_mask = other._cycle_mask;
        return *this;
    }

    inline void BaseVoiceState::compute_step(long pitch)
    {
        int shift;
        if (pitch < 0)
        {
            shift = -1 - ((~pitch) >> NBR_BITS_PER_OCT);
        }
        else
        {
            shift = (pitch >> NBR_BITS_PER_OCT) - _table;
        }
        if (!_ovrspl_flag) { ++shift; }

        const int mask = (1 << NBR_BITS_PER_OCT) - 1;
        const int pitch_frac = static_cast<int>(pitch) & mask;
        _step._all = static_cast<Int64>(floor(
            exp(pitch_frac * (LN2 / static_cast<double>(1 << NBR_BITS_PER_OCT))) * (1UL << 31)));
        assert(_step._all >= static_cast<Int64>(1UL << 31));
        _step._all = shift_bidi(_step._all, shift);
    }

} // namespace rspl
#endif // RSPL_BASEVOICESTATE_H
