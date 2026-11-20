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
    void updateDragTileVisibility();
    void showGrooveView (bool show);
    void rebuildKeyNames (int scale);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& caption, const juce::String& tooltip);
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& caption, const juce::String& tooltip);
    void showInputMenu();
    void showOptionsMenu();
    void updateTooltips();
    void showSongMenu();
    void showKeyActions();
    void saveMidi();
    void showMidiImportMenu();
    void importMidi (bool harmonize);
    void applyShapeStamp (int stampId);
    bool keyPressed (const juce::KeyPress& key) override;

    ChordSlotsProcessor& processor;
    ChordSlotsLookAndFeel lnf;

    // Header controls
    juce::ComboBox keyBox, scaleBox, barsBox, meterBox, slotsBox, snapBox;
    juce::Label keyLabel, barsLabel, meterLabel, slotsLabel, snapLabel;
    juce::TextButton keyActionsButton { "Key Actions" };
    juce::TextButton inputButton { "MIDI In ▾" };
    juce::TextButton optionsButton { "Options" };
    juce::Label fitLabel, fitCaption;

    // Variations
    juce::Label variationLabel;
    juce::TextButton varButtons[cs::kNumVariations];
    juce::TextButton followButton { "Follow" };
    juce::TextButton copyVarButton { "Copy..." };

    // Views
    juce::TextButton harmonyViewButton { "Harmony" }, grooveViewButton { "Groove & Feel" };
    bool grooveView = false;
    TimelineView timeline;
    juce::TextButton midiViewBtn { "MIDI view" };
    std::unique_ptr<juce::TooltipWindow> tooltips;
    ChordPickerOverlay pickerOverlay;

    // Lane Toolbar controls
    juce::ComboBox moodBox, styleBox, voicingBox;
    juce::TextButton shapeBox { juce::String::fromUTF8 ("Shape Stamps ▾") };
    juce::TextButton bassPatternBox;
    juce::Label moodLabel, styleLabel, voicingLabel, shapeLabel, bassPatternLabel;
    juce::TextButton resetCurvesBtn { "Reset" };
    juce::TextButton analyzeMidiBtn;
    juce::Slider melodyFit;
    juce::Label melodyFitLabel;

    LanesView lanes;

    // Footer Panel 1: Harmony Generator
    juce::TextButton lockRhythm { "Rhythm" }, lockRoots { "Roots" }, lockColors { "Colors" }, lockVoicing { "Voicing" };
    juce::Label lockCaption;
    juce::Slider adventure, variety, syncopation, bassMotion;
    juce::ComboBox endingBox;
    juce::Label endingLabel;
    juce::Label adventureLabel, varietyLabel, syncopationLabel, bassMotionLabel;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, rollButton { "ROLL" };

    // Footer Panel 2: Groove & Feel
    juce::TextButton compBox;
    juce::Label compLabel;
    void showPerformanceBrowser();
    void showBassPatternBrowser();
    void refreshCompingSelection();
    juce::Slider strum, accent, humanize;
    juce::Label strumLabel, accentLabel, humanizeLabel;
    juce::TextButton refeelButton { "Re-Feel" };

    // Footer Panel 3: Output & Export
    juce::ComboBox synthModelBox;
    juce::Label synthModelLabel;
    juce::Slider gate, volume;
    juce::Label gateLabel, volumeLabel;
    juce::TextButton playButton { "Play" }, saveButton { "Save MIDI" };
    juce::TextButton splitMidiButton { "Split Ch 1/2" };
    juce::TextButton songButton { "Song" };
    MidiDragTile dragTile;
    MidiDragTile dragChordsTile;
    MidiDragTile dragBassTile;
    MidiDragTile dragSongTile;

    // Context / Hint bar & Targeted Rolls
    juce::Label hint;
    juce::TextButton rollRootBtn { "Roll Root" }, rollColorBtn { "Roll Color" }, rollBassBtn { "Change Bass" }, inspectBtn { "Inspect" };

    void updateContextBar (int slot);

    // Attachments
    juce::AudioProcessorValueTreeState::SliderAttachment gateAttachment, volumeAttachment,
        strumAttachment, accentAttachment, humanizeAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment splitMidiAttachment;
    std::unique_ptr<juce::ParameterAttachment> compAttachment, chordRhythmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> synthModelAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    int keyNamesScale = -1;
    bool refreshing = false;

    juce::Rectangle<int> targetCurvesBounds, harmonyPanelBounds, groovePanelBounds, previewPanelBounds, outputPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsEditor)
};
