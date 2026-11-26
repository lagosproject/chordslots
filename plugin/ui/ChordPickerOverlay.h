// ChordSlots - selected-slot editor and substitutions.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

class ChordPickerOverlay final : public juce::Component
{
public:
    explicit ChordPickerOverlay (ChordSlotsProcessor& proc);
    ~ChordPickerOverlay() override;

    void open (int slotIndex, bool showAlternatives);
    void setSlot (int slotIndex);
    int getSlot() const noexcept { return currentSlot; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    std::function<void()> onClose;

private:
    void setSubstitutionsVisible (bool show);
    void rebuildCandidates();
    void layoutAlternatives (juce::Rectangle<int>);
    void cancelGestures();
    void selectRoot (int rootPc);
    void selectQuality (int typeIdx);
    void selectBass (int bassInt);
    void invertChord (cs::InversionDirection direction);
    void applyChord (cs::Chord c);

    ChordSlotsProcessor& processor;
    int currentSlot = -1;
    cs::Progression prog;
    cs::Chord activeChord;

    class AuditionButton final : public juce::TextButton, private juce::Timer
    {
    public:
        AuditionButton (const juce::String& label, std::array<float, 3> deltas,
                        juce::String outsideNotes, const juce::String& scaleDetails,
                        int melodyPercent, juce::String clashes, bool showMelody,
                        std::array<float, 2> original, std::array<float, 2> targets);
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        juce::String getTooltip() override;
        void cancelGesture();
        std::function<void(bool)> onAudition;
        std::function<void()> onRelease;
        juce::TextButton replace { "Replace" };
        juce::TextButton audition { "Audition" };

    private:
        void paintButton (juce::Graphics&, bool, bool) override;
        void timerCallback() override;
        std::array<float, 3> deltas;
        std::array<float, 2> original, targets;
        juce::String outsideNotes;
        std::array<juce::Rectangle<int>, 3> cells;
        juce::Rectangle<int> nameBounds, melodyBounds;
        int melodyPercent;
        juce::String clashes;
        bool showMelody;
        bool pressed = false;
        bool fired = false;
        bool cancelled = false;
    };

    juce::TextButton closeBtn { "Close" }, substitutionsTab { "Substitutions" }, inspectorTab { "Inspector" };
    juce::TextButton chordLock { "Chord lock" };
    juce::TextButton rollRoot { "Roll Root" }, rollColor { "Roll Color" }, changeBass { "Change Bass" };
    juce::Label lockScope;
    juce::ComboBox qualityBox;
    int contextVariation = -1;
    juce::ToggleButton showOutside { "Show out-of-scale chords" };
    juce::Component alternativesContent;
    juce::Viewport alternativesViewport;
    juce::Label outsideHeading, emptyGroup;
    bool alternativesMode = true;
    int currentMelodyPercent = -1;
    juce::Label titleLabel;

    // 12 Chromatic Roots
    std::array<std::unique_ptr<juce::TextButton>, 12> rootButtons;

    juce::TextButton invertUpBtn { "Invert Up" }, invertDownBtn { "Invert Down" };
    std::vector<std::unique_ptr<juce::TextButton>> bassButtons;

    // Smart candidate substitutions
    struct CandidateItem
    {
        std::unique_ptr<juce::TextButton> btn;
        bool outside = false;
    };
    std::vector<CandidateItem> candidateButtons;

    juce::Rectangle<int> tabDivider, targetSummaryBounds;
    juce::Rectangle<int> rootArea, qualityArea, voicingArea, bassArea, candidateArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPickerOverlay)
};
