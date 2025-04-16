/******************************************************************************
    rspl_resamplerflt.h – Header?only ResamplerFlt
    MODIFIED 2025?04?16: per?voice single?cycle support
******************************************************************************/

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
        enum { BASE_CYCLE_LEN = 1 << 11 };   // 2048?sample saw cycle

        ResamplerFlt();
        ~ResamplerFlt() {}

        /* connections */
        void set_interp(const InterpPack& interp);
        void set_sample(const MipMapFlt& spl);
        void remove_sample();

        /* control */
        void set_pitch(long pitch);
        long get_pitch() const;
        void set_playback_pos(Int64 pos);
        Int64 get_playback_pos() const;

        /* render */
        void interpolate_block(float dest_ptr[], long nbr_spl);
        void clear_buffers();

    private:
        enum VoiceInfo { VoiceInfo_CURRENT = 0, VoiceInfo_FADEOUT, VoiceInfo_NBR_ELT };

        std::vector<float> _buf;
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

        /* helpers */
        void   reset_pitch_cur_voice();
        void   fade_block(float dest_ptr[], long nbr_spl);
        int    compute_table(long pitch);
        void   begin_mip_map_fading();

        /* no copies */
        ResamplerFlt(const ResamplerFlt&);
        ResamplerFlt& operator=(const ResamplerFlt&);
    };

    /*----------------------------- constructor -----------------------------*/
    inline ResamplerFlt::ResamplerFlt()
        : _buf(), _mip_map_ptr(0), _interp_ptr(0), _dwnspl(), _voice_arr(),
        _pitch(0), _buf_len(128), _fade_pos(0),
        _fade_flag(false), _fade_needed_flag(false), _can_use_flag(false)
    {
        _dwnspl.set_coefs(DOWNSAMPLER_COEF_ARR);
        _buf.resize(_buf_len * 2);
    }

    /*------------------------------ wiring --------------------------------*/
    inline void ResamplerFlt::set_interp(const InterpPack& interp)
    {
        _interp_ptr = &interp;
    }

    inline void ResamplerFlt::set_sample(const MipMapFlt& spl)
    {
        assert(spl.is_ready());
        _mip_map_ptr = &spl;
        _pitch = 0;
        _voice_arr[VoiceInfo_CURRENT]._pos._all = 0;
        reset_pitch_cur_voice();
    }

    inline void ResamplerFlt::remove_sample() { _mip_map_ptr = 0; }

    /*----------------------------- pitch ----------------------------------*/
    inline void ResamplerFlt::set_pitch(long pitch)
    {
        assert(_mip_map_ptr && _interp_ptr);
        assert(pitch < _mip_map_ptr->get_nbr_tables() * (1L << NBR_BITS_PER_OCT));

        BaseVoiceState& old_v = _voice_arr[VoiceInfo_FADEOUT];
        BaseVoiceState& cur_v = _voice_arr[VoiceInfo_CURRENT];

        _pitch = pitch;
        const int new_table = compute_table(pitch);
        const bool new_ovrspl = (pitch >= 0);
        _fade_needed_flag = (new_table != cur_v._table) || (new_ovrspl != cur_v._ovrspl_flag);

        cur_v.compute_step(_pitch);
        if (_fade_flag) { old_v.compute_step(_pitch); }
    }

    inline long ResamplerFlt::get_pitch() const { return _pitch; }

    /*--------------------------- playback pos -----------------------------*/
    inline void ResamplerFlt::set_playback_pos(Int64 pos)
    {
        assert(_mip_map_ptr && _interp_ptr);
        assert(pos >= 0 && (pos >> 32) < _mip_map_ptr->get_sample_len());

        _voice_arr[VoiceInfo_CURRENT]._pos._all = pos >> _voice_arr[VoiceInfo_CURRENT]._table;
        if (_fade_flag)
            _voice_arr[VoiceInfo_FADEOUT]._pos._all = pos >> _voice_arr[VoiceInfo_FADEOUT]._table;
    }

    inline Int64 ResamplerFlt::get_playback_pos() const
    {
        const BaseVoiceState& cur_v = _voice_arr[VoiceInfo_CURRENT];
        return (cur_v._pos._all << cur_v._table);
    }

    /*---------------------------- helpers ---------------------------------*/
    inline int ResamplerFlt::compute_table(long pitch)
    {
        return (pitch >= 0) ? (pitch >> NBR_BITS_PER_OCT) : 0;
    }

    /* per?voice table / cycle / mask */
    inline void ResamplerFlt::reset_pitch_cur_voice()
    {
        assert(_mip_map_ptr);
        BaseVoiceState& cur = _voice_arr[VoiceInfo_CURRENT];

        cur._table = compute_table(_pitch);
        cur._table_len = _mip_map_ptr->get_lev_len(cur._table);
        cur._table_ptr = _mip_map_ptr->use_table(cur._table);
        cur._ovrspl_flag = (_pitch >= 0);

        cur._cycle_len = static_cast<UInt32>(BASE_CYCLE_LEN >> cur._table);
        cur._cycle_mask = cur._cycle_len - 1U;

        cur.compute_step(_pitch);
    }

    inline void ResamplerFlt::begin_mip_map_fading()
    {
        BaseVoiceState& old_v = _voice_arr[VoiceInfo_FADEOUT];
        BaseVoiceState& cur_v = _voice_arr[VoiceInfo_CURRENT];

        old_v = cur_v;                // copy incl. cycle data
        reset_pitch_cur_voice();      // recompute cur_v for new table
        const int d = old_v._table - cur_v._table;
        cur_v._pos._all = shift_bidi(old_v._pos._all, d);

        _fade_needed_flag = false;
        _fade_flag = true;
        _fade_pos = 0;
    }

    /*---------------------------- rendering --------------------------------*/
    inline void ResamplerFlt::interpolate_block(float dest_ptr[], long nbr_spl)
    {
        assert(_mip_map_ptr && _interp_ptr && dest_ptr && nbr_spl > 0);

        if (_fade_needed_flag && !_fade_flag) { begin_mip_map_fading(); }

        long pos = 0;
        while (pos < nbr_spl)
        {
            long work = nbr_spl - pos;
            if (_fade_flag)
            {
                work = min(work, _buf_len);
                work = min(work, BaseVoiceState::FADE_LEN - _fade_pos);
                fade_block(dest_ptr + pos, work);
            }
            else if (_voice_arr[VoiceInfo_CURRENT]._ovrspl_flag)
            {
                work = min(work, _buf_len);
                _interp_ptr->interp_ovrspl(&_buf[0], work * 2, _voice_arr[VoiceInfo_CURRENT]);
                _dwnspl.downsample_block(dest_ptr + pos, &_buf[0], work);
            }
            else
            {
                _interp_ptr->interp_norm(dest_ptr + pos, work, _voice_arr[VoiceInfo_CURRENT]);
                _dwnspl.phase_block(dest_ptr + pos, dest_ptr + pos, work);
            }
            pos += work;
        }
    }

    inline void ResamplerFlt::fade_block(float dest_ptr[], long n)
    {
        const long n2 = n * 2;
        const float vStep = 1.0f / (BaseVoiceState::FADE_LEN * 2);
        const float v = _fade_pos * (vStep * 2);

        BaseVoiceState& old_v = _voice_arr[VoiceInfo_FADEOUT];
        BaseVoiceState& cur_v = _voice_arr[VoiceInfo_CURRENT];

        memset(_buf.data(), 0, sizeof(_buf[0]) * n2);

        if (cur_v._ovrspl_flag && old_v._ovrspl_flag)
        {
            _interp_ptr->interp_ovrspl_ramp_add(_buf.data(), n2, cur_v, v, vStep);
            _interp_ptr->interp_ovrspl_ramp_add(_buf.data(), n2, old_v, 1.0f - v, -vStep);
        }
        else if (!cur_v._ovrspl_flag && old_v._ovrspl_flag)
        {
            _interp_ptr->interp_norm_ramp_add(_buf.data(), n2, cur_v, v, vStep);
            _interp_ptr->interp_ovrspl_ramp_add(_buf.data(), n2, old_v, 1.0f - v, -vStep);
        }
        else
        {
            _interp_ptr->interp_ovrspl_ramp_add(_buf.data(), n2, cur_v, v, vStep);
            _interp_ptr->interp_norm_ramp_add(_buf.data(), n2, old_v, 1.0f - v, -vStep);
        }

        _dwnspl.downsample_block(dest_ptr, _buf.data(), n);

        _fade_pos += n;
        _fade_flag = (_fade_pos < BaseVoiceState::FADE_LEN);
    }

    inline void ResamplerFlt::clear_buffers()
    {
        _dwnspl.clear_buffers();
        if (_mip_map_ptr) { reset_pitch_cur_voice(); }
        _fade_needed_flag = false;
        _fade_flag = false;
    }

} // namespace rspl
#endif // RSPL_RESAMPLERFLT_H
