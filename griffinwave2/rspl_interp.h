#ifndef RSPL_INTERP_H
#define RSPL_INTERP_H

#include <cassert>
#include <cmath>

namespace rspl {

    //==========================================================================
    // InterpFltPhase
    //==========================================================================

    template <int SC>
    class InterpFltPhase
    {
    public:
        enum { SCALE = SC };
        enum { FIR_LEN = 12 * SCALE };

        InterpFltPhase();
        rspl_FORCEINLINE float convolve(const float data_ptr[], float q) const;

        float _dif[FIR_LEN]; // Difference values (indices reversed)
        float _imp[FIR_LEN]; // Impulse values (indices reversed)

    private:
        enum { CHK_IMPULSE_NOT_SET = 12345 };
    };

    template <int SC>
    InterpFltPhase<SC>::InterpFltPhase()
    {
        _imp[0] = CHK_IMPULSE_NOT_SET;
    }

    template <int SC>
    rspl_FORCEINLINE float InterpFltPhase<SC>::convolve(const float data_ptr[], float q) const
    {
        // Generic version: should not be used
        assert(false);
        return 0;
    }

    // Specialization for SC = 1
    template <>
    rspl_FORCEINLINE float InterpFltPhase<1>::convolve(const float data_ptr[], float q) const
    {
        assert(_imp[0] != CHK_IMPULSE_NOT_SET);
        float c_0, c_1;
        c_0 = (_imp[0] + _dif[0] * q) * data_ptr[0];
        c_1 = (_imp[1] + _dif[1] * q) * data_ptr[1];
        c_0 += (_imp[2] + _dif[2] * q) * data_ptr[2];
        c_1 += (_imp[3] + _dif[3] * q) * data_ptr[3];
        c_0 += (_imp[4] + _dif[4] * q) * data_ptr[4];
        c_1 += (_imp[5] + _dif[5] * q) * data_ptr[5];
        c_0 += (_imp[6] + _dif[6] * q) * data_ptr[6];
        c_1 += (_imp[7] + _dif[7] * q) * data_ptr[7];
        c_0 += (_imp[8] + _dif[8] * q) * data_ptr[8];
        c_1 += (_imp[9] + _dif[9] * q) * data_ptr[9];
        c_0 += (_imp[10] + _dif[10] * q) * data_ptr[10];
        c_1 += (_imp[11] + _dif[11] * q) * data_ptr[11];
        assert(FIR_LEN == 12);
        return (c_0 + c_1);
    }

    // Specialization for SC = 2
    template <>
    rspl_FORCEINLINE float InterpFltPhase<2>::convolve(const float data_ptr[], float q) const
    {
        assert(_imp[0] != CHK_IMPULSE_NOT_SET);
        float c_0, c_1;
        // For SC = 2, FIR_LEN == 24.
        c_0 = (_imp[0] + _dif[0] * q) * data_ptr[0];
        c_1 = (_imp[1] + _dif[1] * q) * data_ptr[1];
        c_0 += (_imp[2] + _dif[2] * q) * data_ptr[2];
        c_1 += (_imp[3] + _dif[3] * q) * data_ptr[3];
        c_0 += (_imp[4] + _dif[4] * q) * data_ptr[4];
        c_1 += (_imp[5] + _dif[5] * q) * data_ptr[5];
        c_0 += (_imp[6] + _dif[6] * q) * data_ptr[6];
        c_1 += (_imp[7] + _dif[7] * q) * data_ptr[7];
        c_0 += (_imp[8] + _dif[8] * q) * data_ptr[8];
        c_1 += (_imp[9] + _dif[9] * q) * data_ptr[9];
        c_0 += (_imp[10] + _dif[10] * q) * data_ptr[10];
        c_1 += (_imp[11] + _dif[11] * q) * data_ptr[11];
        c_0 += (_imp[12] + _dif[12] * q) * data_ptr[12];
        c_1 += (_imp[13] + _dif[13] * q) * data_ptr[13];
        c_0 += (_imp[14] + _dif[14] * q) * data_ptr[14];
        c_1 += (_imp[15] + _dif[15] * q) * data_ptr[15];
        c_0 += (_imp[16] + _dif[16] * q) * data_ptr[16];
        c_1 += (_imp[17] + _dif[17] * q) * data_ptr[17];
        c_0 += (_imp[18] + _dif[18] * q) * data_ptr[18];
        c_1 += (_imp[19] + _dif[19] * q) * data_ptr[19];
        c_0 += (_imp[20] + _dif[20] * q) * data_ptr[20];
        c_1 += (_imp[21] + _dif[21] * q) * data_ptr[21];
        c_0 += (_imp[22] + _dif[22] * q) * data_ptr[22];
        c_1 += (_imp[23] + _dif[23] * q) * data_ptr[23];
        assert(FIR_LEN == 24);
        return (c_0 + c_1);
    }

    //==========================================================================
    // InterpFlt
    //==========================================================================

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
        ~InterpFlt() {}

        void set_impulse(const double imp_ptr[IMPULSE_LEN]);
        rspl_FORCEINLINE float interpolate(const float data_ptr[], UInt32 frac_pos) const;

    private:
        Phase _phase_arr[NBR_PHASES];

        // Forbidden copy/move operations...
    };

    template <int SC>
    InterpFlt<SC>::InterpFlt() : _phase_arr() {}

    template <int SC>
    void InterpFlt<SC>::set_impulse(const double imp_ptr[IMPULSE_LEN])
    {
        assert(imp_ptr != 0);
        double next_coef_dbl = 0;
        for (int fir_pos = FIR_LEN - 1; fir_pos >= 0; --fir_pos)
        {
            for (int phase_cnt = NBR_PHASES - 1; phase_cnt >= 0; --phase_cnt)
            {
                const int imp_pos = fir_pos * NBR_PHASES + phase_cnt;
                const double coef_dbl = imp_ptr[imp_pos];
                const float coef = static_cast<float>(coef_dbl);
                const float dif = static_cast<float>(next_coef_dbl - coef_dbl);
                const int table_pos = FIR_LEN - 1 - fir_pos;
                Phase& phase = _phase_arr[phase_cnt];
                phase._imp[table_pos] = coef;
                phase._dif[table_pos] = dif;
                next_coef_dbl = coef_dbl;
            }
        }
    }

    template <int SC>
    rspl_FORCEINLINE float InterpFlt<SC>::interpolate(const float data_ptr[], UInt32 frac_pos) const
    {
        assert(data_ptr != 0);
        const float q_scl = 1.0f / (65536.0f * 65536.0f);
        const float q = static_cast<float>(frac_pos << NBR_PHASES_L2) * q_scl;
        const int phase_index = frac_pos >> (32 - NBR_PHASES_L2);
        const Phase& phase = _phase_arr[phase_index];
        const int offset = -FIR_LEN / 2 + 1;
        return phase.convolve(data_ptr + offset, q);
    }

    //==========================================================================
    // InterpPack
    //==========================================================================

    class InterpPack
    {
    public:
        InterpPack();
        ~InterpPack() {}

        void interp_ovrspl(float dest_ptr[], long nbr_spl, BaseVoiceState& voice) const;
        void interp_norm(float dest_ptr[], long nbr_spl, BaseVoiceState& voice) const;
        void interp_ovrspl_ramp_add(float dest_ptr[], long nbr_spl, BaseVoiceState& voice, float vol, float vol_step) const;
        void interp_norm_ramp_add(float dest_ptr[], long nbr_spl, BaseVoiceState& voice, float vol, float vol_step) const;

        static long get_len_pre();
        static long get_len_post();

    private:
        typedef InterpFlt<2> InterpRate1x; // Oversampled (2x)
        typedef InterpFlt<1> InterpRate2x; // Normal-rate

        InterpRate1x _interp_1x;
        InterpRate2x _interp_2x;

        // Forbidden copy/move...
    };

    inline InterpPack::InterpPack() : _interp_1x(), _interp_2x()
    {
        // Use external FIR arrays here
        _interp_1x.set_impulse(FIR_1X_COEF_ARR);
        _interp_2x.set_impulse(FIR_2X_COEF_ARR);
    }

inline void InterpPack::interp_ovrspl(float dest_ptr[], long nbr_spl, BaseVoiceState &voice) const
{
    assert(dest_ptr != 0);
    assert(nbr_spl > 0);
    assert(voice._table_ptr != 0);
    long cnt = 0;
    do
    {
        assert(voice._pos._part._msw < voice._table_len);
        dest_ptr[cnt] = 0.5f * _interp_2x.interpolate(voice._table_ptr + voice._pos._part._msw, voice._pos._part._lsw);
        voice._pos._all += voice._step._all;
        ++cnt;
    }
    while (cnt < nbr_spl);
}

inline void InterpPack::interp_norm(float dest_ptr[], long nbr_spl, BaseVoiceState &voice) const
{
    assert(dest_ptr != 0);
    assert(nbr_spl > 0);
    assert(voice._table_ptr != 0);
    long cnt = 0;
    do
    {
        assert(voice._pos._part._msw < voice._table_len);
        dest_ptr[cnt] = _interp_1x.interpolate(voice._table_ptr + voice._pos._part._msw, voice._pos._part._lsw);
        voice._pos._all += voice._step._all;
        ++cnt;
    }
    while (cnt < nbr_spl);
}

inline void InterpPack::interp_ovrspl_ramp_add(float dest_ptr[], long nbr_spl, BaseVoiceState &voice, float vol, float vol_step) const
{
    assert(dest_ptr != 0);
    assert(nbr_spl > 0);
    assert(voice._table_ptr != 0);
    assert(vol >= 0 && vol <= 1);
    assert(vol_step >= -1 && vol_step <= 1);
    vol *= 0.5f;
    vol_step *= 0.5f;
    long cnt = 0;
    do
    {
        assert(voice._pos._part._msw < voice._table_len);
        dest_ptr[cnt] += vol * _interp_2x.interpolate(voice._table_ptr + voice._pos._part._msw, voice._pos._part._lsw);
        voice._pos._all += voice._step._all;
        vol += vol_step;
        ++cnt;
    }
    while (cnt < nbr_spl);
}

inline void InterpPack::interp_norm_ramp_add(float dest_ptr[], long nbr_spl, BaseVoiceState &voice, float vol, float vol_step) const
{
    assert(dest_ptr != 0);
    assert(nbr_spl > 0);
    assert(voice._table_ptr != 0);
    assert(vol >= 0 && vol <= 1);
    assert(vol_step >= -1 && vol_step <= 1);
    vol_step *= 2.0f;
    long cnt = 0;
    do
    {
        assert(voice._pos._part._msw < voice._table_len);
        dest_ptr[cnt] += vol * _interp_1x.interpolate(voice._table_ptr + voice._pos._part._msw, voice._pos._part._lsw);
        voice._pos._all += voice._step._all;
        vol += vol_step;
        cnt += 2;
    }
    while (cnt < nbr_spl);
}

inline long InterpPack::get_len_pre()
{
    assert(static_cast<long>(InterpRate1x::FIR_LEN) >= static_cast<long>(InterpRate2x::FIR_LEN));
    return static_cast<long>(InterpRate1x::FIR_LEN / 2);
}

inline long InterpPack::get_len_post()
{
    assert(static_cast<long>(InterpRate1x::FIR_LEN) >= static_cast<long>(InterpRate2x::FIR_LEN));
    return static_cast<long>(InterpRate1x::FIR_LEN / 2);
}


} // namespace rspl

#endif // RSPL_INTERP_H
