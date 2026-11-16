// ChordSlots - audio processor: host-synced chord sequencer + MIDI out + preview synth.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PreviewSynth.h"

#include <chordslots/Bank.h>
#include <chordslots/Generator.h>
#include <chordslots/Performance.h>
#include <chordslots/Trigger.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>

class ChordSlotsProcessor final : public juce::AudioProcessor,
                                  public juce::ChangeBroadcaster,
                                  public juce::Timer,
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

    void timerCallback() override;
    void publish();
    cs::PlayData getPlayData (int variationIndex = 0) const;
    double getPublishedBpm() const noexcept { return publishedBpm.load(); }
    double getLastBpm() const noexcept { return lastBpm.load(); }

    //==============================================================================
    // Model access (any non-audio thread). Listeners get a change message after
    // every edit and after a state restore.

    cs::Progression getProgression() const;
    cs::Progression getProgression (int variationIndex) const;

    int getEditVariation() const noexcept { return editVariation.load(); }
    void setEditVariation (int index);

    int getPlayingVariation() const noexcept;
    void setPlayingVariation (int index);

    bool isFollowingPlayback() const noexcept { return followPlayback.load(); }
    void setFollowPlayback (bool follow) noexcept;

    void copyVariation (int src, int dst);
    void setMeter (int num, int den);

    int getHostTimeSigNum() const noexcept { return hostTimeSigNum.load(); }
    int getHostTimeSigDen() const noexcept { return hostTimeSigDen.load(); }
    bool isFollowingHostMeter() const noexcept { return followHostMeter.load(); }
    void setFollowHostMeter (bool follow) noexcept { followHostMeter.store (follow); }

    /** Runs fn (Progression&, Rng&) under the model lock, then publishes the
        result to the audio thread. */
    template <typename Fn>
    void edit (Fn&& fn, bool undoable = true)
    {
        {
            const juce::ScopedLock sl (modelLock);
            const int idx = editVariation.load();
            if (undoable)
                pushHistory (idx);
            fn (variations[(size_t) idx], rng);
            cs::sanitizeRhythm (variations[(size_t) idx]);
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
    enum class MidiExportMode
    {
        Master = 0,
        ChordsOnly = 1,
        BassOnly = 2
    };

    juce::MidiFile createMidiFile (MidiExportMode mode = MidiExportMode::Master) const;
    /** Writes the progression to a temp .mid (for drag and drop). */
    juce::File writeTempMidiFile (MidiExportMode mode = MidiExportMode::Master) const;
    juce::String describeProgression() const;

    /** Extracts Brightness, Tension, and Energy curves from a dropped MIDI file. */
    bool loadMidiToCurves (const juce::File& file);
    void setSynthModel (PreviewSoundModel model);
    bool isSplitMidi() const noexcept;

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
    void pushHistory (int variationIndex);

    //==============================================================================
    // Model (guarded by modelLock)
    juce::CriticalSection modelLock;
    std::array<cs::Progression, cs::kNumVariations> variations;
    cs::Rng rng;
    std::array<std::vector<cs::Progression>, cs::kNumVariations> histories;
    std::atomic<int> editVariation { 0 };
    std::atomic<bool> followPlayback { false };

    // Hand-over to the audio thread
    mutable juce::SpinLock playLock;
    cs::PlayBank pendingPlayBank;
    std::atomic<bool> playDirty { false };

    // Audio-thread state
    cs::PlayBank playBank;
    int activePlayingVariation = 0;
    PreviewSynth synth;
    double sampleRate = 44100.0;

    struct HeldVoice
    {
        int note = 0;
        int channel = 1;
        bool isBass = false;
    };
    static constexpr int kMaxHeldNotes = 32;
    HeldVoice held[kMaxHeldNotes] = {};
    int heldCount = 0;
    int heldSlot = -1;
    int currentSlot = -1;
    bool wasPlaying = false;
    double expectedStep = 0.0;

    struct AuditionVoice
    {
        int note = 0;
        int channel = 1;
    };
    AuditionVoice auditionNotes[cs::kMaxNotes] = {};
    int auditionCount = 0;
    int auditionSamplesLeft = 0;

    std::atomic<bool> previewOn { false };
    std::atomic<int> auditionRequest { -1 };
    std::atomic<double> uiStep { -1.0 };
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<double> publishedBpm { 120.0 };
    std::atomic<bool> tempoDirty { false };

    void parameterChanged (const juce::String&, float) override;

    struct PendingNote
    {
        int note = 0;
        int velocity = 0;
        int channel = 1;
        int samplesUntilOn = 0;
        int slot = -1;
    };
    // Worst case pending note analysis:
    // Strum/humanize timing delay max is 80ms + 6ms = 86ms. At 120 bpm, 86ms spans ~0.17 beats (~4 steps).
    // The densest comp pattern (PulseEighth/TrapStabs) spaces hits by >= 12 steps (250ms at 120 bpm,
    // or 100ms at 300 bpm). Across audio block boundaries, at most 2 hits can have strummed voices
    // pending simultaneously. With at most 7 upper voices per chord, at most 14 notes can be pending
    // concurrently. kMaxPendingNotes = 128 is therefore >4x the worst-case bound and completely safe.
    static constexpr int kMaxPendingNotes = 128;
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
    std::atomic<float>* splitMidiParam = nullptr;
    std::atomic<float>* variationParam = nullptr;
    std::atomic<float>* quantizeSwitchParam = nullptr;
    std::atomic<float>* compStyleParam = nullptr;
    std::atomic<float>* synthModelParam = nullptr;

    std::atomic<int> hostTimeSigNum { 4 };
    std::atomic<int> hostTimeSigDen { 4 };
    std::atomic<bool> followHostMeter { false };
    std::atomic<uint32_t> perfSeed { 1 };

    cs::TriggerEngine triggerEngine;
    std::atomic<int> uiTriggeredSlot { -1 };
    juce::int64 triggerGlowExpirySamples = 0;
    juce::int64 totalSamplesProcessed = 0;
    uint32_t liveTriggerPass = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsProcessor)
};
