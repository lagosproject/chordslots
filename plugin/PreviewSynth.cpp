// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PreviewSynth.h"

#include <cmath>

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
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PreviewSound*> (s) != nullptr; }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        if (newRate > 0.0)
        {
            adsr.setSampleRate (newRate);
            adsr.setParameters ({ 0.004f, 0.45f, 0.62f, 0.22f });
        }
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        const double sr = getSampleRate();
        const double hz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        const double detune = std::pow (2.0, 7.0 / 1200.0);
        inc[0] = (float) (hz * detune / sr);
        inc[1] = (float) (hz / detune / sr);
        subInc = (float) (hz * 0.5 / sr);
        // Deterministic but different start phases keep stacked notes from flanging.
        phase[0] = std::fmod ((float) midiNote * 0.37f, 1.0f);
        phase[1] = std::fmod ((float) midiNote * 0.61f + 0.5f, 1.0f);
        subPhase = 0.0f;

        level = 0.05f + 0.08f * velocity; // leaves headroom for seven-note voicings
        // Brighter when played harder, and key-tracked so low notes are not dull.
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

        const float sr = (float) getSampleRate();
        float* l = out.getWritePointer (0, start);
        float* r = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

        for (int i = 0; i < num; ++i)
        {
            const float env = adsr.getNextSample();

            float saw[2];
            for (int k = 0; k < 2; ++k)
            {
                saw[k] = 2.0f * phase[k] - 1.0f - polyBlep (phase[k], inc[k]);
                phase[k] += inc[k];
                if (phase[k] >= 1.0f)
                    phase[k] -= 1.0f;
            }
            const float sub = std::sin (juce::MathConstants<float>::twoPi * subPhase) * 0.5f;
            subPhase += subInc;
            if (subPhase >= 1.0f)
                subPhase -= 1.0f;

            // One-pole low-pass per channel, cutoff following the envelope.
            const float cutoff = juce::jmin (cutoffBase + cutoffEnv * env * env, sr * 0.45f);
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / sr);
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

        if (! adsr.isActive())
            clearCurrentNote();
    }

private:
    juce::ADSR adsr;
    float phase[2] = {}, inc[2] = {};
    float subPhase = 0.0f, subInc = 0.0f;
    float level = 0.2f, cutoffBase = 1000.0f, cutoffEnv = 3000.0f;
    float lpL = 0.0f, lpR = 0.0f;
};

} // namespace

PreviewSynth::PreviewSynth()
{
    for (int i = 0; i < 24; ++i)
        synth.addVoice (new PreviewVoice());
    synth.addSound (new PreviewSound());
}

void PreviewSynth::prepare (double sampleRate)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    smoothedGain.reset (sampleRate, 0.03);
}

void PreviewSynth::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, float gain)
{
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    smoothedGain.setTargetValue (gain);
    smoothedGain.applyGain (buffer, buffer.getNumSamples());
}

void PreviewSynth::allNotesOff()
{
    synth.allNotesOff (0, false);
}
