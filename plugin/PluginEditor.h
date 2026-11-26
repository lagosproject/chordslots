// ChordSlots - editor UI.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/TimelineView.h"
#include "ui/LanesView.h"
#include "ui/MidiDragTile.h"
#include "ui/ChordPickerOverlay.h"
#include "ui/SongPanel.h"

//==============================================================================
class ChordSlotsEditor final : public juce::AudioProcessorEditor,
                               public juce::FileDragAndDropTarget,
                               private juce::ChangeListener,
                               private juce::Timer
{
public:
    explicit ChordSlotsEditor (ChordSlotsProcessor&);
    ~ChordSlotsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void updateDragTileVisibility();
    void updateTransport();
    void setRollSettingsOpen (bool open);
    void showGrooveView (bool show);
    void rebuildKeyNames (int scale);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& caption, const juce::String& tooltip);
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& caption, const juce::String& tooltip);
    void showInputMenu();
    void showOptionsMenu();
    void updateTooltips();
    void showSongView();
    void showKeyActions();
    void saveMidi (ChordSlotsProcessor::MidiExportMode mode);
    void showMidiImportMenu();
    void importMidi (bool harmonize);
    void importMidiFile (const juce::File&, bool harmonize);
    void updateMidiImportLabel (const cs::Progression&);
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
    juce::Label fitLabel;
    juce::TextButton loopSummary, moreOptions { "More options" }, playAfterRoll { "Play after Roll" }, gestures { "Gestures" };
    juce::Label gestureHelp;
    int inspectedSlot = -1;

    // Variations
    juce::TextButton varButtons[cs::kNumVariations];
    juce::TextButton followButton { "Follow" };
    juce::TextButton copyVarButton { "Copy..." };

    // Views
    juce::TextButton harmonyViewButton { "Harmony" }, grooveViewButton { "Feel" };
    bool grooveView = false;
    TimelineView timeline;
    juce::TextButton midiViewBtn { "Notes" };
    std::unique_ptr<juce::TooltipWindow> tooltips;
    ChordPickerOverlay pickerOverlay;

    // Lane Toolbar controls
    juce::ComboBox moodBox, styleBox, voicingBox;
    juce::TextButton shapeBox { juce::String::fromUTF8 ("Shape Stamps ▾") };
    juce::TextButton bassPatternBox;
    juce::Label moodLabel, styleLabel, voicingLabel, bassPatternLabel;
    juce::TextButton resetCurvesBtn { "Reset" };
    juce::TextButton startMelodyButton;
    juce::TextButton analyzeMidiBtn { "MIDI options" };
    juce::TextButton melodyAuditionButton { "Hear melody" };
    juce::TextButton melodyTimingButton { "Original timing" }, midiDetailsButton { "Details" };
    juce::TextButton autoMelodyKeyButton { "Auto Key & Scale" };
    juce::Label midiFilename, midiKeyDetails, midiTempoDetails;
    bool midiDragOver = false;
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
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, rollButton { "Roll Chords" };
    juce::TextButton rollSettingsButton { "Roll settings" };
    struct RollSettingsPanel final : juce::Component
    {
        void paint (juce::Graphics&) override;
    } rollSettingsPanel;

    // Footer Panel 2: Groove & Feel
    juce::TextButton compBox;
    juce::Label compLabel;
    juce::ComboBox arpDirectionBox, arpRateBox, arpOctavesBox, pulseRateBox;
    juce::Label arpDirectionLabel, arpRateLabel, arpLengthLabel, arpOctavesLabel, pulseRateLabel, compPatternAccentLabel;
    juce::Slider arpLength, compPatternAccent;
    juce::TextButton arpAuditionButton { "Hear arp" }, arpSaveButton { "Save arp" };
    void showPerformanceBrowser();
    void showBassPatternBrowser();
    void refreshCompingSelection();
    juce::Slider strum, accent, humanize;
    juce::Label strumLabel, accentLabel, humanizeLabel;
    juce::TextButton refeelButton { "Roll feel" };

    // Footer Panel 3: Output & Export
    juce::ComboBox synthModelBox;
    juce::Label synthModelLabel;
    juce::Slider gate, volume;
    juce::Slider standaloneTempo;
    juce::TextButton autoMidiTempoButton { "Auto" };
    juce::Label gateLabel, volumeLabel;
    juce::TextButton playButton { "Play" }, saveButton { "Save MIDI" };
    juce::TextButton splitMidiButton { "Split Ch 1/2" };
    juce::TextButton songButton { "Song" };
    MidiDragTile dragTile;
    MidiDragTile dragChordsTile;
    MidiDragTile dragBassTile;
    MidiDragTile dragSongTile;
    MidiDragTile dragArpTile;
    SongPanel songPanel;

    // Context / Hint bar & Targeted Rolls


    // Attachments
    juce::AudioProcessorValueTreeState::SliderAttachment gateAttachment, volumeAttachment,
        strumAttachment, accentAttachment, humanizeAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment splitMidiAttachment;
    std::unique_ptr<juce::ParameterAttachment> compAttachment, chordRhythmAttachment, pulseRateAttachment, arpDirectionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpRateAttachment, arpOctavesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpLengthAttachment, compPatternAccentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> synthModelAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    int keyNamesScale = -1;
    bool refreshing = false;

    juce::Rectangle<int> workspaceTabBounds;
    juce::Rectangle<int> targetCurvesBounds, railBounds, footerBounds, footerSeparator, midiPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsEditor)
};
