#pragma once
#include <JuceHeader.h>
#include <cstring>

#if defined (_MSC_VER)
#pragma warning (4 : 4786) // "identifier was truncated"
#pragma warning (4 : 4800) // "forcing value to bool 'true' or 'false'"
#endif

#include "src/resampler/def.h"
#include "src/resampler/Downsampler2Flt.h"
#include "src/resampler/fnc.h"
#include "src/resampler/Fixed3232.h"
#include "src/resampler/Int16.h"
#include "src/resampler/Int64.h"
#include "src/resampler/InterpFlt.h"
#include "src/resampler/InterpPack.h"
#include "src/resampler/MipMapFlt.h"
#include "src/resampler/ResamplerFlt.h"
#include "src/resampler/BaseVoiceState.h"

// Implementation .cpp files (you can compile them separately if you prefer)
#include "src/resampler/BaseVoiceState.cpp"
#include "src/resampler/Downsampler2Flt.cpp"
#include "src/resampler/InterpPack.cpp"
#include "src/resampler/MipMapFlt.cpp"
#include "src/resampler/ResamplerFlt.cpp"

#include "src/resampler/Downsampler2Flt.hpp"
#include "src/resampler/InterpFlt.hpp"
#include "src/resampler/InterpFltPhase.hpp"
#include "src/resampler/MipMapFlt.hpp"

#if defined (_MSC_VER)
#include <crtdbg.h>
#include <new.h>
#endif

#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <streambuf>
#include <vector>
#include <cassert>
#include <climits>
#include <cstdlib>

namespace project
{

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

    using namespace juce;
    using namespace hise;
    using namespace scriptnode;

    //===============================================================
    // Real-time saw example with EXACT test_saw() wrap logic
    //===============================================================
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
        static constexpr int getFixChannelAmount() { return 2; }
        static constexpr int NumTables = 0;
        static constexpr int NumSliderPacks = 0;
        static constexpr int NumAudioFiles = 0;
        static constexpr int NumFilters = 0;
        static constexpr int NumDisplayBuffers = 0;
        //===============================================================
        // Resampler fields
        //===============================================================
        std::vector<float> sawSignal;
        rspl::MipMapFlt    mipMap;
        rspl::InterpPack   interpPack;
        rspl::ResamplerFlt resampler;

        // We replicate test_saw from the original main.cpp:
        //   wavelength must be a power of two,
        //   block_len = 57 (arbitrary),
        //   we build saw_len = wavelength * block_len * 4
        const long wavelength = 1L << 10; // 1024
        long sawLen = 0;

        // Our user parameters
        float currentPitchParameter = 0.0f; // in octaves
        float volume = 1.0f;

        Griffin_WT() {}

        //===============================================================
        // Prepare: replicate the "test_saw" approach
        //===============================================================
        void prepare(PrepareSpecs specs)
        {
            const long blockLen = 256;
            sawLen = wavelength * blockLen; // same as test_saw

            // Build the saw wave (just like generate_steady_saw)
            sawSignal.resize(sawLen);
            {
                double val = -1.0;
                const double step = 2.0 / (static_cast<double>(wavelength) - 1.0);
                for (long i = 0; i < sawLen; ++i)
                {
                    if ((i % wavelength) == 0)
                        val = -1.0;
                    sawSignal[i] = static_cast<float>(val);
                    val += step;
                }
            }

            // MipMap: 12 levels -> up to 10 octaves
            mipMap.init_sample(
                sawLen,
                rspl::InterpPack::get_len_pre(),
                rspl::InterpPack::get_len_post(),
                12, // same as test_saw
                rspl::ResamplerFlt::_fir_mip_map_coef_arr,
                rspl::ResamplerFlt::MIP_MAP_FIR_LEN
            );
            mipMap.fill_sample(sawSignal.data(), sawLen);

            // Hook up resampler
            resampler.set_sample(mipMap);
            resampler.set_interp(interpPack);
            resampler.clear_buffers();
        }

        void reset()
        {
            resampler.clear_buffers();
        }

        //===============================================================
        // Process: EXACT looping logic from test_saw
        //===============================================================
        template <typename ProcessDataType>
        void process(ProcessDataType& data)
        {
            auto& fixData = data.template as<ProcessData<getFixChannelAmount()>>();
            auto audioBlock = fixData.toAudioBlock();
            float* leftChannelData = audioBlock.getChannelPointer(0);
            float* rightChannelData = audioBlock.getChannelPointer(1);
            const int numSamples = data.getNumSamples();

            // Convert pitch: we are going from -2..+10, or whatever you do
            const long fixedPitch = rspl::round_long(
                currentPitchParameter * (1 << rspl::ResamplerFlt::NBR_BITS_PER_OCT)
            );
            resampler.set_pitch(fixedPitch);

            // We replicate test_saw EXACTLY:
            //   if pos > (saw_len >> 1), do the bitmask and skip 16 periods
            rspl::Int64 pos = resampler.get_playback_pos();
            if ((pos >> 32) > (sawLen >> 1))
            {
                // same logic as test_saw
                // 1) keep fractional bits only up to 'wavelength'
                pos &= ((static_cast<rspl::Int64>(wavelength) << 32) - 1);
                // 2) skip 16 periods
                pos += (static_cast<rspl::Int64>(wavelength) * 16) << 32;
                resampler.set_playback_pos(pos);
            }

            // Grab the block
            std::vector<float> temp(numSamples, 0.0f);
            resampler.interpolate_block(temp.data(), numSamples);

            // Volume & output
            for (int i = 0; i < numSamples; ++i)
            {
                float s = temp[i] * volume;
                leftChannelData[i] = s;
                rightChannelData[i] = s;
            }
        }

        //===============================================================
        // Parameter Handling
        //===============================================================
        template <int P>
        void setParameter(double v)
        {
            if (P == 0) { volume = static_cast<float>(v); }
            else if (P == 1) { currentPitchParameter = static_cast<float>(v); }
        }

        // Example: 2 parameters -> volume & pitch
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
