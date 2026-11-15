// ChordSlots - Rich In-Context Chord Inspector & Picker Overlay
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class ChordPickerOverlay final : public juce::Component
{
public:
    explicit ChordPickerOverlay (ChordSlotsProcessor& proc);

    void setSlot (int slotIndex);
    int getSlot() const noexcept { return currentSlot; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    std::function<void()> onClose;

private:
    void rebuildCandidates();
    void selectRoot (int rootPc);
    void selectQuality (int typeIdx);
    void selectBass (int bassInt);
    void applyChord (cs::Chord c);

    ChordSlotsProcessor& processor;
    int currentSlot = -1;
    cs::Progression prog;
    cs::Chord activeChord;

    juce::TextButton closeBtn { "✕" };
    juce::Label titleLabel;

    // 12 Chromatic Roots
    std::array<std::unique_ptr<juce::TextButton>, 12> rootButtons;

    // Quality buttons
    struct QualityBtn
    {
        std::unique_ptr<juce::TextButton> btn;
        int typeIndex = 0;
        QualityBtn (std::unique_ptr<juce::TextButton> b, int t) : btn (std::move (b)), typeIndex (t) {}
    };
    std::vector<QualityBtn> qualityButtons;

    // Bass / Inversion buttons
    std::vector<std::unique_ptr<juce::TextButton>> bassButtons;

    // Smart candidate substitutions
    struct CandidateItem
    {
        std::unique_ptr<juce::TextButton> btn;
        cs::Chord chord;
        float deltaFit = 0.0f;
        CandidateItem (std::unique_ptr<juce::TextButton> b, cs::Chord c, float d) : btn (std::move (b)), chord (c), deltaFit (d) {}
    };
    std::vector<CandidateItem> candidateButtons;

    juce::Rectangle<int> rootArea, qualityArea, bassArea, candidateArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPickerOverlay)
};
