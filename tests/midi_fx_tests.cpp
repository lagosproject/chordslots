// Headless smoke check for the AU MIDI FX processor variant.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../plugin/PluginProcessor.h"

#include <cstdio>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    auto processor = std::make_unique<ChordSlotsProcessor>();
    int checks = 0;
    int failures = 0;
    const auto check = [&] (bool condition, const char* description)
    {
        ++checks;
        if (! condition)
        {
            ++failures;
            std::printf ("FAIL %s\n", description);
        }
    };

    check (processor->isMidiEffect(), "processor identifies as MIDI FX");
    check (processor->getName() == "ChordSlots MIDI FX", "processor name uses MIDI FX product identity");
    check (processor->getTailLengthSeconds() == 0.0, "MIDI FX has no synth tail");
    check (processor->getBusCount (true) == 0 && processor->getBusCount (false) == 0,
           "MIDI FX processor has no audio buses");
    check (processor->isBusesLayoutSupported (juce::AudioProcessor::BusesLayout {}),
           "empty JUCE bus layout is accepted safely");

    constexpr int blockSize = 512;
    constexpr int numBlocks = 240;
    processor->setRateAndBufferSizeDetails (48000.0, blockSize);
    processor->prepareToPlay (48000.0, blockSize);
    processor->setPreviewPlaying (true);
    juce::AudioBuffer<float> buffer (0, blockSize);
    juce::MidiBuffer midi;
    int noteOns = 0;
    int noteOffs = 0;
    for (int block = 0; block < numBlocks; ++block)
    {
        midi.clear();
        processor->processBlock (buffer, midi);
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            noteOns += message.isNoteOn() ? 1 : 0;
            noteOffs += message.isNoteOff() ? 1 : 0;
        }
    }
    processor->setPreviewPlaying (false);
    processor->releaseResources();
    check (noteOns > 0, "zero-channel callback outputs generated MIDI note-ons");
    check (noteOffs > 0, "zero-channel callback outputs generated MIDI note-offs");
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
