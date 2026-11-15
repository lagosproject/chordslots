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

    void setModel (PreviewSoundModel m) noexcept { model = m; }

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
        else
            adsr.setParameters ({ 0.004f, 0.45f, 0.62f, 0.22f });
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        updateAdsrParams();
        const double sr = getSampleRate();
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
        synth.addVoice (new PreviewVoice());
    synth.addSound (new PreviewSound());
}

void PreviewSynth::setSoundModel (PreviewSoundModel model)
{
    currentModel = model;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* pv = dynamic_cast<PreviewVoice*> (synth.getVoice (i)))
            pv->setModel (model);
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
