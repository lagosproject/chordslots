// ChordSlots - audio processor: host-synced chord sequencer + MIDI out + preview synth.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PreviewSynth.h"

#include <chordslots/Generator.h>
#include <chordslots/Performance.h>
#include <chordslots/Trigger.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>

class ChordSlotsProcessor final : public juce::AudioProcessor,
                                  public juce::ChangeBroadcaster,
                                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    ChordSlotsProcessor();
    ~ChordSlotsProcessor() override;

    cs::PerformanceParams getPerformanceParams() const noexcept;
    cs::TriggerConfig getTriggerConfig() const noexcept;
    int getTriggeredSlot() const noexcept { return uiTriggeredSlot.load(); }
    void resetTrigger() noexcept;
    void refeel() noexcept;

    //==============================================================================
    // Model access (any non-audio thread). Listeners get a change message after
    // every edit and after a state restore.

    cs::Progression getProgression() const;

    /** Runs fn (Progression&, Rng&) under the model lock, then publishes the
        result to the audio thread. */
    template <typename Fn>
    void edit (Fn&& fn, bool undoable = true)
    {
        {
            const juce::ScopedLock sl (modelLock);
            if (undoable)
                pushHistory();
            fn (model, rng);
            cs::sanitizeRhythm (model);
            publish();
        }
        sendChangeMessage();
    }

    /** Stores an undo point without changing anything (call before a drag). */
    void beginGesture();
    bool undo();
    bool canUndo() const;

    //==============================================================================
    // Transport / audition (lock-free, safe from the UI)

    void setPreviewPlaying (bool shouldPlay) noexcept { previewOn.store (shouldPlay); }
    bool isPreviewPlaying() const noexcept { return previewOn.load(); }
    void auditionSlot (int slot) noexcept { auditionRequest.store (slot); }
    /** Loop position in sixteenth steps, or a negative value when stopped. */
    double getPlayPosition() const noexcept { return uiStep.load(); }

    //==============================================================================
    juce::MidiFile createMidiFile() const;
    /** Writes the progression to a temp .mid (for drag and drop). */
    juce::File writeTempMidiFile() const;
    juce::String describeProgression() const;

    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChordSlots"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushHistory();
    void publish();

    //==============================================================================
    // Model (guarded by modelLock)
    juce::CriticalSection modelLock;
    cs::Progression model;
    cs::Rng rng;
    std::vector<cs::Progression> history;

    // Hand-over to the audio thread
    juce::SpinLock playLock;
    cs::PlayData pendingPlay;
    std::atomic<bool> playDirty { false };

    // Audio-thread state
    cs::PlayData play;
    PreviewSynth synth;
    double sampleRate = 44100.0;
    int held[cs::kMaxNotes] = {};
    int heldCount = 0;
    int heldSlot = -1;
    int currentSlot = -1;
    bool wasPlaying = false;
    double expectedStep = 0.0;
    int auditionNotes[cs::kMaxNotes] = {};
    int auditionCount = 0;
    int auditionSamplesLeft = 0;

    std::atomic<bool> previewOn { false };
    std::atomic<int> auditionRequest { -1 };
    std::atomic<double> uiStep { -1.0 };
    std::atomic<double> lastBpm { 120.0 };

    void parameterChanged (const juce::String&, float) override;

    struct PendingNote
    {
        int note = 0;
        int velocity = 0;
        int samplesUntilOn = 0;
        int slot = -1;
    };
    static constexpr int kMaxPendingNotes = 32;
    PendingNote pendingNotes[kMaxPendingNotes] = {};
    int pendingCount = 0;

    std::atomic<float>* gateParam = nullptr;
    std::atomic<float>* synthOnParam = nullptr;
    std::atomic<float>* synthVolParam = nullptr;
    std::atomic<float>* strumParam = nullptr;
    std::atomic<float>* accentParam = nullptr;
    std::atomic<float>* humanizeParam = nullptr;
    std::atomic<float>* triggerModeParam = nullptr;
    std::atomic<float>* triggerRootParam = nullptr;
    std::atomic<float>* triggerLayoutParam = nullptr;
    std::atomic<uint32_t> perfSeed { 1 };

    cs::TriggerEngine triggerEngine;
    std::atomic<int> uiTriggeredSlot { -1 };
    juce::int64 triggerGlowExpirySamples = 0;
    juce::int64 totalSamplesProcessed = 0;
    uint32_t liveTriggerPass = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsProcessor)
};
