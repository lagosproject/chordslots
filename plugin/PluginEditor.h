// ChordSlots - editor UI.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Localization.h"
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
    void selectVariation (int index);
    void updateVariationControls();
    void showProgressionLibrary();
    void updateTooltips();
    void showSongView();
    void showKeyActions();
    void saveMidi (ChordSlotsProcessor::MidiExportMode mode);
    void showMidiImportMenu();
    enum class MidiImport { Curves, Melody, Chords };
    void importMidi (MidiImport);
    void importMidiFile (const juce::File&, MidiImport);
    void showChordImport (const juce::File&);
    void updateMidiImportLabel (const cs::Progression&);
    void applyShapeStamp (int stampId);
    bool keyPressed (const juce::KeyPress& key) override;

    ChordSlotsProcessor& processor;
    ui::EditorLocale locale;
    ChordSlotsLookAndFeel lnf;

    // Header controls
    juce::ComboBox keyBox, scaleBox, barsBox, meterBox, slotsBox, snapBox;
    juce::Label keyLabel, barsLabel, meterLabel, slotsLabel, snapLabel;
    juce::TextButton keyActionsButton { ui::tr ("Key Actions") };
    juce::TextButton inputButton { ui::tr (u8"MIDI In \u25be") };
    juce::TextButton optionsButton { ui::tr ("Options") };
    juce::Label fitLabel;
    juce::TextButton loopSummary, moreOptions { ui::tr ("More options") }, playAfterRoll { ui::tr ("Play after Roll") }, gestures { ui::tr ("Gestures") };
    juce::Label gestureHelp;
    int inspectedSlot = -1;

    // Variations
    juce::TextButton varButtons[cs::kNumVariations];
    juce::ComboBox variationBox;
    juce::TextButton addVariationButton { "+" }, progressionLibraryButton { ui::tr ("Progressions") };
    juce::Component::SafePointer<juce::DialogWindow> progressionLibraryWindow;
    juce::TextButton followButton { ui::tr ("Follow") };
    juce::TextButton copyVarButton { ui::tr ("Copy...") };

    // Views
    juce::TextButton harmonyViewButton { ui::tr ("Harmony") }, grooveViewButton { ui::tr ("Feel") };
    bool grooveView = false;
    TimelineView timeline;
    juce::TextButton midiViewBtn { ui::tr ("Notes") };
    std::unique_ptr<juce::TooltipWindow> tooltips;
    ChordPickerOverlay pickerOverlay;

    // Lane Toolbar controls
    juce::ComboBox moodBox, styleBox, voicingBox;
    juce::TextButton shapeBox { ui::tr (u8"Shape Stamps \u25be") };
    juce::TextButton bassPatternBox;
    juce::Label moodLabel, styleLabel, voicingLabel, bassPatternLabel;
    juce::TextButton resetCurvesBtn { ui::tr ("Reset") };
    juce::TextButton startMelodyButton;
    juce::TextButton analyzeMidiBtn { ui::tr ("MIDI options") };
    juce::TextButton melodyAuditionButton { ui::tr ("Hear melody") };
    juce::TextButton melodyTimingButton { ui::tr ("Original timing") }, midiDetailsButton { ui::tr ("Details") };
    juce::TextButton autoMelodyKeyButton { ui::tr ("Auto Key & Scale") };
    juce::Label midiFilename, midiKeyDetails, midiTempoDetails;
    bool midiDragOver = false;
    juce::Slider melodyFit;
    juce::Label melodyFitLabel;

    LanesView lanes;

    // Footer Panel 1: Harmony Generator
    juce::TextButton lockRhythm { ui::tr ("Rhythm") }, lockRoots { ui::tr ("Roots") }, lockColors { ui::tr ("Colors") }, lockVoicing { ui::tr ("Voicing") };
    juce::Label lockCaption;
    juce::Slider adventure, variety, syncopation, bassMotion;
    juce::ComboBox endingBox;
    juce::Label endingLabel;
    juce::Label adventureLabel, varietyLabel, syncopationLabel, bassMotionLabel;
    juce::TextButton undoButton { ui::tr ("Undo") }, redoButton { ui::tr ("Redo") }, rollButton { ui::tr ("Roll Chords") };
    juce::TextButton rollSettingsButton { ui::tr ("Roll settings") };
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
    juce::TextButton arpAuditionButton { ui::tr ("Hear arp") }, arpSaveButton { ui::tr ("Save arp") };
    void showPerformanceBrowser();
    void showBassPatternBrowser();
    void refreshCompingSelection();
    void applyCompStyle (cs::CompStyle);
    juce::Slider strum, accent, humanize;
    juce::Label strumLabel, accentLabel, humanizeLabel;
    juce::TextButton refeelButton { ui::tr ("Roll feel") };

    // Footer Panel 3: Output & Export
    juce::ComboBox synthModelBox;
    juce::Label synthModelLabel;
    juce::Slider gate, volume;
    juce::Slider standaloneTempo;
    juce::TextButton autoMidiTempoButton { ui::tr ("Auto") };
    juce::Label gateLabel, volumeLabel;
    juce::TextButton playButton { ui::tr ("Play") }, saveButton { ui::tr ("Save MIDI") };
    juce::TextButton splitMidiButton { ui::tr ("Split Ch 1/2") };
    juce::TextButton songButton { ui::tr ("Song") };
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
    std::unique_ptr<juce::ParameterAttachment> followMelodyRhythmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpRateAttachment, arpOctavesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpLengthAttachment, compPatternAccentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> synthModelAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    int keyNamesScale = -1;
    bool refreshing = false;

    juce::Rectangle<int> workspaceTabBounds, stageTitleBounds;
    juce::Rectangle<int> targetCurvesBounds, railBounds, footerBounds, footerSeparator, midiPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsEditor)
};
