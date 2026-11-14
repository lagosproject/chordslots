// ChordSlots - small built-in synth so the plugin makes sound with no routing.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/** Two detuned band-limited saws and a sub sine through a gentle low-pass.
    Deliberately plain: it is there to audition harmony, not to be the sound. */
class PreviewSynth
{
public:
    PreviewSynth();

    void prepare (double sampleRate);
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, float gain);
    void allNotesOff();

private:
    juce::Synthesiser synth;
    juce::SmoothedValue<float> smoothedGain;
};
