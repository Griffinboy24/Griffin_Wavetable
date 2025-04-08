#pragma once
#include <JuceHeader.h>
#include <cstring>

#if defined (_MSC_VER)
#pragma warning (4 : 4786) // "identifier was truncated to '255' characters in the debug information"
#pragma warning (4 : 4800) // "forcing value to bool 'true' or 'false' (performance warning)"
#endif

#include "src/griffinwave/rspl.h"
#include "src/griffinwave/rspl_basevoicestate.h"
#include "src/griffinwave/rspl_downsampler2flt.h"
#include "src/griffinwave/rspl_interp.h"
#include "src/griffinwave/rspl_mipmap.h"
#include "src/griffinwave/rspl_resamplerflt.h"
#include "src/griffinwave/rspl_stopwatch.h"
#include "src/griffinwave/rspl_default_coefs.h" // Contains full recommended coefficient arrays

#if defined (_MSC_VER)
#include <crtdbg.h>
#include <new.h>
#endif // _MSC_VER

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

    namespace Uniquename {
        // No additional functions needed here
    }

    // Use this enum to refer to the cables.
    enum class GlobalCables
    {
        cbl_WT1 = 0
    };

    // Subclass your node from this cable manager.
    using cable_manager_t = routing::global_cable_cpp_manager<SN_GLOBAL_CABLE(576864674)>;

    // Conversion helper: convert semitone pitch (float) to RSPL fixed–point pitch.
    // RSPL: 0 means original, 0x10000 means one octave up.
    inline long semitoneToRsplPitch(float semitones)
    {
        return static_cast<long>((semitones / 12.0f) * 65536.0f);
    }

    template <int NV>
    struct Griffin_WT : public data::base, public cable_manager_t
    {
        SNEX_NODE(Griffin_WT);

        struct MetadataClass
        {
            SN_NODE_ID("Griffin_WT");
        };

        // Node Properties
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

        // RSPL Resampling Classes
        rspl::MipMapFlt   mipMap;       // Holds the sample (single cycle waveform)
        rspl::InterpPack  interpPack;   // Interpolator helper
        rspl::ResamplerFlt resampler;    // Combines mip–mapping, interpolation and downsampling

        // New parameters:
        float pitchSemitones = 0.0f;    // Pitch, in semitones (e.g., 0.0 is natural; positive for up; negative for down)
        float volume = 1.0f;            // Volume control

        // Sample length for a single cycle (assumed 2048 samples)
        static constexpr int sampleLength = 2048;
        // Number of mip–map levels per octave. For example, to support about 6 octaves (–36 to +36 semitones),
        // you might use 7 levels (level 0 is the original, then levels 1–6 are downsampled each by 2).
        static constexpr int nbrMipLevels = 7;

        Griffin_WT()
        {
            // Register global cable callback to update the sample waveform.
            this->registerDataCallback<GlobalCables::cbl_WT1>([this](const var& data)
                {
                    if (data.isArray())
                    {
                        if (data.size() == sampleLength)
                        {
                            std::vector<float> tempBuffer(sampleLength);
                            for (int i = 0; i < sampleLength; ++i)
                            {
                                DBG(String(i) + ": " + data[i].toString());
                                tempBuffer[i] = static_cast<float>(data[i]);
                            }
                            // Initialize the MIP–map if not already ready.
                            if (!mipMap.is_ready())
                            {
                                // Get the additional lengths required by the interpolator.
                                long addPre = rspl::InterpPack::get_len_pre();
                                long addPost = rspl::InterpPack::get_len_post();
                                // Initialize mipMap with the desired number of mip map levels (per octave).
                                mipMap.init_sample(sampleLength, addPre, addPost, nbrMipLevels, rspl::fir_halfband_lpf, 81);
                                mipMap.fill_sample(tempBuffer.data(), sampleLength);
                                resampler.set_interp(interpPack);
                                resampler.set_sample(mipMap);
                            }
                            else
                            {
                                // Optionally update the sample if necessary.
                            }
                            DBG("Global cable array received and RSPL sample updated.");
                        }
                        else
                        {
                            DBG("Global cable array size mismatch. Expected 2048, got: " + String(data.size()));
                        }
                    }
                    else
                    {
                        DBG("Global cable data is not an array. Received: " + data.toString());
                    }
                });
        }

        // Prepare: (if required) reinitialize RSPL structures when sample rate changes.
        void prepare(PrepareSpecs specs)
        {
            // Your implementation here (if dynamic sample rate changes are needed).
        }

        void reset() {}

        // Process the audio block using RSPL resampler.
        template <typename ProcessDataType>
        inline void process(ProcessDataType& data)
        {
            auto& fixData = data.template as<ProcessData<getFixChannelAmount()>>();
            auto audioBlock = fixData.toAudioBlock();
            auto* leftChannelData = audioBlock.getChannelPointer(0);
            auto* rightChannelData = audioBlock.getChannelPointer(1);
            int numSamples = data.getNumSamples();

            // Convert our pitch (in semitones) to RSPL fixed–point format.
            long rsplPitch = semitoneToRsplPitch(pitchSemitones);
            resampler.set_pitch(rsplPitch);

            // Create a temporary buffer to receive the resampled output.
            std::vector<float> tmpBuffer(numSamples, 0.0f);
            resampler.interpolate_block(tmpBuffer.data(), numSamples);

            // Apply volume and write to both channels.
            for (int i = 0; i < numSamples; ++i)
            {
                float s = tmpBuffer[i] * volume;
                leftChannelData[i] = s;
                rightChannelData[i] = s;
            }
        }

        // Parameter Handling: Parameter 0 - Volume; Parameter 1 - Pitch (in semitones)
        template <int P>
        inline void setParameter(double v)
        {
            if (P == 0)
            {
                volume = static_cast<float>(v);
            }
            else if (P == 1)
            {
                pitchSemitones = static_cast<float>(v);
            }
        }

        // Create GUI Parameters: Create parameters for "Volume" and "Pitch"
        void createParameters(ParameterDataList& data)
        {
            parameter::data pVol("Volume", { 0.0, 1.0, 0.001 });
            registerCallback<0>(pVol);
            pVol.setDefaultValue(1.0);
            data.add(std::move(pVol));

            parameter::data pPitch("Pitch", { -36.0, 36.0, 0.001 });
            registerCallback<1>(pPitch);
            pPitch.setDefaultValue(0.0);
            data.add(std::move(pPitch));
        }

        // Event Handling (modify as necessary for MIDI or other events)
        void handleHiseEvent(HiseEvent& e)
        {
            /*
            if (e.isNoteOn())
            {
                // Example: update pitch based on MIDI note number.
                float note = e.getNoteNumber();
                // Adjust pitchSemitones accordingly.
            }
            */
        }

        SN_EMPTY_PROCESS_FRAME;
    };

} // namespace project
