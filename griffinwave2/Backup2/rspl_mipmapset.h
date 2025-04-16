/*====================================================================
    rspl_mipmapset.h – thin container holding 256 independent
    per?frame mip?maps.  It re?uses rspl::MipMapFlt unchanged.
====================================================================*/
#ifndef RSPL_MIPMAPSET_H
#define RSPL_MIPMAPSET_H

#include <array>
#include <vector>

namespace rspl
{
    template <int FRAMES, long FRAME_STRIDE>
    class MipMapSet
    {
    public:
        MipMapSet() = default;

        /*------------------------------------------------------------
            build()

            1. wavetable points to the beginning of frame 0.
            2. Each frame occupies FRAME_STRIDE samples
               (main cycle + any user padding).
        ------------------------------------------------------------*/
        void build(const float* wavetable,
            long        frame_len,
            long        add_len_pre,
            long        add_len_post,
            int         nbr_tables,
            const double* fir,
            int         fir_len)
        {
            _lev_len.resize(nbr_tables);
            for (int t = 0; t < nbr_tables; ++t)
                _lev_len[t] = (frame_len + ((1L << t) - 1)) >> t;

            for (int f = 0; f < FRAMES; ++f)
            {
                const float* src = wavetable + f * FRAME_STRIDE;

                MipMapFlt& mm = _frames[f];
                bool need = mm.init_sample(frame_len,
                    add_len_pre,
                    add_len_post,
                    nbr_tables,
                    fir,
                    fir_len);
                /* full frame provided at once, so need must be false */
                (void)need; /* silence unused variable warning */
                mm.fill_sample(src, frame_len);
            }
        }

        /* fast access */
        inline const float* use_table(int level, int frame) const
        {
            return _frames[frame].use_table(level);
        }

        inline long get_lev_len(int level) const
        {
            return _lev_len[level];
        }

        inline int get_nbr_tables() const
        {
            return static_cast<int>(_lev_len.size());
        }

    private:
        std::array<MipMapFlt, FRAMES> _frames;
        std::vector<long>             _lev_len;
    };
} /* namespace rspl */

#endif /* RSPL_MIPMAPSET_H */
