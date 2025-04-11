#pragma once
#include <JuceHeader.h>
#include <cstring>

#if defined (_MSC_VER)
#pragma warning (4 : 4786) // "identifier was truncated"
#pragma warning (4 : 4800) // "forcing value to bool 'true' or 'false'"
#endif

#include "src/griffinwave2/rspl.h"
#include "src/griffinwave2/rspl_basevoicestate.h"
#include "src/griffinwave2/rspl_downsampler2flt.h"
#include "src/griffinwave2/rspl_interp.h"       // Modified to use bitmasked looping
#include "src/griffinwave2/rspl_mipmap.h"
#include "src/griffinwave2/rspl_resamplerflt.h"

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
    // Real-time saw example with EXACT loop logic using bit-masked looping.
    // The full wavetable contains 256 cycles. Only the first cycle is generated as a saw,
    // and the remaining 255 cycles are generated as sine.
    // For playback, we want to loop strictly over the first saw cycle (i.e. 2048 samples).
    // The new library uses bitmasking for loop wrapping.
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
        // The sawSignal holds the full waveform table in the new format (no duplicate padding).
        std::vector<float> sawSignal;
        rspl::MipMapFlt    mipMap;
        rspl::InterpPack   interpPack;
        rspl::ResamplerFlt resampler;

        // Single-cycle length for each waveform (2048 samples).
        const long wavelength = 1L << 11; // 2048

        // Total number of cycles in the waveform table.
        const long totalCycles = 256;

        // We want only one cycle to be a saw.
        const long sawCycles = 1;
        // The remaining cycles will be sine.
        const long sineCycles = totalCycles - sawCycles; // 255

        // The saw section length in the table (in samples).
        long sawSectionLen = sawCycles * wavelength; // 1 * 2048 = 2048

        // Playback will loop over a single saw cycle.
        const long loopCycles = 1;
        const long loopRegionLength = loopCycles * wavelength; // 2048

        // Total waveform table length (no extra padding).
        long totalTableLen = totalCycles * wavelength;  // 256 * 2048

        // With bitmask looping we don't need extra padding.
        const long extraPadding = 0;

        // User parameters.
        float currentPitchParameter = 0.0f; // in octaves.
        float volume = 1.0f;

        Griffin_WT() {}

        //===============================================================
        // Prepare: Build a temporary waveform table without extra padding.
        // The first 'sawCycles' (1 cycle) is generated as a saw waveform (with 80% headroom)
        // and the remaining 'sineCycles' (255 cycles) are generated as sine.
        //===============================================================
        void prepare(PrepareSpecs specs)
        {
            sawSectionLen = sawCycles * wavelength;
            totalTableLen = totalCycles * wavelength;
            const long sampleLength = totalTableLen;  // no extra padding

            // Temporary buffer (entire waveform, unpadded).
            std::vector<float> waveform(totalTableLen);

            // Generate the saw waveform for the first saw cycle.
            const float headroom = 0.8f;
            {
                double saw_val = -1.0;
                double saw_step = 2.0 / (static_cast<double>(wavelength) - 1.0);
                for (long sample = 0; sample < wavelength; ++sample)
                {
                    waveform[sample] = static_cast<float>(headroom * saw_val);
                    saw_val += saw_step;
                }
            }

            // Generate the sine waveform for the remaining sineCycles.
            {
                for (long cycle = 0; cycle < sineCycles; ++cycle)
                {
                    for (long sample = 0; sample < wavelength; ++sample)
                    {
                        const long idx = sawCycles * wavelength + cycle * wavelength + sample;
                        const double phase = (2.0 * M_PI * sample) / wavelength;
                        waveform[idx] = static_cast<float>(std::sin(phase));
                    }
                }
            }

            // Create the full buffer without extra padding.
            sawSignal.resize(sampleLength);
            for (long i = 0; i < sampleLength; i++)
            {
                sawSignal[i] = waveform[i];
            }

            // Set up the mipMap from the unpadded buffer.
            mipMap.init_sample(
                sampleLength,
                rspl::InterpPack::get_len_pre(),
                rspl::InterpPack::get_len_post(),
                12,
                rspl::ResamplerFlt::_fir_mip_map_coef_arr,
                rspl::ResamplerFlt::MIP_MAP_FIR_LEN
            );
            mipMap.fill_sample(sawSignal.data(), sampleLength);

            // Connect the resampler.
            resampler.set_sample(mipMap);
            resampler.set_interp(interpPack);
            resampler.clear_buffers();
        }

        void reset()
        {
            resampler.clear_buffers();
        }

        //===============================================================
        // Process: Loop playback strictly over the first saw cycle.
        // The loop region in the waveform buffer is [0, 2048).
        // The new library uses bitmask wrapping so the FIR interpolation will
        // automatically wrap from index 2047 back to 0.
        //===============================================================
        template <typename ProcessDataType>
        void process(ProcessDataType& data)
        {
            auto& fixData = data.template as<ProcessData<getFixChannelAmount()>>();
            auto audioBlock = fixData.toAudioBlock();
            float* leftChannelData = audioBlock.getChannelPointer(0);
            float* rightChannelData = audioBlock.getChannelPointer(1);
            const int numSamples = data.getNumSamples();

            // Convert pitch parameter to fixed-point.
            const long fixedPitch = rspl::round_long(
                currentPitchParameter * (1 << rspl::ResamplerFlt::NBR_BITS_PER_OCT)
            );
            resampler.set_pitch(fixedPitch);

            // Get the current fixed-point playback position.
            rspl::Int64 pos = resampler.get_playback_pos();
            long integerPos = static_cast<long>(pos >> 32);
            DBG("Before reset, integerPos = " + String(integerPos));

            // With extraPadding=0, ensure the playback pointer is within [0, 2048).
            if (integerPos < 0 || integerPos >= loopRegionLength)
            {
                // Wrap the fixed-point position within loopRegionLength.
                rspl::Int64 wrappedPart = pos & ((static_cast<rspl::Int64>(loopRegionLength) << 32) - 1);
                rspl::Int64 newPos = wrappedPart;  // no offset needed
                DBG("Resetting playback pos. New integerPos = " + String(static_cast<long>(newPos >> 32)));
                resampler.set_playback_pos(newPos);
            }

            // Generate an intermediate audio block.
            std::vector<float> temp(numSamples, 0.0f);
            resampler.interpolate_block(temp.data(), numSamples);

            // Apply volume and output to both channels.
            for (int i = 0; i < numSamples; ++i)
            {
                float s = temp[i] * volume;
                leftChannelData[i] = s;
                rightChannelData[i] = s;
            }
        }

        //===============================================================
        // Parameter handling.
        //===============================================================
        template <int P>
        void setParameter(double v)
        {
            if (P == 0) { volume = static_cast<float>(v); }
            else if (P == 1) { currentPitchParameter = static_cast<float>(v); }
        }

        // Define parameters.
        void createParameters(ParameterDataList& data)
        {
            {
                parameter::data p("Volume", { 0.0, 1.0, 0.0001 });
                p.setDefaultValue(0.35);
                registerCallback<0>(p);
                data.add(std::move(p));
            }
            {
                parameter::data p("Pitch", { 0.0, 9.0, 0.0001 });
                p.setDefaultValue(0.0);
                registerCallback<1>(p);
                data.add(std::move(p));
            }
        }

        void handleHiseEvent(HiseEvent& e) {}
        SN_EMPTY_PROCESS_FRAME;
    };

} // namespace project
