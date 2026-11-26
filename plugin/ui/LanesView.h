// ChordSlots - 3-lane target editor view.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class LanesView final : public juce::Component,
                        public juce::TooltipClient
{
public:
    explicit LanesView (ChordSlotsProcessor&);

    void setProgression (const cs::Progression&);
    void setSelectedSlot (int slot) { selectedSlot = slot; repaint(); }
    void paint (juce::Graphics&) override;
    juce::String getTooltip() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> laneBounds (int lane) const;
    void applyDrag (const juce::MouseEvent&);
    int slotAtX (float x) const;
    int laneAt (juce::Point<float> pos) const;
    void setReadout (int lane, int slot);

    ChordSlotsProcessor& processor;
    cs::Progression prog;
    int selectedSlot = -1;
    int dragLane = -1;
    unsigned editedMask = 0;
    int readoutLane = -1, readoutSlot = -1; // value shown for the lane and slot under the pointer
};
