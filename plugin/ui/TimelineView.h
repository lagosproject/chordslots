// ChordSlots - timeline view.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class TimelineView final : public juce::Component,
                           public juce::TooltipClient
{
public:
    explicit TimelineView (ChordSlotsProcessor&);

    void setProgression (const cs::Progression&);
    void setPlayhead (double step);
    double getPlayhead() const noexcept { return playhead; }
    void setMidiVisible (bool show);

    void paint (juce::Graphics&) override;
    juce::String getTooltip() override;
    void mouseEnter (const juce::MouseEvent& e) override { mouseMove (e); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    int getSelectedSlot() const noexcept { return selected; }
    void setSelectedSlot (int s) { selected = s; repaint(); }

    bool keyPressed (const juce::KeyPress& key) override;

    std::function<void(int)> onSlotSelected;
    std::function<void(int)> onOpenAlternatives;

private:
    int timelineLeft() const;
    double positionForX (float x) const;
    float xForStep (double step) const;
    int stepForX (float x) const;
    int cutAt (float x) const;
    int slotAt (float x) const;
    juce::Rectangle<float> slotBounds (int i) const;
    juce::Rectangle<float> lockBounds (int i) const;
    juce::Rectangle<float> optionsBounds (int i) const;
    void showSlotMenu (int slot, int step);

    void showCutMenu (int cut);
    void updateMidiNotes();
    void paintMidi (juce::Graphics&);

    struct MidiNote
    {
        double start, end;
        int pitch, velocity;
        bool bass;
    };
    std::vector<MidiNote> midiNotes;
    bool midiVisible = false;
    float midiHoverY = -1.0f;
    int dragStep = -1;

    ChordSlotsProcessor& processor;
    cs::Progression prog;
    double playhead = -1.0;
    int hoverSlot = -1, hoverCut = -1, dragCut = -1, selected = -1;
    bool hoverLock = false, hoverOptions = false;
};
