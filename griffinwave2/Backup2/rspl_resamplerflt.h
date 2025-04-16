/*====================================================================
    rspl_resamplerflt.h – ResamplerFlt now drives a per?frame
    rspl::MipMapSet<256, 3072>.
====================================================================*/
#ifndef RSPL_RESAMPLERFLT_H
#define RSPL_RESAMPLERFLT_H

#include <vector>
#include <cstring>
#include <cassert>


namespace rspl
{
    class ResamplerFlt
    {
    public:
        enum { MIP_MAP_FIR_LEN = 81 };
        enum { NBR_BITS_PER_OCT = BaseVoiceState::NBR_BITS_PER_OCT };

        enum { FRAME_LEN = 1 << 11 };          /* 2048 */
        enum { FRAME_PAD = FRAME_LEN >> 1 };   /* 1024 (guard) */
        enum { FRAME_STRIDE = FRAME_LEN + FRAME_PAD }; /* 3072 */
        enum { FRAME_COUNT = 256 };

        typedef MipMapSet<FRAME_COUNT, FRAME_STRIDE> SampleSet;

        ResamplerFlt();
        ~ResamplerFlt() {}

        void set_interp(const InterpPack& ip);
        void set_sample(const SampleSet& set);
        void remove_sample();

        void set_pitch(long p);
        long get_pitch() const { return _pitch; }

        void set_frame(UInt32 f);
        UInt32 get_frame() const { return _target_frame; }

        void set_playback_pos(Int64 pos);
        Int64 get_playback_pos() const;

        void interpolate_block(float* dst, long n);
        void clear_buffers();

    private:
        enum Voice { CUR = 0, FADE, NBR };

        std::vector<float> _buf;
        const SampleSet* _set;
        const InterpPack* _ip;
        Downsampler2Flt    _dwn;
        BaseVoiceState     _v[NBR];
        long               _pitch;
        UInt32             _target_frame;
        long               _buf_len;
        long               _fade_pos;
        bool               _fade_flag;
        bool               _fade_needed;

        /* helpers */
        void reset_cur_voice();
        void begin_fade();
        void fade_block(float* dst, long n);
        static int table_from_pitch(long p);

        /* no copy */
        ResamplerFlt(const ResamplerFlt&) = delete;
        ResamplerFlt& operator=(const ResamplerFlt&) = delete;
    };

    /*---------------- ctor ----------------*/
    inline ResamplerFlt::ResamplerFlt()
        : _buf(),
        _set(nullptr),
        _ip(nullptr),
        _dwn(),
        _v(),
        _pitch(0),
        _target_frame(0),
        _buf_len(128),
        _fade_pos(0),
        _fade_flag(false),
        _fade_needed(false)
    {
        _dwn.set_coefs(DOWNSAMPLER_COEF_ARR);
        _buf.resize(_buf_len * 2);
    }

    /*-------------- wiring --------------*/
    inline void ResamplerFlt::set_interp(const InterpPack& ip) { _ip = &ip; }

    inline void ResamplerFlt::set_sample(const SampleSet& set)
    {
        _set = &set;
        _v[CUR]._pos._all = 0;
        reset_cur_voice();
    }

    inline void ResamplerFlt::remove_sample() { _set = nullptr; }

    /*-------------- helpers -------------*/
    inline int ResamplerFlt::table_from_pitch(long p)
    {
        return (p >= 0) ? (p >> NBR_BITS_PER_OCT) : 0;
    }

    inline void ResamplerFlt::reset_cur_voice()
    {
        BaseVoiceState& v = _v[CUR];
        v._table = table_from_pitch(_pitch);
        v._ovrspl_flag = (_pitch >= 0);

        v._cycle_len = FRAME_LEN >> v._table;
        v._cycle_mask = v._cycle_len - 1U;

        v._frame_idx = _target_frame;
        v._table_ptr = _set->use_table(v._table, v._frame_idx);

        v.compute_step(_pitch);
    }

    /*-------------- pitch ---------------*/
    inline void ResamplerFlt::set_pitch(long p)
    {
        assert(_set && _ip);
        assert(p < _set->get_nbr_tables() * (1L << NBR_BITS_PER_OCT));

        _pitch = p;
        bool need = (table_from_pitch(p) != _v[CUR]._table) ||
            ((_pitch >= 0) != _v[CUR]._ovrspl_flag);
        if (need) _fade_needed = true;

        _v[CUR].compute_step(_pitch);
        if (_fade_flag) _v[FADE].compute_step(_pitch);
    }

    /*-------------- frame ---------------*/
    inline void ResamplerFlt::set_frame(UInt32 f)
    {
        f &= (FRAME_COUNT - 1);
        if (f == _target_frame) return;
        _target_frame = f;
        _fade_needed = true;
    }

    /*----------- playback pos -----------*/
    inline void ResamplerFlt::set_playback_pos(Int64 pos)
    {
        _v[CUR]._pos._all = pos >> _v[CUR]._table;
        if (_fade_flag) _v[FADE]._pos._all = pos >> _v[FADE]._table;
    }

    inline Int64 ResamplerFlt::get_playback_pos() const
    {
        return (_v[CUR]._pos._all << _v[CUR]._table);
    }

    /*-------------- fade ---------------*/
    inline void ResamplerFlt::begin_fade()
    {
        _v[FADE] = _v[CUR];          /* copy old voice */
        reset_cur_voice();           /* rebuild CUR with new state */

        int d = _v[FADE]._table - _v[CUR]._table;
        _v[CUR]._pos._all = shift_bidi(_v[FADE]._pos._all, d);

        _fade_flag = true;
        _fade_pos = 0;
        _fade_needed = false;
    }

    inline void ResamplerFlt::fade_block(float* dst, long n)
    {
        const long n2 = n * 2;
        const float step = 1.0f / (BaseVoiceState::FADE_LEN * 2);
        const float vol = _fade_pos * step * 2;

        memset(_buf.data(), 0, sizeof(_buf[0]) * n2);

        _ip->interp_ovrspl_ramp_add(_buf.data(), n2, _v[CUR], vol, step);
        _ip->interp_ovrspl_ramp_add(_buf.data(), n2, _v[FADE], 1.0f - vol, -step);

        _dwn.downsample_block(dst, _buf.data(), n);

        _fade_pos += n;
        _fade_flag = (_fade_pos < BaseVoiceState::FADE_LEN);
    }

    /*------------- render --------------*/
    inline void ResamplerFlt::interpolate_block(float* dst, long n)
    {
        assert(_set && _ip && dst && n > 0);

        if (_fade_needed && !_fade_flag) begin_fade();

        long pos = 0;
        while (pos < n)
        {
            long work = n - pos;

            if (_fade_flag)
            {
                work = min(work, _buf_len);
                work = min(work, BaseVoiceState::FADE_LEN - _fade_pos);
                fade_block(dst + pos, work);
            }
            else if (_v[CUR]._ovrspl_flag)
            {
                work = min(work, _buf_len);
                _ip->interp_ovrspl(_buf.data(), work * 2, _v[CUR]);
                _dwn.downsample_block(dst + pos, _buf.data(), work);
            }
            else
            {
                _ip->interp_norm(dst + pos, work, _v[CUR]);
                _dwn.phase_block(dst + pos, dst + pos, work);
            }
            pos += work;
        }
    }

    /*------------ clear ----------------*/
    inline void ResamplerFlt::clear_buffers()
    {
        _dwn.clear_buffers();
        if (_set) reset_cur_voice();
        _fade_flag = _fade_needed = false;
    }

} /* namespace rspl */

#endif /* RSPL_RESAMPLERFLT_H */
