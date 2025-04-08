#pragma once
#include <JuceHeader.h>
#include <cstring>


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

    // Use this enum to refer to the cables, e.g. this->setGlobalCableValue<GlobalCables::cbl_WT1>(0.4)
    enum class GlobalCables
    {
        cbl_WT1 = 0
    };

    // Subclass your node from this
    using cable_manager_t = routing::global_cable_cpp_manager<SN_GLOBAL_CABLE(576864674)>;

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

        //======================================================================
        // WavePlayer: Plays a single cycle wave stored in a local buffer.
        // The buffer is assumed to have 2048 samples.
        // The oscillator is made audible by using a phase accumulator with a 
        // phase increment computed from a target frequency.
        //======================================================================
        class WavePlayer
        {
        public:
            WavePlayer() : playhead(0.0f), volume(1.0f), phaseInc(0.0f)
            {
                memset(buffer, 0, sizeof(buffer)); // Initialize to silence
            }

            // Prepare the oscillator with the given sample rate and target frequency.
            // For a single-cycle wave of length 2048, the phase increment to achieve
            // a target frequency is: (wave length * targetFrequency) / sampleRate.
            void prepare(double sampleRate, double targetFrequency = 440.0)
            {
                phaseInc = static_cast<float>((2048.0 * targetFrequency) / sampleRate);
                playhead = 0.0f;
            }

            // Update the waveform buffer from new data.
            void setWaveBuffer(const float* newBuffer)
            {
                memcpy(buffer, newBuffer, sizeof(buffer));
                playhead = 0.0f;
            }

            // Process one sample: read from the buffer (using nearest–neighbor lookup),
            // apply volume, increment the playhead by phaseInc, and wrap if needed.
            inline float processSample()
            {
                int idx = static_cast<int>(playhead);
                float sample = buffer[idx] * volume;
                playhead += phaseInc;
                if (playhead >= 2048.0f)
                    playhead -= 2048.0f;
                return sample;
            }

            inline void setVolume(float vol)
            {
                volume = vol;
            }

        private:
            float buffer[2048];
            float playhead;
            float volume;
            float phaseInc;
        };

        WavePlayer wavePlayer;

        //==========================================================================
        // Constructor: Register the global cable callback to update the wave buffer.
        // For debugging, the callback prints the array contents, then copies the data.
        //==========================================================================
        Griffin_WT()
        {
            this->registerDataCallback<GlobalCables::cbl_WT1>([this](const var& data)
                {
                    if (data.isArray())
                    {
                        if (data.size() == 2048)
                        {
                            float tempBuffer[2048];
                            for (int i = 0; i < 2048; ++i)
                            {
                                DBG(String(i) + ": " + data[i].toString());
                                tempBuffer[i] = static_cast<float>(data[i]);
                            }
                            wavePlayer.setWaveBuffer(tempBuffer);
                            DBG("Global cable array received and wave buffer updated.");
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

        //==========================================================================
        // Prepare: Initialize the oscillator using the sample rate.
        //==========================================================================
        void prepare(PrepareSpecs specs)
        {
            // Prepare the oscillator with a target frequency of 440 Hz.
            wavePlayer.prepare(specs.sampleRate, 440.0);
        }

        //==========================================================================
        // Reset: Called when the plugin is reloaded.
        //==========================================================================
        void reset() {}

        //==========================================================================
        // Process: For each sample, output the wave (with applied volume) on both channels.
        //==========================================================================
        template <typename ProcessDataType>
        inline void process(ProcessDataType& data)
        {
            auto& fixData = data.template as<ProcessData<getFixChannelAmount()>>();
            auto audioBlock = fixData.toAudioBlock();
            auto* leftChannelData = audioBlock.getChannelPointer(0);
            auto* rightChannelData = audioBlock.getChannelPointer(1);
            int numSamples = data.getNumSamples();

            for (int i = 0; i < numSamples; ++i)
            {
                float s = wavePlayer.processSample();
                leftChannelData[i] = s;
                rightChannelData[i] = s;
            }
        }

        //==========================================================================
        // Parameter Handling: One parameter for volume control.
        //==========================================================================
        template <int P>
        inline void setParameter(double v)
        {
            if (P == 0)
            {
                wavePlayer.setVolume(static_cast<float>(v));
            }
        }

        //==========================================================================
        // Create GUI Parameters: Only one parameter "Volume" is created.
        //==========================================================================
        void createParameters(ParameterDataList& data)
        {
            parameter::data p("Volume", { 0.0, 1.0, 0.001 });
            registerCallback<0>(p);
            p.setDefaultValue(1.0);
            data.add(std::move(p));
        }

        //==============================================================================
        // Event Handling (Modify as needed for MIDI or other events)
        //==============================================================================
        void handleHiseEvent(HiseEvent& e)
        {
            /*
            if (e.isNoteOn())
            {
                float note = e.getNoteNumber();
            }
            */
        }

        SN_EMPTY_PROCESS_FRAME;
    };

} // namespace project
