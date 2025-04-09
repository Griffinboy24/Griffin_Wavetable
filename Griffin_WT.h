#pragma once
#include <JuceHeader.h>
#include <cstring>

#if defined (_MSC_VER)
#pragma warning (4 : 4786) // "identifier was truncated"
#pragma warning (4 : 4800) // "forcing value to bool 'true' or 'false'"
#endif


#include "src\griffinwave\rspl.h"
#include "src\griffinwave\rspl_basevoicestate.h"
#include "src\griffinwave\rspl_default_coefs.h"
#include "src\griffinwave\rspl_downsampler2flt.h"
#include "src\griffinwave\rspl_interp.h"
#include "src\griffinwave\rspl_mipmap.h"
#include "src\griffinwave\rspl_resamplerflt.h"


#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <streambuf>
#include <vector>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cmath> // required for sin()

namespace project
{

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

    using namespace juce;
    using namespace hise;
    using namespace scriptnode;

    //===============================================================
    // Real-time saw example with EXACT test_saw() wrap logic, using two
    // padded waveforms: a saw (first half) and then a sine (second half). 
    // We only use the saw in playback.
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
        std::vector<float> sawSignal;  // Now holds both waveforms consecutively.
        rspl::MipMapFlt    mipMap;
        rspl::InterpPack   interpPack;
        rspl::ResamplerFlt resampler;

        // We replicate test_saw from the original main.cpp:
        //   wavelength must be a power of two,
        //   block_len = 256 (arbitrary),
        //   we build two sections by copying a cycle repeatedly.
        // Single-cycle length for each waveform is now 2048 samples.
        const long wavelength = 1L << 11; // 2048
        long tableSectionLen = 0;         // length of one waveform section (saw or sine)

        // Our user parameters
        float currentPitchParameter = 0.0f; // in octaves
        float volume = 1.0f;

        Griffin_WT() {}

        //===============================================================
        // Prepare: replicate the test_saw approach but create two padded sections.
        // First half is a saw (with headroom) and second half is sine.
        //===============================================================
        void prepare(PrepareSpecs specs)
        {
            const long blockLen = 256;  // number of cycles per waveform section
            tableSectionLen = wavelength * blockLen; // length for one waveform type
            const long totalTableLen = tableSectionLen * 2;  // two sections: saw then sine

            // Resize wavetable to hold both waveforms
            sawSignal.resize(totalTableLen);

            // Generate saw waveform (first half) with headroom (e.g. 80% of full amplitude)
            const float headroom = 0.8f;
            {
                double saw_val;
                const double saw_step = 2.0 / (static_cast<double>(wavelength) - 1.0);
                // Repeat single-cycle saw wave for blockLen cycles.
                for (long cycle = 0; cycle < blockLen; ++cycle)
                {
                    saw_val = -1.0;
                    for (long sample = 0; sample < wavelength; ++sample)
                    {
                        const long idx = cycle * wavelength + sample;
                        sawSignal[idx] = static_cast<float>(headroom * saw_val);
                        saw_val += saw_step;
                    }
                }
            }

            // Generate sine waveform (second half) padded identically.
            {
                for (long cycle = 0; cycle < blockLen; ++cycle)
                {
                    for (long sample = 0; sample < wavelength; ++sample)
                    {
                        const long idx = tableSectionLen + cycle * wavelength + sample;
                        const double phase = (2.0 * M_PI * sample) / wavelength;
                        sawSignal[idx] = static_cast<float>(std::sin(phase));
                    }
                }
            }

            // MipMap: 12 levels -> up to 10 octaves.
            // Note: totalTableLen is used now.
            mipMap.init_sample(
                totalTableLen,
                rspl::InterpPack::get_len_pre(),
                rspl::InterpPack::get_len_post(),
                12, // same as test_saw
                rspl::ResamplerFlt::_fir_mip_map_coef_arr,
                rspl::ResamplerFlt::MIP_MAP_FIR_LEN
            );
            mipMap.fill_sample(sawSignal.data(), totalTableLen);

            // Hook up resampler.
            resampler.set_sample(mipMap);
            resampler.set_interp(interpPack);
            resampler.clear_buffers();
        }

        void reset()
        {
            resampler.clear_buffers();
        }

        //===============================================================
        // Process: EXACT looping logic from test_saw.
        // We force looping within the first section (saw) only.
        // Debug prints via VS debug console are coded (commented out).
        //===============================================================
        template <typename ProcessDataType>
        void process(ProcessDataType& data)
        {
            auto& fixData = data.template as<ProcessData<getFixChannelAmount()>>();
            auto audioBlock = fixData.toAudioBlock();
            float* leftChannelData = audioBlock.getChannelPointer(0);
            float* rightChannelData = audioBlock.getChannelPointer(1);
            const int numSamples = data.getNumSamples();

            // Convert pitch: we are going from -2..+10, or whatever you do.
            const long fixedPitch = rspl::round_long(
                currentPitchParameter * (1 << rspl::ResamplerFlt::NBR_BITS_PER_OCT)
            );
            resampler.set_pitch(fixedPitch);

            // EXACT test_saw loop-check:
            // We use only the saw section (first half), so if pos > (tableSectionLen >> 1),
            // do the bitmask and skip 16 periods.
            rspl::Int64 pos = resampler.get_playback_pos();
            if ((pos >> 32) > (tableSectionLen >> 1))
            {
                // Debug print for VS debug console (commented out)
                // OutputDebugStringA("Looping wavetable - saw section reset\n");
                // Reset position: 1) keep fractional bits only up to 'wavelength'
                pos &= ((static_cast<rspl::Int64>(wavelength) << 32) - 1);
                // 2) skip 16 periods (saw cycles)
                pos += (static_cast<rspl::Int64>(wavelength) * 16) << 32;
                resampler.set_playback_pos(pos);
            }

            // Grab the block into a temporary buffer.
            std::vector<float> temp(numSamples, 0.0f);
            resampler.interpolate_block(temp.data(), numSamples);

            // Apply volume & output.
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

        // Example: 2 parameters -> volume & pitch.
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
