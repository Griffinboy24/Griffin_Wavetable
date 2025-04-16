/*====================================================================
    Griffin_WT.h – uses the new per?frame mip?map set.
====================================================================*/
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <cstring>


#include "src\\griffinwave2\\rspl_big_arrays.cpp"
#include "src\\griffinwave2\\rspl_big_arrays.h"
#include "src\\griffinwave2\\rspl.h"
#include "src\\griffinwave2\\rspl_basevoicestate.h"
#include "src\\griffinwave2\\rspl_downsampler2flt.h"
#include "src\\griffinwave2\\rspl_interp.h"
#include "src\\griffinwave2\\rspl_mipmap.h"
#include "src\\griffinwave2\\rspl_mipmapset.h"
#include "src\\griffinwave2\\rspl_resamplerflt.h"

/*============================================================================*/
namespace project
{
    using namespace juce;
    using namespace hise;
    using namespace scriptnode;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

    template <int NV>
    struct Griffin_WT : public data::base
    {
        SNEX_NODE(Griffin_WT);
        struct MetadataClass { SN_NODE_ID("Griffin_WT"); };

        static constexpr bool isModNode() { return false; }
        static constexpr bool isPolyphonic() { return NV > 1; }
        static constexpr bool hasTail() { return false; }
        static constexpr bool isSuspendedOnSilence() { return false; }
        static constexpr int  getFixChannelAmount() { return 2; }
        static constexpr int  NumTables = 0;
        static constexpr int  NumSliderPacks = 0;
        static constexpr int  NumAudioFiles = 0;
        static constexpr int  NumFilters = 0;
        static constexpr int  NumDisplayBuffers = 0;

        /* constants */
        static constexpr long FRAME_LEN = rspl::ResamplerFlt::FRAME_LEN;
        static constexpr long FRAME_PAD = rspl::ResamplerFlt::FRAME_PAD;
        static constexpr long FRAME_STRIDE = rspl::ResamplerFlt::FRAME_STRIDE;
        static constexpr int  FRAME_COUNT = rspl::ResamplerFlt::FRAME_COUNT;

        /* rspl objects */
        std::vector<float> wavetable;
        rspl::InterpPack   interp;
        rspl::ResamplerFlt res;
        rspl::ResamplerFlt::SampleSet mipset;

        /* parameters */
        float vol_param = 1.0f;
        float pitch_param = 0.0f;
        int   frame_param = 0;

        Griffin_WT() {}

        /*---------------- prepare ----------------*/
        void prepare(PrepareSpecs)
        {
            /* generate table: saw frame 0, pure sin frames 1..255 */
            wavetable.resize(FRAME_COUNT * FRAME_STRIDE);
            for (int f = 0; f < FRAME_COUNT; ++f)
            {
                float* dst = &wavetable[f * FRAME_STRIDE];

                /* main 2048?sample cycle */
                if (f == 0)
                {
                    const double step = 2.0 / (FRAME_LEN - 1.0);
                    double v = -1.0;
                    for (int s = 0; s < FRAME_LEN; ++s, v += step)
                        dst[s] = static_cast<float>(v);
                }
                else
                {
                    for (int s = 0; s < FRAME_LEN; ++s)
                        dst[s] = static_cast<float>(
                            sin(2.0 * M_PI * s / FRAME_LEN));
                }

                /* pad: copy first 1024 samples */
                std::memcpy(dst + FRAME_LEN, dst, FRAME_PAD * sizeof(float));
            }

            /* build independent mip?maps */
            mipset.build(wavetable.data(),
                FRAME_STRIDE,          /* len (cycle+pad) */
                rspl::InterpPack::get_len_pre(),
                rspl::InterpPack::get_len_post(),
                12,
                rspl::MIP_MAP_FIR_COEF_ARR,
                rspl::ResamplerFlt::MIP_MAP_FIR_LEN);

            res.set_interp(interp);
            res.set_sample(mipset);
        }

        void reset() { res.clear_buffers(); }

        /*---------------- process ---------------*/
        template <typename PD>
        void process(PD& data)
        {
            auto& fix = data.template as<ProcessData<getFixChannelAmount()>>();
            auto blk = fix.toAudioBlock();
            float* L = blk.getChannelPointer(0);
            float* R = blk.getChannelPointer(1);
            int n = data.getNumSamples();

            long fpitch = rspl::round_long(
                pitch_param * (1 << rspl::ResamplerFlt::NBR_BITS_PER_OCT));
            res.set_pitch(fpitch);
            res.set_frame(static_cast<rspl::UInt32>(frame_param));

            std::vector<float> tmp(n);
            res.interpolate_block(tmp.data(), n);

            for (int i = 0; i < n; ++i)
            {
                float s = tmp[i] * vol_param;
                L[i] = s;
                R[i] = s;
            }
        }

        /*------------- parameters --------------*/
        template <int P> void setParameter(double v)
        {
            if (P == 0)      vol_param = static_cast<float>(v);
            else if (P == 1) pitch_param = static_cast<float>(v);
            else if (P == 2) frame_param = static_cast<int>(v);
        }

        void createParameters(ParameterDataList& d)
        {
            {
                parameter::data p("Volume", { 0.0, 1.0, 0.001 });
                p.setDefaultValue(0.4); registerCallback<0>(p); d.add(std::move(p));
            }
            {
                parameter::data p("Pitch", { -2.0, 10.0, 0.01 });
                p.setDefaultValue(0.0); registerCallback<1>(p); d.add(std::move(p));
            }
            {
                parameter::data p("Frame", { 0.0, 255.0, 1.0 });
                p.setDefaultValue(0.0); registerCallback<2>(p); d.add(std::move(p));
            }
        }

        void handleHiseEvent(HiseEvent&) {}
        SN_EMPTY_PROCESS_FRAME;
    };
} /* namespace project */
