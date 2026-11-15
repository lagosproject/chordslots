// ChordSlots - editor UI.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/TimelineView.h"
#include "ui/LanesView.h"
#include "ui/MidiDragTile.h"
#include "ui/ChordPickerOverlay.h"

//==============================================================================
class ChordSlotsEditor final : public juce::AudioProcessorEditor,
                               private juce::ChangeListener,
                               private juce::Timer
{
public:
    explicit ChordSlotsEditor (ChordSlotsProcessor&);
    ~ChordSlotsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void rebuildKeyNames (int scale);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& caption);
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& caption);
    void showInputMenu();
    void saveMidi();
    void analyzeMidi();
    void applyShapeStamp (int stampId);
    bool keyPressed (const juce::KeyPress& key) override;

    ChordSlotsProcessor& processor;
    ChordSlotsLookAndFeel lnf;

    // Header controls
    juce::ComboBox keyBox, scaleBox, barsBox, meterBox, slotsBox, snapBox;
    juce::Label keyLabel, scaleLabel, barsLabel, meterLabel, slotsLabel, snapLabel;
    juce::TextButton inputButton { "MIDI In ▾" };
    juce::Label fitLabel, fitCaption;

    // Variations
    juce::Label variationLabel;
    juce::TextButton varButtons[cs::kNumVariations];
    juce::TextButton followButton { "Follow" };
    juce::TextButton copyVarButton { "Copy..." };

    // Views
    TimelineView timeline;
    ChordPickerOverlay pickerOverlay;

    // Lane Toolbar controls
    juce::Label laneToolbarLabel;
    juce::ComboBox moodBox, styleBox, shapeBox;
    juce::Label moodLabel, styleLabel, shapeLabel;
    juce::TextButton resetCurvesBtn { "Reset" };
    juce::TextButton analyzeMidiBtn;

    LanesView lanes;

    // Footer Panel 1: Harmony Generator
    juce::TextButton lockRhythm { "Rhythm" }, lockRoots { "Roots" }, lockColors { "Colors" }, lockVoicing { "Voicing" };
    juce::Label lockCaption;
    juce::Slider adventure, syncopation, bassMotion;
    juce::Label adventureLabel, syncopationLabel, bassMotionLabel;
    juce::TextButton undoButton { "Undo" }, rollButton { "ROLL" };

    // Footer Panel 2: Groove & Feel
    juce::ComboBox compBox;
    juce::Label compLabel;
    juce::Slider strum, accent, humanize;
    juce::Label strumLabel, accentLabel, humanizeLabel;
    juce::TextButton refeelButton { "Re-Feel" };

    // Footer Panel 3: Output & Export
    juce::ComboBox synthModelBox;
    juce::Label synthModelLabel;
    juce::Slider gate, volume;
    juce::Label gateLabel, volumeLabel;
    juce::TextButton playButton { "Play" }, saveButton { "Save .mid" };
    juce::TextButton splitMidiButton { "Split Ch 1/2" };
    MidiDragTile dragTile;
    MidiDragTile dragChordsTile;
    MidiDragTile dragBassTile;

    // Context / Hint bar & Targeted Rolls
    juce::Label hint;
    juce::TextButton rollRootBtn { "Roll Root" }, rollColorBtn { "Roll Color" }, rollBassBtn { "Roll Bass" }, inspectBtn { "Inspect ⋯" };

    void updateContextBar (int slot);

    // Attachments
    juce::AudioProcessorValueTreeState::SliderAttachment gateAttachment, volumeAttachment,
        strumAttachment, accentAttachment, humanizeAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment splitMidiAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> compAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> synthModelAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    int keyNamesScale = -1;
    bool refreshing = false;

    juce::Rectangle<int> harmonyPanelBounds, groovePanelBounds, outputPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsEditor)
};
