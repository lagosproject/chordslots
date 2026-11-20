// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PreviewSynth.h"
#include "ChordSlotsPianoAssets.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>

class PreviewPianoBank final : private juce::Thread
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> audio;
        int root = 0;
    };

    PreviewPianoBank() : Thread ("ChordSlots piano loader") { startThread(); }

    ~PreviewPianoBank() override { stopThread (-1); }

    void request() noexcept
    {
        requested.store (true, std::memory_order_relaxed);
    }

    const Sample* find (int note) const noexcept
    {
        if (! ready.load (std::memory_order_acquire))
            return nullptr;

        const int index = juce::jlimit (0, (int) samples.size() - 1, (note - 21 + 3) / 6);
        return &samples[(size_t) index];
    }

    static constexpr double sampleRate = 44100.0;

private:
    void run() override
    {
        // Parameter listeners can run on the audio thread. They only set a flag.
        while (! threadShouldExit() && ! requested.load (std::memory_order_relaxed))
            wait (20);

        juce::FlacAudioFormat format;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            if (threadShouldExit())
                return;

            auto& sample = samples[i];
            sample.root = 21 + 6 * (int) i;
            const auto resource = "piano_" + juce::String (sample.root) + "_flac";
            int bytes = 0;
            const auto* data = ChordSlotsPianoAssets::getNamedResource (resource.toRawUTF8(), bytes);
            if (data == nullptr)
                return;

            std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (
                new juce::MemoryInputStream (data, (size_t) bytes, false), true));
            if (reader == nullptr || reader->numChannels != 1 || reader->sampleRate != sampleRate
                || reader->lengthInSamples < 2 || reader->lengthInSamples > 5 * sampleRate)
                return;

            const auto frames = (int) reader->lengthInSamples;
            sample.audio.setSize (1, frames + 1);
            if (! reader->read (&sample.audio, 0, frames, 0, true, false))
                return;
            sample.audio.setSample (0, frames, 0.0f);
        }

        // The bank is immutable after publication and lives as long as its voices.
        ready.store (true, std::memory_order_release);
    }

    std::array<Sample, 12> samples;
    std::atomic<bool> requested { false };
    std::atomic<bool> ready { false };
};

namespace
{

struct PreviewSound final : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** PolyBLEP residual for a naive sawtooth, t = phase 0..1, dt = phase increment. */
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

class PreviewVoice final : public juce::SynthesiserVoice
{
public:
    explicit PreviewVoice (const PreviewPianoBank& bank) : piano (bank) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PreviewSound*> (s) != nullptr; }

    void setModel (PreviewSoundModel m) noexcept { nextModel = m; }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        if (newRate > 0.0)
        {
            adsr.setSampleRate (newRate);
            updateAdsrParams();
        }
    }

    void updateAdsrParams()
    {
        if (model == PreviewSoundModel::RhodesEP)
            adsr.setParameters ({ 0.002f, 1.2f, 0.45f, 0.35f });
        else if (model == PreviewSoundModel::FeltPiano)
            adsr.setParameters ({ 0.012f, 0.85f, 0.50f, 0.40f });
        else if (model == PreviewSoundModel::AcousticPiano)
            adsr.setParameters ({ 0.002f, 0.0f, 1.0f, 0.23f });
        else
            adsr.setParameters ({ 0.004f, 0.45f, 0.62f, 0.22f });
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        model = nextModel;
        const double sr = getSampleRate();
        if (model == PreviewSoundModel::AcousticPiano)
        {
            if (const auto* sample = piano.find (midiNote))
            {
                sampleData = sample->audio.getReadPointer (0);
                sampleFrames = sample->audio.getNumSamples() - 1;
                samplePosition = 0.0;
                sampleStep = PreviewPianoBank::sampleRate / sr * std::exp2 ((midiNote - sample->root) / 12.0);
                level = velocity;
                adsr.reset();
                updateAdsrParams();
                adsr.noteOn();
                return;
            }

            // Keep preview responsive if a note arrives during the first load.
            model = PreviewSoundModel::FeltPiano;
        }

        updateAdsrParams();
        const double hz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        const double detune = std::pow (2.0, 7.0 / 1200.0);
        inc[0] = (float) (hz * detune / sr);
        inc[1] = (float) (hz / detune / sr);
        subInc = (float) (hz * 0.5 / sr);
        tineInc = (float) (hz * 14.0 / sr);
        if (tineInc >= 0.5f) tineInc = 0.49f;

        // Deterministic but different start phases keep stacked notes from flanging.
        phase[0] = std::fmod ((float) midiNote * 0.37f, 1.0f);
        phase[1] = std::fmod ((float) midiNote * 0.61f + 0.5f, 1.0f);
        subPhase = 0.0f;
        modPhase = 0.0f;
        tinePhase = 0.0f;
        modIndex = 1.0f + 3.0f * velocity;

        level = 0.05f + 0.08f * velocity; // leaves headroom for seven-note voicings
        cutoffBase = (float) juce::jlimit (200.0, 6000.0, hz * 1.5 + 300.0);
        cutoffEnv = 1500.0f + 5000.0f * velocity;
        lpL = lpR = 0.0f;
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            adsr.noteOff();
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    using juce::SynthesiserVoice::renderNextBlock;

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! adsr.isActive())
            return;

        if (model == PreviewSoundModel::AcousticPiano)
        {
            renderPiano (out, start, num);
            return;
        }

        const float sr = (float) getSampleRate();
        const float twoPi = juce::MathConstants<float>::twoPi;
        float* l = out.getWritePointer (0, start);
        float* r = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

        for (int i = 0; i < num; ++i)
        {
            const float env = adsr.getNextSample();

            if (model == PreviewSoundModel::RhodesEP)
            {
                const float modEnv = env * env;
                const float mod = std::sin (twoPi * modPhase) * (modIndex * modEnv);
                const float carrier = std::sin (twoPi * phase[0] + mod);
                const float tine = std::sin (twoPi * tinePhase) * (modEnv * modEnv * 0.20f);

                phase[0] += inc[0]; if (phase[0] >= 1.0f) phase[0] -= 1.0f;
                modPhase += inc[0]; if (modPhase >= 1.0f) modPhase -= 1.0f;
                tinePhase += tineInc; if (tinePhase >= 1.0f) tinePhase -= 1.0f;

                const float sig = (carrier + tine) * level * env;
                if (r != nullptr)
                {
                    l[i] += sig * 0.95f;
                    r[i] += sig * 1.05f;
                }
                else
                {
                    l[i] += sig;
                }
            }
            else if (model == PreviewSoundModel::FeltPiano)
            {
                float tri = 2.0f * std::abs (2.0f * (phase[0] - std::floor (phase[0] + 0.5f))) - 1.0f;
                float tri2 = 2.0f * std::abs (2.0f * (phase[1] - std::floor (phase[1] + 0.5f))) - 1.0f;
                phase[0] += inc[0]; if (phase[0] >= 1.0f) phase[0] -= 1.0f;
                phase[1] += inc[1]; if (phase[1] >= 1.0f) phase[1] -= 1.0f;

                const float cutoff = juce::jlimit (250.0f, sr * 0.35f, cutoffBase * (0.5f + 0.5f * env));
                const float a = 1.0f - std::exp (-twoPi * cutoff / sr);
                lpL += a * ((0.85f * tri + 0.35f * tri2) - lpL);
                lpR += a * ((0.35f * tri + 0.85f * tri2) - lpR);

                const float g = env * level * 1.25f;
                if (r != nullptr)
                {
                    l[i] += lpL * g;
                    r[i] += lpR * g;
                }
                else
                {
                    l[i] += 0.5f * (lpL + lpR) * g;
                }
            }
            else // CleanSaw
            {
                float saw[2];
                for (int k = 0; k < 2; ++k)
                {
                    saw[k] = 2.0f * phase[k] - 1.0f - polyBlep (phase[k], inc[k]);
                    phase[k] += inc[k];
                    if (phase[k] >= 1.0f)
                        phase[k] -= 1.0f;
                }
                const float sub = std::sin (twoPi * subPhase) * 0.5f;
                subPhase += subInc;
                if (subPhase >= 1.0f)
                    subPhase -= 1.0f;

                // One-pole low-pass per channel, cutoff following the envelope.
                const float cutoff = juce::jmin (cutoffBase + cutoffEnv * env * env, sr * 0.45f);
                const float a = 1.0f - std::exp (-twoPi * cutoff / sr);
                lpL += a * ((0.8f * saw[0] + 0.35f * saw[1] + sub) - lpL);
                lpR += a * ((0.35f * saw[0] + 0.8f * saw[1] + sub) - lpR);

                const float g = env * level;
                if (r != nullptr)
                {
                    l[i] += lpL * g;
                    r[i] += lpR * g;
                }
                else
                {
                    l[i] += 0.5f * (lpL + lpR) * g;
                }
            }
        }

        if (! adsr.isActive())
            clearCurrentNote();
    }

private:
    void renderPiano (juce::AudioBuffer<float>& out, int start, int num)
    {
        auto* left = out.getWritePointer (0, start);
        auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;
        for (int i = 0; i < num && samplePosition < sampleFrames && adsr.isActive(); ++i)
        {
            const auto frame = (int) samplePosition;
            const auto fraction = (float) (samplePosition - frame);
            const float value = sampleData[frame] + fraction * (sampleData[frame + 1] - sampleData[frame]);
            const float signal = value * level * adsr.getNextSample();
            left[i] += signal;
            if (right != nullptr)
                right[i] += signal;
            samplePosition += sampleStep;
        }

        if (samplePosition >= sampleFrames || ! adsr.isActive())
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    const PreviewPianoBank& piano;
    const float* sampleData = nullptr;
    double samplePosition = 0.0, sampleStep = 0.0;
    int sampleFrames = 0;
    PreviewSoundModel nextModel = PreviewSoundModel::CleanSaw;
    PreviewSoundModel model = PreviewSoundModel::CleanSaw;
    juce::ADSR adsr;
    float phase[2] = {}, inc[2] = {};
    float subPhase = 0.0f, subInc = 0.0f;
    float modPhase = 0.0f, modIndex = 2.0f;
    float tinePhase = 0.0f, tineInc = 0.0f;
    float level = 0.2f, cutoffBase = 1000.0f, cutoffEnv = 3000.0f;
    float lpL = 0.0f, lpR = 0.0f;
};

} // namespace

PreviewSynth::PreviewSynth()
{
    for (int i = 0; i < 24; ++i)
        synth.addVoice (new PreviewVoice (*piano));
    synth.addSound (new PreviewSound());
}

PreviewSynth::~PreviewSynth() = default;

void PreviewSynth::setSoundModel (PreviewSoundModel model) noexcept
{
    if (model == PreviewSoundModel::AcousticPiano)
        piano->request();
    requestedModel.store (model, std::memory_order_relaxed);
}

void PreviewSynth::prepare (double sampleRate)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    smoothedGain.reset (sampleRate, 0.03);
}

void PreviewSynth::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, float gain)
{
    const auto model = requestedModel.load (std::memory_order_relaxed);
    if (model != currentModel)
    {
        currentModel = model;
        for (int i = 0; i < synth.getNumVoices(); ++i)
            static_cast<PreviewVoice*> (synth.getVoice (i))->setModel (model);
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    smoothedGain.setTargetValue (gain);
    smoothedGain.applyGain (buffer, buffer.getNumSamples());
}

void PreviewSynth::allNotesOff()
{
    synth.allNotesOff (0, false);
}
