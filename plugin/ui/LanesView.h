// ChordSlots - 3-lane target editor view.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class LanesView final : public juce::Component,
                        public juce::FileDragAndDropTarget
{
public:
    explicit LanesView (ChordSlotsProcessor&);

    void setProgression (const cs::Progression&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    juce::Rectangle<float> laneBounds (int lane) const;
    void applyDrag (const juce::MouseEvent&);

    ChordSlotsProcessor& processor;
    cs::Progression prog;
    int dragLane = -1;
    unsigned editedMask = 0;
    bool isDragOver = false;
};
