// ChordSlots - Rich In-Context Chord Inspector & Picker Overlay
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class ChordPickerOverlay final : public juce::Component
{
public:
    explicit ChordPickerOverlay (ChordSlotsProcessor& proc);

    void open (int slotIndex, bool showPlot);
    void setSlot (int slotIndex);
    int getSlot() const noexcept { return currentSlot; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    std::function<void()> onClose;

private:
    void rebuildCandidates();
    void selectCandidate (int index);
    void paintPlot (juce::Graphics&);
    void layoutPlot (juce::Rectangle<int>);
    void selectRoot (int rootPc);
    void selectQuality (int typeIdx);
    void selectBass (int bassInt);
    void applyChord (cs::Chord c);

    ChordSlotsProcessor& processor;
    int currentSlot = -1;
    cs::Progression prog;
    cs::Chord activeChord;

    class AuditionButton final : public juce::TextButton, private juce::Timer
    {
    public:
        AuditionButton();
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::function<void(bool)> onAudition;

    private:
        void timerCallback() override;
        bool fired = false;
    };

    juce::TextButton closeBtn { "Close" }, swapBtn { "Swap" }, inspectorBtn { "Inspect" };
    AuditionButton auditionBtn;
    juce::Label selectionLabel;
    bool plotMode = false;
    int selectedCandidate = -1;
    juce::Rectangle<int> plotArea;
    juce::Point<float> currentPoint;
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
        juce::Point<float> metrics, point;
        CandidateItem (std::unique_ptr<juce::TextButton> b, cs::Chord c, juce::Point<float> m)
            : btn (std::move (b)), chord (c), metrics (m) {}
    };
    std::vector<CandidateItem> candidateButtons;

    juce::Rectangle<int> rootArea, qualityArea, bassArea, candidateArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPickerOverlay)
};
