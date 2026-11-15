// ChordSlots - small built-in synth so the plugin makes sound with no routing.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

enum class PreviewSoundModel : int
{
    CleanSaw = 0,
    RhodesEP,
    FeltPiano
};

/** Built-in synth with Clean Saw, 2-op FM Rhodes EP, and warm Felt Piano models.
    Deliberately plain and instant: it is there to audition harmony, not to be the sound. */
class PreviewSynth
{
public:
    PreviewSynth();

    void prepare (double sampleRate);
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, float gain);
    void allNotesOff();
    void setSoundModel (PreviewSoundModel model);

private:
    juce::Synthesiser synth;
    juce::SmoothedValue<float> smoothedGain;
    PreviewSoundModel currentModel { PreviewSoundModel::CleanSaw };
};
