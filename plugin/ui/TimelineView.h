// ChordSlots - timeline view.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class TimelineView final : public juce::Component
{
public:
    explicit TimelineView (ChordSlotsProcessor&);

    void setProgression (const cs::Progression&);
    void setPlayhead (double step);

    void paint (juce::Graphics&) override;
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

private:
    float xForStep (double step) const;
    int stepForX (float x) const;
    int cutAt (float x) const;
    int slotAt (float x) const;
    juce::Rectangle<float> slotBounds (int i) const;
    juce::Rectangle<float> lockBounds (int i) const;
    juce::Rectangle<float> optionsBounds (int i) const;
    void showSlotMenu (int slot, int step);

    ChordSlotsProcessor& processor;
    cs::Progression prog;
    double playhead = -1.0;
    int hoverSlot = -1, hoverCut = -1, dragCut = -1, selected = -1;
    bool hoverLock = false, hoverOptions = false;
};
