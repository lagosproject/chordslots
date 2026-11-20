// ChordSlots - small built-in synth so the plugin makes sound with no routing.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

enum class PreviewSoundModel : int
{
    CleanSaw = 0,
    RhodesEP = 1,
    FeltPiano = 2,
    AcousticPiano = 3
};

class PreviewPianoBank;

/** Built-in instruments for auditioning harmony without external routing. */
class PreviewSynth
{
public:
    PreviewSynth();
    ~PreviewSynth();

    void prepare (double sampleRate);
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, float gain);
    void allNotesOff();
    void setSoundModel (PreviewSoundModel model) noexcept;

private:
    juce::SharedResourcePointer<PreviewPianoBank> piano;
    juce::Synthesiser synth;
    juce::SmoothedValue<float> smoothedGain;
    std::atomic<PreviewSoundModel> requestedModel { PreviewSoundModel::CleanSaw };
    PreviewSoundModel currentModel { PreviewSoundModel::CleanSaw };
};
