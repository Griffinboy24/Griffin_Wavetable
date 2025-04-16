/******************************************************************************
    rspl_resamplerflt.h - Header-only conversion of the ResamplerFlt module.
    
    This file combines:
      - ResamplerFlt.h    [&#8203;:contentReference[oaicite:0]{index=0}]
      - ResamplerFlt.cpp  [&#8203;:contentReference[oaicite:1]{index=1}]
    
    Usage:
      1. Construct an instance of InterpPack (from your rspl_interp.h module)
         and an instance of MipMapFlt (from rspl_mipmap.h).
      2. Connect them to an instance of ResamplerFlt using set_interp() and set_sample().
      3. Then set pitch, playback position, and use interpolate_block() to generate data.
    
    Copyright (c) 2003 Laurent de Soras
    Distributed under the GNU Lesser General Public License, version 2.1 or later.
*******************************************************************************/

#ifndef RSPL_RESAMPLERFLT_H
#define RSPL_RESAMPLERFLT_H

#include <vector>
#include <cstring>
#include <cassert>
#include <cmath>


namespace rspl {

    class InterpPack;
    class MipMapFlt;

    class ResamplerFlt
    {
    public:
        enum { MIP_MAP_FIR_LEN = 81 };
        enum { NBR_BITS_PER_OCT = BaseVoiceState::NBR_BITS_PER_OCT };

        ResamplerFlt();
        virtual ~ResamplerFlt() {}

        inline void set_interp(const InterpPack& interp);
        inline void set_sample(const MipMapFlt& spl);
        inline void remove_sample();

        inline void set_pitch(long pitch);
        inline long get_pitch() const;

        inline void set_playback_pos(Int64 pos);
        inline Int64 get_playback_pos() const;

        inline void interpolate_block(float dest_ptr[], long nbr_spl);
        inline void clear_buffers();

        // Removed local static _fir_mip_map_coef_arr.
        // Use MIP_MAP_FIR_COEF_ARR wherever the mip-map FIR coefficients are needed.

    protected:
    private:
        enum VoiceInfo
        {
            VoiceInfo_CURRENT = 0, // Current (or fade-in) voice
            VoiceInfo_FADEOUT,     // Fade-out voice
            VoiceInfo_NBR_ELT
        };

        typedef std::vector<float> SplData;

        inline void reset_pitch_cur_voice();
        inline void fade_block(float dest_ptr[], long nbr_spl);
        inline int compute_table(long pitch);
        inline void begin_mip_map_fading();

        std::vector<float> _buf;   // Intermediate buffer (downsampled; actual size is 2*_buf_len)
        const MipMapFlt* _mip_map_ptr;
        const InterpPack* _interp_ptr;
        Downsampler2Flt    _dwnspl;
        BaseVoiceState     _voice_arr[VoiceInfo_NBR_ELT];
        long               _pitch;
        long               _buf_len;
        long               _fade_pos;
        bool               _fade_flag;
        bool               _fade_needed_flag;
        bool               _can_use_flag;

        // Removed local _dwnspl_coef_arr; we now use external DOWNSAMPLER_COEF_ARR.

        // Forbidden copy/move...
    };

    inline ResamplerFlt::ResamplerFlt()
        : _buf(), _mip_map_ptr(0), _interp_ptr(0), _dwnspl(), _voice_arr(), _pitch(0),
        _buf_len(128), _fade_pos(0), _fade_flag(false), _fade_needed_flag(false), _can_use_flag(false)
    {
        _dwnspl.set_coefs(DOWNSAMPLER_COEF_ARR);
        _buf.resize(_buf_len * 2);
    }

    inline void ResamplerFlt::set_interp(const InterpPack& interp)
    {
        assert(&interp != 0);
        _interp_ptr = &interp;
    }

    inline void ResamplerFlt::set_sample(const MipMapFlt& spl)
    {
        assert(&spl != 0);
        assert(spl.is_ready());
        _mip_map_ptr = &spl;
        _pitch = 0;
        _voice_arr[VoiceInfo_CURRENT]._pos._all = 0;
        reset_pitch_cur_voice();
    }

    inline void ResamplerFlt::remove_sample()
    {
        _mip_map_ptr = 0;
    }

    inline void ResamplerFlt::set_pitch(long pitch)
    {
        assert(_mip_map_ptr != 0);
        assert(_interp_ptr != 0);
        // Ensure pitch is below maximum (number of tables * 0x10000)
        assert(pitch < _mip_map_ptr->get_nbr_tables() * (1L << NBR_BITS_PER_OCT));

        BaseVoiceState& old_voc = _voice_arr[VoiceInfo_FADEOUT];
        BaseVoiceState& cur_voc = _voice_arr[VoiceInfo_CURRENT];

        _pitch = pitch;
        const int new_table = compute_table(pitch);
        const bool new_ovrspl_flag = (pitch >= 0);
        _fade_needed_flag = (new_table != cur_voc._table || new_ovrspl_flag != cur_voc._ovrspl_flag);

        cur_voc.compute_step(_pitch);
        if (_fade_flag)
        {
            old_voc.compute_step(_pitch);
        }
    }

    inline long ResamplerFlt::get_pitch() const
    {
        assert(_mip_map_ptr != 0);
        assert(_interp_ptr != 0);
        return _pitch;
    }

    inline void ResamplerFlt::set_playback_pos(Int64 pos)
    {
        assert(_mip_map_ptr != 0);
        assert(_interp_ptr != 0);
        assert(pos >= 0);
        // The integer part must be less than the sample length.
        assert((pos >> 32) < _mip_map_ptr->get_sample_len());

        _voice_arr[VoiceInfo_CURRENT]._pos._all = pos >> _voice_arr[VoiceInfo_CURRENT]._table;
        if (_fade_flag)
        {
            _voice_arr[VoiceInfo_FADEOUT]._pos._all = pos >> _voice_arr[VoiceInfo_FADEOUT]._table;
        }
    }

    inline Int64 ResamplerFlt::get_playback_pos() const
    {
        assert(_mip_map_ptr != 0);
        assert(_interp_ptr != 0);
        const BaseVoiceState& cur_voc = _voice_arr[VoiceInfo_CURRENT];
        const Int64 pos = cur_voc._pos._all;
        const int table = cur_voc._table;
        return (pos << table);
    }

    inline void ResamplerFlt::interpolate_block(float dest_ptr[], long nbr_spl)
    {
        assert(_mip_map_ptr != 0);
        assert(_interp_ptr != 0);
        assert(dest_ptr != 0);
        assert(nbr_spl > 0);

        if (_fade_needed_flag && !_fade_flag)
        {
            begin_mip_map_fading();
        }

        long block_pos = 0;
        while (block_pos < nbr_spl)
        {
            long work_len = nbr_spl - block_pos;
            if (_fade_flag)
            {
                work_len = min(work_len, _buf_len);
                work_len = min(work_len, BaseVoiceState::FADE_LEN - _fade_pos);
                fade_block(&dest_ptr[block_pos], work_len);
            }
            else if (_voice_arr[VoiceInfo_CURRENT]._ovrspl_flag)
            {
                work_len = min(work_len, _buf_len);
                _interp_ptr->interp_ovrspl(&_buf[0], work_len * 2, _voice_arr[VoiceInfo_CURRENT]);
                _dwnspl.downsample_block(&dest_ptr[block_pos], &_buf[0], work_len);
            }
            else
            {
                _interp_ptr->interp_norm(&dest_ptr[block_pos], work_len, _voice_arr[VoiceInfo_CURRENT]);
                _dwnspl.phase_block(&dest_ptr[block_pos], &dest_ptr[block_pos], work_len);
            }
            block_pos += work_len;
        }
    }

    inline void ResamplerFlt::clear_buffers()
    {
        _dwnspl.clear_buffers();
        if (_mip_map_ptr != 0)
        {
            reset_pitch_cur_voice();
        }
        _fade_needed_flag = false;
        _fade_flag = false;
    }

inline void ResamplerFlt::reset_pitch_cur_voice()
{
    assert(_mip_map_ptr != 0);
    BaseVoiceState & cur_voc = _voice_arr[VoiceInfo_CURRENT];
    cur_voc._table = compute_table(_pitch);
    cur_voc._table_len = _mip_map_ptr->get_lev_len(cur_voc._table);
    cur_voc._table_ptr = _mip_map_ptr->use_table(cur_voc._table);
    cur_voc._ovrspl_flag = (_pitch >= 0);
    cur_voc.compute_step(_pitch);
}

inline void ResamplerFlt::fade_block(float dest_ptr[], long nbr_spl)
{
    assert(dest_ptr != 0);
    const long nbr_spl_ovr = nbr_spl * 2;
    const float vol_step = 1.0f / (BaseVoiceState::FADE_LEN * 2);
    const float vol = _fade_pos * (vol_step * 2);
    BaseVoiceState & old_voc = _voice_arr[VoiceInfo_FADEOUT];
    BaseVoiceState & cur_voc = _voice_arr[VoiceInfo_CURRENT];

    memset(&_buf[0], 0, sizeof(_buf[0]) * nbr_spl_ovr);

    assert(old_voc._ovrspl_flag || cur_voc._ovrspl_flag);

    if (cur_voc._ovrspl_flag && old_voc._ovrspl_flag)
    {
        _interp_ptr->interp_ovrspl_ramp_add(&_buf[0], nbr_spl_ovr, cur_voc, vol, vol_step);
        _interp_ptr->interp_ovrspl_ramp_add(&_buf[0], nbr_spl_ovr, old_voc, 1.0f - vol, -vol_step);
    }
    else if (!cur_voc._ovrspl_flag && old_voc._ovrspl_flag)
    {
        _interp_ptr->interp_norm_ramp_add(&_buf[0], nbr_spl_ovr, cur_voc, vol, vol_step);
        _interp_ptr->interp_ovrspl_ramp_add(&_buf[0], nbr_spl_ovr, old_voc, 1.0f - vol, -vol_step);
    }
    else
    {
        _interp_ptr->interp_ovrspl_ramp_add(&_buf[0], nbr_spl_ovr, cur_voc, vol, vol_step);
        _interp_ptr->interp_norm_ramp_add(&_buf[0], nbr_spl_ovr, old_voc, 1.0f - vol, -vol_step);
    }

    _dwnspl.downsample_block(&dest_ptr[0], &_buf[0], nbr_spl);

    _fade_pos += nbr_spl;
    _fade_flag = (_fade_pos < BaseVoiceState::FADE_LEN);
}

inline int ResamplerFlt::compute_table(long pitch)
{
    int table = 0;
    if (pitch >= 0)
    {
        table = pitch >> NBR_BITS_PER_OCT;
    }
    return table;
}

inline void ResamplerFlt::begin_mip_map_fading()
{
    BaseVoiceState & old_voc = _voice_arr[VoiceInfo_FADEOUT];
    BaseVoiceState & cur_voc = _voice_arr[VoiceInfo_CURRENT];

    old_voc = cur_voc; // Use the assignment operator defined in BaseVoiceState
    reset_pitch_cur_voice();
    const int table_dif = old_voc._table - cur_voc._table;
    cur_voc._pos._all = shift_bidi(old_voc._pos._all, table_dif);
    _fade_needed_flag = false;
    _fade_flag = true;
    _fade_pos = 0;
}


} // namespace rspl

#endif // RSPL_RESAMPLERFLT_H
