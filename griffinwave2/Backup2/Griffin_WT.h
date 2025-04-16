#pragma once
#include <JuceHeader.h>
#include <cstring>

#if defined (_MSC_VER)
#pragma warning (4 : 4786) // "identifier was truncated"
#pragma warning (4 : 4800) // "forcing value to bool 'true' or 'false'"
#endif

/*===============================================================
  External rspl headers (unchanged)
===============================================================*/
#include "src\\griffinwave2\\rspl_big_arrays.cpp"
#include "src\\griffinwave2\\rspl_big_arrays.h"
#include "src\\griffinwave2\\rspl.h"
#include "src\\griffinwave2\\rspl_basevoicestate.h"
#include "src\\griffinwave2\\rspl_downsampler2flt.h"
#include "src\\griffinwave2\\rspl_interp.h"
#include "src\\griffinwave2\\rspl_mipmap.h"
#include "src\\griffinwave2\\rspl_resamplerflt.h"

#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <streambuf>
#include <vector>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cmath> // for sin()

namespace project
{

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

    using namespace juce;
    using namespace hise;
    using namespace scriptnode;

    /*===============================================================
      Griffin_WT
    ===============================================================*/
    template <int NV>
    struct Griffin_WT : public data::base
    {
        SNEX_NODE(Griffin_WT);

        struct MetadataClass
        {
            SN_NODE_ID("Griffin_WT");
        };

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

        /*---------------------------------------------------------------
          Resampler, mip?map and wavetable fields
        ---------------------------------------------------------------*/
        std::vector<float> wavetable;
        rspl::MipMapFlt    mipMap;
        rspl::InterpPack   interpPack;
        rspl::ResamplerFlt resampler;

        /*---------------------------------------------------------------
          Wavetable specification
        ---------------------------------------------------------------*/
        static constexpr long baseCycleLen = 1L << 11;      // 2048
        static constexpr long halfCycle = baseCycleLen >> 1;
        const long numSawCycles = 1;
        const long numSineCycles = 255;
        const long totalCycles = numSawCycles + numSineCycles;
        const long paddedCycleLen = baseCycleLen + halfCycle; // 3072
        long       totalTableLen = totalCycles * paddedCycleLen;

        /*---------------------------------------------------------------
          Parameters
        ---------------------------------------------------------------*/
        float currentPitchParameter = 0.0f; // in octaves
        float volume = 1.0f;

        /*---------------------------------------------------------------
          Ctor / prepare / reset
        ---------------------------------------------------------------*/
        Griffin_WT() {}

        void prepare(PrepareSpecs specs)
        {
            totalTableLen = totalCycles * paddedCycleLen;
            wavetable.resize(totalTableLen);

            /* first cycle = saw */
            {
                const float headroom = 0.8f;
                const double saw_step = 2.0 / (static_cast<double>(baseCycleLen) - 1.0);
                double saw_val = -1.0;
                for (long s = 0; s < baseCycleLen; ++s)
                {
                    wavetable[s] = static_cast<float>(headroom * saw_val);
                    saw_val += saw_step;
                }
                /* pad first half */
                for (long s = 0; s < halfCycle; ++s)
                    wavetable[baseCycleLen + s] = wavetable[s];
            }

            /* remaining cycles = sine */
            for (long c = 1; c < totalCycles; ++c)
            {
                long offset = c * paddedCycleLen;
                for (long s = 0; s < baseCycleLen; ++s)
                {
                    const double phase = (2.0 * M_PI * s) / baseCycleLen;
                    wavetable[offset + s] = static_cast<float>(std::sin(phase));
                }
                for (long s = 0; s < halfCycle; ++s)
                    wavetable[offset + baseCycleLen + s] = wavetable[offset + s];
            }

            /* build mip?map */
            mipMap.init_sample(
                totalTableLen,
                rspl::InterpPack::get_len_pre(),
                rspl::InterpPack::get_len_post(),
                12,
                rspl::MIP_MAP_FIR_COEF_ARR,
                rspl::ResamplerFlt::MIP_MAP_FIR_LEN);
            mipMap.fill_sample(wavetable.data(), totalTableLen);

            resampler.set_sample(mipMap);
            resampler.set_interp(interpPack);
            resampler.clear_buffers();
        }

        void reset() { resampler.clear_buffers(); }

        /*---------------------------------------------------------------
          process – uses per?voice masking, no manual wrapping
        ---------------------------------------------------------------*/
        template <typename ProcessDataType>
        void process(ProcessDataType& data)
        {
            auto& fix = data.template as<ProcessData<getFixChannelAmount()>>();
            auto block = fix.toAudioBlock();
            float* L = block.getChannelPointer(0);
            float* R = block.getChannelPointer(1);
            const int n = data.getNumSamples();

            const long fixedPitch = rspl::round_long(
                currentPitchParameter * (1 << rspl::ResamplerFlt::NBR_BITS_PER_OCT));
            resampler.set_pitch(fixedPitch);

            std::vector<float> tmp(n, 0.0f);
            resampler.interpolate_block(tmp.data(), n);

            for (int i = 0; i < n; ++i)
            {
                float s = tmp[i] * volume;
                L[i] = s;
                R[i] = s;
            }
        }

        /*---------------------------------------------------------------
          Parameter handling
        ---------------------------------------------------------------*/
        template <int P>
        void setParameter(double v)
        {
            if (P == 0)        volume = static_cast<float>(v);
            else if (P == 1)   currentPitchParameter = static_cast<float>(v);
        }

        void createParameters(ParameterDataList& data)
        {
            {
                parameter::data p("Volume", { 0.0, 1.0, 0.001 });
                p.setDefaultValue(0.4);
                registerCallback<0>(p);
                data.add(std::move(p));
            }
            {
                parameter::data p("Pitch", { -2.0, 10.0, 0.01 });
                p.setDefaultValue(0.0);
                registerCallback<1>(p);
                data.add(std::move(p));
            }
        }

        void handleHiseEvent(HiseEvent& e) {}
        SN_EMPTY_PROCESS_FRAME;
    };

} // namespace project
