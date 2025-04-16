#ifndef RSPL_INTERP_H
#define RSPL_INTERP_H

#include <cassert>
#include <cmath>

namespace rspl {

    /*=========================== InterpFltPhase ============================*/

    template <int SC>
    class InterpFltPhase
    {
    public:
        enum { SCALE = SC };
        enum { FIR_LEN = 12 * SCALE };

        InterpFltPhase();
        rspl_FORCEINLINE float convolve(const float data_ptr[], float q) const;

        float _dif[FIR_LEN];
        float _imp[FIR_LEN];

    private:
        enum { CHK_IMPULSE_NOT_SET = 12345 };
    };

    template <int SC>
    InterpFltPhase<SC>::InterpFltPhase()
    {
        _imp[0] = CHK_IMPULSE_NOT_SET;
    }

    template <int SC>
    rspl_FORCEINLINE float InterpFltPhase<SC>::convolve(const float[], float) const
    {
        assert(false); /* generic version unused */
        return 0;
    }

    /*-------------------------- SC = 1 special -----------------------------*/
    template <>
    rspl_FORCEINLINE float InterpFltPhase<1>::convolve(const float data_ptr[], float q) const
    {
        assert(_imp[0] != CHK_IMPULSE_NOT_SET);
        float c0 = 0, c1 = 0;
        c0 = (_imp[0] + _dif[0] * q) * data_ptr[0];
        c1 = (_imp[1] + _dif[1] * q) * data_ptr[1];
        c0 += (_imp[2] + _dif[2] * q) * data_ptr[2];
        c1 += (_imp[3] + _dif[3] * q) * data_ptr[3];
        c0 += (_imp[4] + _dif[4] * q) * data_ptr[4];
        c1 += (_imp[5] + _dif[5] * q) * data_ptr[5];
        c0 += (_imp[6] + _dif[6] * q) * data_ptr[6];
        c1 += (_imp[7] + _dif[7] * q) * data_ptr[7];
        c0 += (_imp[8] + _dif[8] * q) * data_ptr[8];
        c1 += (_imp[9] + _dif[9] * q) * data_ptr[9];
        c0 += (_imp[10] + _dif[10] * q) * data_ptr[10];
        c1 += (_imp[11] + _dif[11] * q) * data_ptr[11];
        return c0 + c1;
    }

    /*-------------------------- SC = 2 special -----------------------------*/
    template <>
    rspl_FORCEINLINE float InterpFltPhase<2>::convolve(const float data_ptr[], float q) const
    {
        assert(_imp[0] != CHK_IMPULSE_NOT_SET);
        float c0 = 0, c1 = 0;
        c0 = (_imp[0] + _dif[0] * q) * data_ptr[0];
        c1 = (_imp[1] + _dif[1] * q) * data_ptr[1];
        c0 += (_imp[2] + _dif[2] * q) * data_ptr[2];
        c1 += (_imp[3] + _dif[3] * q) * data_ptr[3];
        c0 += (_imp[4] + _dif[4] * q) * data_ptr[4];
        c1 += (_imp[5] + _dif[5] * q) * data_ptr[5];
        c0 += (_imp[6] + _dif[6] * q) * data_ptr[6];
        c1 += (_imp[7] + _dif[7] * q) * data_ptr[7];
        c0 += (_imp[8] + _dif[8] * q) * data_ptr[8];
        c1 += (_imp[9] + _dif[9] * q) * data_ptr[9];
        c0 += (_imp[10] + _dif[10] * q) * data_ptr[10];
        c1 += (_imp[11] + _dif[11] * q) * data_ptr[11];
        c0 += (_imp[12] + _dif[12] * q) * data_ptr[12];
        c1 += (_imp[13] + _dif[13] * q) * data_ptr[13];
        c0 += (_imp[14] + _dif[14] * q) * data_ptr[14];
        c1 += (_imp[15] + _dif[15] * q) * data_ptr[15];
        c0 += (_imp[16] + _dif[16] * q) * data_ptr[16];
        c1 += (_imp[17] + _dif[17] * q) * data_ptr[17];
        c0 += (_imp[18] + _dif[18] * q) * data_ptr[18];
        c1 += (_imp[19] + _dif[19] * q) * data_ptr[19];
        c0 += (_imp[20] + _dif[20] * q) * data_ptr[20];
        c1 += (_imp[21] + _dif[21] * q) * data_ptr[21];
        c0 += (_imp[22] + _dif[22] * q) * data_ptr[22];
        c1 += (_imp[23] + _dif[23] * q) * data_ptr[23];
        return c0 + c1;
    }

    /*============================= InterpFlt ===============================*/

    template <int SC>
    class InterpFlt
    {
    public:
        typedef InterpFltPhase<SC> Phase;

        enum { SCALE = Phase::SCALE };
        enum { FIR_LEN = Phase::FIR_LEN };
        enum { NBR_PHASES_L2 = 6 };
        enum { NBR_PHASES = 1 << NBR_PHASES_L2 };
        enum { IMPULSE_LEN = FIR_LEN * NBR_PHASES };

        InterpFlt();
        void set_impulse(const double imp_ptr[IMPULSE_LEN]);

        rspl_FORCEINLINE float interpolate(const float data_ptr[],
            UInt32 frac_pos) const;

        /* NEW: per?voice mask variant */
        rspl_FORCEINLINE float interpolate_masked(const float table_ptr[],
            UInt32    base_idx,
            UInt32    frac_pos,
            UInt32    cycle_mask) const;

    private:
        Phase _phase_arr[NBR_PHASES];
    };

    template <int SC>
    InterpFlt<SC>::InterpFlt() : _phase_arr() {}

    template <int SC>
    void InterpFlt<SC>::set_impulse(const double imp_ptr[IMPULSE_LEN])
    {
        double next = 0;
        for (int fir = FIR_LEN - 1; fir >= 0; --fir)
        {
            for (int ph = NBR_PHASES - 1; ph >= 0; --ph)
            {
                const int imp_pos = fir * NBR_PHASES + ph;
                const double coef_d = imp_ptr[imp_pos];
                const float  coef = static_cast<float>(coef_d);
                const float  dif = static_cast<float>(next - coef_d);
                const int    tblPos = FIR_LEN - 1 - fir;
                Phase& p = _phase_arr[ph];
                p._imp[tblPos] = coef;
                p._dif[tblPos] = dif;
                next = coef_d;
            }
        }
    }

    template <int SC>
    rspl_FORCEINLINE float InterpFlt<SC>::interpolate(const float data_ptr[],
        UInt32 frac_pos) const
    {
        const float q_scl = 1.0f / (65536.0f * 65536.0f);
        const float q = static_cast<float>(frac_pos << NBR_PHASES_L2) * q_scl;
        const int   ph = frac_pos >> (32 - NBR_PHASES_L2);
        const int   offset = -FIR_LEN / 2 + 1;
        return _phase_arr[ph].convolve(data_ptr + offset, q);
    }

    /*------------------ masked tap?by?tap variant --------------------------*/
    template <int SC>
    rspl_FORCEINLINE float InterpFlt<SC>::interpolate_masked(const float table_ptr[],
        UInt32 base_idx,
        UInt32 frac_pos,
        UInt32 cycle_mask) const
    {
        const float q_scl = 1.0f / (65536.0f * 65536.0f);
        const float q = static_cast<float>(frac_pos << NBR_PHASES_L2) * q_scl;
        const int   ph = frac_pos >> (32 - NBR_PHASES_L2);
        const Phase& phase = _phase_arr[ph];
        const int offset = -FIR_LEN / 2 + 1;

        float sum = 0.0f;
        for (int tap = 0; tap < FIR_LEN; ++tap)
        {
            const UInt32 idx = (base_idx + offset + tap) & cycle_mask;
            sum += (phase._imp[tap] + phase._dif[tap] * q) * table_ptr[idx];
        }
        return sum;
    }

    /*============================= InterpPack ==============================*/

    class InterpPack
    {
    public:
        InterpPack();

        void interp_ovrspl(float dest_ptr[], long nbr_spl, BaseVoiceState& v) const;
        void interp_norm(float dest_ptr[], long nbr_spl, BaseVoiceState& v) const;
        void interp_ovrspl_ramp_add(float dest_ptr[], long nbr_spl,
            BaseVoiceState& v, float vol, float vol_step) const;
        void interp_norm_ramp_add(float dest_ptr[], long nbr_spl,
            BaseVoiceState& v, float vol, float vol_step) const;

        static long get_len_pre();
        static long get_len_post();

    private:
        typedef InterpFlt<2> InterpRate1x;  // oversampled
        typedef InterpFlt<1> InterpRate2x;  // normal

        InterpRate1x _interp_1x;
        InterpRate2x _interp_2x;
    };

    inline InterpPack::InterpPack() : _interp_1x(), _interp_2x()
    {
        _interp_1x.set_impulse(FIR_1X_COEF_ARR);
        _interp_2x.set_impulse(FIR_2X_COEF_ARR);
    }

    /*---------------------- masked core helpers ----------------------------*/
    inline void InterpPack::interp_ovrspl(float dest_ptr[], long n, BaseVoiceState& v) const
    {
        const UInt32 mask = v._cycle_mask;
        for (long i = 0; i < n; ++i)
        {
            dest_ptr[i] = 0.5f * _interp_2x.interpolate_masked(
                v._table_ptr, v._pos._part._msw, v._pos._part._lsw, mask);
            v._pos._all += v._step._all;
        }
    }

    inline void InterpPack::interp_norm(float dest_ptr[], long n, BaseVoiceState& v) const
    {
        const UInt32 mask = v._cycle_mask;
        for (long i = 0; i < n; ++i)
        {
            dest_ptr[i] = _interp_1x.interpolate_masked(
                v._table_ptr, v._pos._part._msw, v._pos._part._lsw, mask);
            v._pos._all += v._step._all;
        }
    }

    inline void InterpPack::interp_ovrspl_ramp_add(float dest_ptr[], long n,
        BaseVoiceState& v,
        float vol, float vol_step) const
    {
        vol *= 0.5f;
        vol_step *= 0.5f;
        const UInt32 mask = v._cycle_mask;
        for (long i = 0; i < n; ++i)
        {
            dest_ptr[i] += vol * _interp_2x.interpolate_masked(
                v._table_ptr, v._pos._part._msw, v._pos._part._lsw, mask);
            v._pos._all += v._step._all;
            vol += vol_step;
        }
    }

    inline void InterpPack::interp_norm_ramp_add(float dest_ptr[], long n,
        BaseVoiceState& v,
        float vol, float vol_step) const
    {
        vol_step *= 2.0f;
        const UInt32 mask = v._cycle_mask;
        long i = 0;
        while (i < n)
        {
            dest_ptr[i] += vol * _interp_1x.interpolate_masked(
                v._table_ptr, v._pos._part._msw, v._pos._part._lsw, mask);
            v._pos._all += v._step._all;
            vol += vol_step;
            i += 2;          // keep original stride
        }
    }

    inline long InterpPack::get_len_pre() { return static_cast<long>(InterpRate1x::FIR_LEN / 2); }
    inline long InterpPack::get_len_post() { return static_cast<long>(InterpRate1x::FIR_LEN / 2); }

} // namespace rspl
#endif // RSPL_INTERP_H
