// ChordSlots - MIDI drag and drop tile.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class MidiDragTile final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    explicit MidiDragTile (ChordSlotsProcessor&,
                           ChordSlotsProcessor::MidiExportMode mode = ChordSlotsProcessor::MidiExportMode::Master,
                           const juce::String& label = "Drag MIDI");

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    ChordSlotsProcessor& processor;
    ChordSlotsProcessor::MidiExportMode exportMode;
    juce::String tileLabel;
    bool dragging = false;
};
