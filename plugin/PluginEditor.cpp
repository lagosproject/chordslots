// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "EditorPreferences.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

constexpr auto showTooltipsKey = "showTooltips";

juce::PropertiesFile& editorSettings()
{
    static juce::PropertiesFile settings ([]
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "ChordSlots";
        options.filenameSuffix = "editor.settings";
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = 0;
        return options;
    }());
    return settings;
}

}

bool editorPreferences::tooltipsEnabled()
{
    return editorSettings().getBoolValue (showTooltipsKey, true);
}

void editorPreferences::setTooltipsEnabled (bool enabled)
{
    editorSettings().setValue (showTooltipsKey, enabled);
}

ChordSlotsEditor::ChordSlotsEditor (ChordSlotsProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      timeline (p),
      pickerOverlay (p),
      lanes (p),
      dragTile (p, ChordSlotsProcessor::MidiExportMode::Master, "Drag MIDI"),
      dragChordsTile (p, ChordSlotsProcessor::MidiExportMode::ChordsOnly, "Chords"),
      dragBassTile (p, ChordSlotsProcessor::MidiExportMode::BassOnly, "Bass"),
      dragSongTile (p, ChordSlotsProcessor::MidiExportMode::Song, "Drag Song"),
      gateAttachment (p.apvts, "gate", gate),
      volumeAttachment (p.apvts, "synthVol", volume),
      strumAttachment (p.apvts, "strum", strum),
      accentAttachment (p.apvts, "accent", accent),
      humanizeAttachment (p.apvts, "humanize", humanize),
      splitMidiAttachment (p.apvts, "splitMidi", splitMidiButton)
{
    setLookAndFeel (&lnf);

    // Header Combos
    setupCombo (keyBox, keyLabel, "KEY",
              "Transpose the progression to a different root note.");
    setupCombo (scaleBox, scaleLabel, "SCALE",
              "Choose the scale and generate chords for it using the current slot timings.");
    setupCombo (barsBox, barsLabel, "BARS",
              "Set the length of the progression in bars.");
    setupCombo (meterBox, meterLabel, "METER",
              "Choose the time signature. Host follows your DAW.");
    setupCombo (slotsBox, slotsLabel, "SLOTS",
              "Set the number of chord slots. Use the timeline to add or remove individual cuts.");
    setupCombo (snapBox, snapLabel, "GRID",
              "Set the note division used to snap timeline cuts.");

    for (int pc = 0; pc < 12; ++pc)
        keyBox.addItem (utf8 (cs::pitchClassName (pc, pc, 0)), pc + 1);

    for (int i = 0; i < cs::numScales(); ++i)
        scaleBox.addItem (cs::scales()[(size_t) i].name, i + 1);

    for (int b : { 1, 2, 4, 8 })
        barsBox.addItem (juce::String (b), b);

    meterBox.addItem ("4/4", 1);
    meterBox.addItem ("3/4", 2);
    meterBox.addItem ("6/8", 3);
    meterBox.addItem ("Host", 4);

    for (int n = 1; n <= cs::kMaxSlots; ++n)
        slotsBox.addItem (juce::String (n), n);

    snapBox.addItem ("1/4", 24);
    snapBox.addItem ("1/4 T", 16);
    snapBox.addItem ("1/8", 12);
    snapBox.addItem ("1/8 T", 8);
    snapBox.addItem ("1/16", 6);
    snapBox.addItem ("1/16 T", 4);
    snapBox.addItem ("1/32", 3);

    // Input / Trigger popover button
    inputButton.setTooltip ("Configure incoming MIDI trigger mode and note mapping");
    inputButton.onClick = [this] { showInputMenu(); };
    addAndMakeVisible (inputButton);

    optionsButton.setTooltip ("Change editor preferences such as hover help.");
    optionsButton.onClick = [this] { showOptionsMenu(); };
    #if ! JUCE_IOS && ! JUCE_ANDROID
    if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
    #endif
        addAndMakeVisible (optionsButton);
    updateTooltips();

    keyBox.onChange = [this]
    {
        if (refreshing) return;
        const int tonic = keyBox.getSelectedId() - 1;
        processor.edit ([tonic] (cs::Progression& pr, cs::Rng&) { cs::setTonic (pr, tonic); });
    };

    scaleBox.onChange = [this]
    {
        if (refreshing) return;
        const int scale = scaleBox.getSelectedId() - 1;
        processor.edit ([scale] (cs::Progression& pr, cs::Rng& rng)
        {
            pr.scale = scale;
            const bool keep = pr.locks.rhythm;
            pr.locks.rhythm = true; // a new scale needs new chords, not new cuts
            cs::roll (pr, rng);
            pr.locks.rhythm = keep;
        });
    };

    barsBox.onChange = [this]
    {
        if (refreshing) return;
        const int bars = barsBox.getSelectedId();
        processor.edit ([bars] (cs::Progression& pr, cs::Rng&) { cs::setBars (pr, bars); });
    };

    meterBox.onChange = [this]
    {
        if (refreshing) return;
        const int id = meterBox.getSelectedId();
        if (id == 4)
        {
            processor.setFollowHostMeter (true);
            const int hNum = processor.getHostTimeSigNum();
            const int hDen = processor.getHostTimeSigDen();
            if (hNum > 0 && hDen > 0)
                processor.setMeter (hNum, hDen);
        }
        else
        {
            processor.setFollowHostMeter (false);
            int num = 4, den = 4;
            if (id == 2) { num = 3; den = 4; }
            else if (id == 3) { num = 6; den = 8; }
            processor.setMeter (num, den);
        }
    };

    slotsBox.onChange = [this]
    {
        if (refreshing) return;
        const int n = slotsBox.getSelectedId();
        processor.edit ([n] (cs::Progression& pr, cs::Rng& rng)
        {
            const int previousCount = pr.numSlots;
            cs::setNumSlots (pr, n);
            if (pr.numSlots == previousCount)
                return;
            if (pr.mood >= 0)
            {
                const int scale = pr.scale;
                const float adv = pr.adventure, sync = pr.syncopation;
                cs::applyMood (pr, pr.mood, false); // re-spread curves over new slot count
                pr.scale = scale;
                pr.adventure = adv;
                pr.syncopation = sync;
            }
            cs::roll (pr, rng);
        });
    };

    snapBox.onChange = [this]
    {
        if (refreshing) return;
        const int snap = snapBox.getSelectedId();
        processor.edit ([snap] (cs::Progression& pr, cs::Rng&) { pr.snap = snap; }, false);
    };

    moodBox.onChange = [this]
    {
        if (refreshing || moodBox.getSelectedId() == 0) return;
        const int mood = moodBox.getSelectedId() - 1;
        processor.edit ([mood] (cs::Progression& pr, cs::Rng& rng)
        {
            cs::applyMood (pr, mood);
            cs::roll (pr, rng);
        });
    };

    styleBox.onChange = [this]
    {
        if (refreshing || styleBox.getSelectedId() == 0) return;
        const int id = styleBox.getSelectedId();
        const int style = (id == 1 ? -1 : id - 2);
        processor.edit ([style] (cs::Progression& pr, cs::Rng&)
        {
            pr.style = style;
        });
    };

    voicingBox.onChange = [this]
    {
        if (refreshing || voicingBox.getSelectedId() == 0) return;
        const int style = voicingBox.getSelectedId() - 1;
        processor.edit ([style] (cs::Progression& pr, cs::Rng& rng)
        {
            pr.voicingStyle = style;
            cs::revoice (pr, rng);
        });
    };

    // Fit score badge
    fitCaption.setText ("MATCH", juce::dontSendNotification);
    fitCaption.setTooltip ("How closely the current progression matches your target curves.");
    fitLabel.setTooltip (fitCaption.getTooltip());
    fitCaption.setFont (ui::font (9.0f, true));
    fitCaption.setColour (juce::Label::textColourId, ui::textDim);
    fitCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fitCaption);

    fitLabel.setFont (ui::font (18.0f, true));
    fitLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fitLabel);

    // Variation bank
    variationLabel.setText ("VARIATION", juce::dontSendNotification);
    variationLabel.setFont (ui::font (10.0f, true));
    variationLabel.setColour (juce::Label::textColourId, ui::textDim);
    variationLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (variationLabel);

    followButton.setButtonText ("Follow");
    followButton.setClickingTogglesState (true);
    followButton.setTooltip ("When enabled, editing follows the playing variation");
    followButton.onClick = [this]
    {
        processor.setFollowPlayback (followButton.getToggleState());
    };
    addAndMakeVisible (followButton);

    const char* const varNames[] = { "A", "B", "C", "D" };
    for (int i = 0; i < cs::kNumVariations; ++i)
    {
        varButtons[i].setButtonText (varNames[i]);
        varButtons[i].setClickingTogglesState (false);
        varButtons[i].setTooltip (juce::String ("Switch to Variation ") + varNames[i]);
        varButtons[i].onClick = [this, i]
        {
            processor.setEditVariation (i);
            if (processor.isFollowingPlayback())
                processor.setPlayingVariation (i);
            refresh();
        };
        addAndMakeVisible (varButtons[i]);
    }

    copyVarButton.setTooltip ("Copy current variation to another slot");
    copyVarButton.onClick = [this]
    {
        const int cur = processor.getEditVariation();
        const char names[] = { 'A', 'B', 'C', 'D' };
        juce::PopupMenu menu;
        for (int i = 0; i < cs::kNumVariations; ++i)
        {
            if (i != cur)
                menu.addItem (i + 1, juce::String ("Copy ") + names[cur] + " to " + names[i]);
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&copyVarButton),
                            [this, cur] (int result)
        {
            if (result > 0)
                processor.copyVariation (cur, result - 1);
        });
    };
    addAndMakeVisible (copyVarButton);

    // Views
    addAndMakeVisible (timeline);
    addAndMakeVisible (harmonyViewButton);
    addAndMakeVisible (grooveViewButton);
    harmonyViewButton.setTooltip ("Show target curves, chord generation controls and locks.");
    grooveViewButton.setTooltip ("Show controls for playing patterns, timing and dynamics.");
    harmonyViewButton.onClick = [this] { showGrooveView (false); };
    grooveViewButton.onClick = [this] { showGrooveView (true); };
    addAndMakeVisible (midiViewBtn);
    midiViewBtn.setClickingTogglesState (true);
    midiViewBtn.setTooltip ("Show the generated notes, including velocity, gate and strum");
    midiViewBtn.onClick = [this]
    {
        timeline.setMidiVisible (midiViewBtn.getToggleState());
        midiViewBtn.setButtonText (midiViewBtn.getToggleState() ? "Chords" : "MIDI view");
        resized();
    };
    timeline.onOpenAlternatives = [this] (int slot)
    {
        pickerOverlay.open (slot, true);
        pickerOverlay.setVisible (true);
        pickerOverlay.toFront (true);
    };

    setupCombo (moodBox, moodLabel, "MOOD PRESET",
              "Load a set of target curves and harmony settings, then generate a progression.");
    const auto& m = cs::moods();
    for (int i = 0; i < (int) m.size(); ++i)
        moodBox.addItem (m[(size_t) i].name, i + 1);

    setupCombo (styleBox, styleLabel, "CHORD TRANSITIONS",
              "Choose which chord changes Roll favors. Auto follows the mood and scale. Free removes that preference.");
    styleBox.addItem ("Auto", 1);
    styleBox.addItem ("Major", 2);
    styleBox.addItem ("Minor", 3);
    styleBox.addItem ("Extended harmony", 4);
    styleBox.addItem ("Free", 5);
    styleBox.setSelectedId (1, juce::dontSendNotification);

    setupCombo (voicingBox, voicingLabel, "VOICING",
              "Choose how chord notes are spread across registers.");
    for (int i = 0; i < cs::kNumVoicingStyles; ++i)
        voicingBox.addItem (cs::voicingStyleName ((cs::VoicingStyle) i), i + 1);
    voicingBox.setSelectedId ((int) cs::VoicingStyle::Piano + 1, juce::dontSendNotification);

    setupCombo (bassPatternBox, bassPatternLabel, "BASS PATTERN",
              "Choose how the bass plays under each chord.");
    bassPatternBox.addItem ("Sustain", (int) cs::BassStyle::Sustain + 1);
    bassPatternBox.addItem ("Octave pump", (int) cs::BassStyle::OctavePump + 1);
    bassPatternBox.addItem ("Offbeat 8ths", (int) cs::BassStyle::OffbeatEighths + 1);
    bassPatternBox.addItem ("808 legato", (int) cs::BassStyle::Legato808 + 1);
    bassPatternBox.addItem ("Walking", (int) cs::BassStyle::Walking + 1);
    bassPatternBox.onChange = [this]
    {
        if (refreshing || bassPatternBox.getSelectedId() == 0) return;
        const auto bassStyle = (cs::BassStyle) (bassPatternBox.getSelectedId() - 1);
        processor.edit ([bassStyle] (cs::Progression& pr, cs::Rng&) { pr.bassStyle = bassStyle; });
    };

    shapeLabel.setText ("SHAPES", juce::dontSendNotification);
    shapeLabel.setFont (ui::font (10.0f, true));
    shapeLabel.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (shapeLabel);
    addAndMakeVisible (shapeBox);
    shapeBox.setTooltip ("Apply a shape to all three target curves and roll chords to fit it.");
    shapeLabel.setTooltip (shapeBox.getTooltip());
    shapeBox.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Rise (Gradual build)");
        menu.addItem (2, "Drop (Gradual release)");
        menu.addItem (3, "Arch (Peak center)");
        menu.addItem (4, "Pop Build (Step up)");
        menu.addItem (5, "Valley (Dip center)");
        juce::Component::SafePointer<ChordSlotsEditor> editor (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapeBox), [editor] (int stamp)
        {
            if (editor != nullptr && stamp != 0)
                editor->applyShapeStamp (stamp);
        });
    };

    resetCurvesBtn.setTooltip ("Set all three target curves to 50% and roll chords to fit them.");
    resetCurvesBtn.onClick = [this]
    {
        processor.edit ([] (cs::Progression& pr, cs::Rng& rng)
        {
            for (int i = 0; i < pr.numSlots; ++i)
            {
                pr.slots[(size_t) i].targetB = 0.5f;
                pr.slots[(size_t) i].targetT = 0.5f;
                pr.slots[(size_t) i].targetE = 0.5f;
            }
            cs::roll (pr, rng);
        });
    };
    addAndMakeVisible (resetCurvesBtn);

    analyzeMidiBtn.setTooltip ("Analyze a MIDI file or harmonize a MIDI melody");
    analyzeMidiBtn.onClick = [this] { showMidiImportMenu(); };
    addAndMakeVisible (analyzeMidiBtn);

    setupKnob (melodyFit, melodyFitLabel, "MELODY FIT",
              "Set how strongly Roll favors chords that fit the imported melody. Set to 0% to ignore it.");
    melodyFit.setRange (0.0, 1.0, 0.01);
    melodyFit.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    melodyFit.setPopupDisplayEnabled (true, false, this);
    melodyFit.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    melodyFit.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (100.0 * value)) + " %";
    };
    melodyFit.onValueChange = [this]
    {
        if (refreshing) return;
        const float value = (float) melodyFit.getValue();
        processor.edit ([value] (cs::Progression& pr, cs::Rng&) { pr.melodyFit = value; }, false);
    };

    addAndMakeVisible (lanes);

    // Overlay
    pickerOverlay.setVisible (false);
    pickerOverlay.onClose = [this] { pickerOverlay.setVisible (false); };
    addChildComponent (pickerOverlay);

    // Footer Panel 1: Locks
    lockCaption.setText ("LOCKS", juce::dontSendNotification);
    lockCaption.setTooltip ("Locked layers keep their values when you roll.");
    lockCaption.setFont (ui::font (10.0f, true));
    lockCaption.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (lockCaption);

    struct LockDef
    {
        juce::TextButton* button;
        bool cs::Locks::* field;
        const char* tip;
    };
    for (const LockDef& d : {
        LockDef { &lockRhythm, &cs::Locks::rhythm, "Locks rhythm and slot cut positions during Roll" },
        LockDef { &lockRoots,  &cs::Locks::roots,  "Locks chord root notes during Roll" },
        LockDef { &lockColors, &cs::Locks::colors, "Locks chord types and qualities during Roll" },
        LockDef { &lockVoicing,&cs::Locks::voicing,"Locks inversions and voice registers during Roll" } })
    {
        d.button->setClickingTogglesState (true);
        d.button->setTooltip (d.tip);
        auto* button = d.button;
        const auto field = d.field;
        button->onClick = [this, button, field]
        {
            const bool on = button->getToggleState();
            processor.edit ([field, on] (cs::Progression& pr, cs::Rng&) { pr.locks.*field = on; }, false);
        };
        addAndMakeVisible (*button);
    }

    // Knobs with numeric text boxes
    setupKnob (adventure, adventureLabel, "ADVENTURE",
              "Raise this to let Roll use more borrowed chords and extensions.");
    setupKnob (variety, varietyLabel, "VARIETY",
              "Raise this to explore more chord choices each time you Roll.");
    setupKnob (syncopation, syncopationLabel, "SYNCOPATION",
              "Raise this to move more unlocked chord changes off the regular beat during Roll.");
    setupKnob (bassMotion, bassMotionLabel, "BASS MOTION",
              "Raise this to let Roll use inversions for smoother movement between bass notes.");
    setupKnob (strum, strumLabel, "STRUM",
              "Spread the upper chord notes in time. Positive plays low to high. Negative plays high to low.");
    setupKnob (accent, accentLabel, "ACCENT",
              "Raise this to emphasize the highest chord note and soften the inner notes.");
    setupKnob (humanize, humanizeLabel, "HUMANIZE",
              "Add small variations in note timing and velocity for a more natural feel.");
    setupKnob (gate, gateLabel, "GATE",
              "Set how long notes are held relative to their rhythmic duration.");
    setupKnob (volume, volumeLabel, "SYNTH VOL",
              "Adjust the loudness of the built-in preview instrument.");

    adventure.setRange (0.0, 1.0, 0.01);
    variety.setRange (0.0, 1.0, 0.01);
    syncopation.setRange (0.0, 1.0, 0.01);
    bassMotion.setRange (0.0, 1.0, 0.01);
    strum.setRange (-1.0, 1.0, 0.01);
    accent.setRange (0.0, 1.0, 0.01);
    humanize.setRange (0.0, 1.0, 0.01);

    adventure.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    variety.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    syncopation.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    bassMotion.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    strum.textFromValueFunction = [] (double v)
    {
        const int ms = juce::roundToInt (v * 80.0);
        return (ms > 0 ? "+" : "") + juce::String (ms) + " ms";
    };
    accent.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    humanize.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 6.0)) + " ms"; };
    gate.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    volume.textFromValueFunction = [] (double v)
    {
        return juce::String (juce::Decibels::gainToDecibels ((float) v, -48.0f), 1) + " dB";
    };

    adventure.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    variety.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    syncopation.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    bassMotion.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    strum.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 80.0; };
    accent.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    humanize.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 6.0; };
    gate.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    volume.valueFromTextFunction = [] (const juce::String& s)
    {
        return (double) juce::Decibels::decibelsToGain ((float) s.getDoubleValue(), -48.0f);
    };

    // APVTS attachments seed their sliders before these functions are installed;
    // force the existing text boxes to pick up the unit-aware representations.
    for (auto* slider : { &adventure, &variety, &syncopation, &bassMotion,
                          &strum, &accent, &humanize, &gate, &volume })
        slider->updateText();

    adventure.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    variety.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    syncopation.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    bassMotion.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    strum.setColour (juce::Slider::rotarySliderFillColourId, ui::energy);
    accent.setColour (juce::Slider::rotarySliderFillColourId, ui::energy);
    humanize.setColour (juce::Slider::rotarySliderFillColourId, ui::energy);
    gate.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    volume.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);

    adventure.onValueChange = [this]
    {
        if (refreshing) return;
        const float val = (float) adventure.getValue();
        processor.edit ([val] (cs::Progression& pr, cs::Rng&) { pr.adventure = val; }, false);
    };
    variety.onValueChange = [this]
    {
        if (refreshing) return;
        const float value = (float) variety.getValue();
        processor.edit ([value] (cs::Progression& pr, cs::Rng&) { pr.variety = value; }, false);
    };
    syncopation.onValueChange = [this]
    {
        if (refreshing) return;
        const float val = (float) syncopation.getValue();
        processor.edit ([val] (cs::Progression& pr, cs::Rng&) { pr.syncopation = val; }, false);
    };
    bassMotion.onValueChange = [this]
    {
        if (refreshing) return;
        const float val = (float) bassMotion.getValue();
        processor.edit ([val] (cs::Progression& pr, cs::Rng&) { pr.bassMotion = val; }, false);
    };

    // Groove comping combo
    setupCombo (compBox, compLabel, "COMPING", "Choose the rhythm used to play the chords.");
    compBox.addItem ("Sustain", 1);
    compBox.addItem ("1/4 Pulse", 2);
    compBox.addItem ("1/8 Pulse", 3);
    compBox.addItem ("Offbeat Reggae", 4);
    compBox.addItem ("Ballad Stabs", 5);
    compBox.addItem ("Trap Stabs", 6);
    compBox.addItem ("Triplet Swing", 7);
    compAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "compStyle", compBox);

    // Synth sound model combo
    setupCombo (synthModelBox, synthModelLabel, "SOUND",
              "Choose the instrument sound for the built-in preview.");
    synthModelBox.addItem ("Clean Saw", 1);
    synthModelBox.addItem ("Rhodes EP", 2);
    synthModelBox.addItem ("Felt Piano", 3);
    synthModelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "synthModel", synthModelBox);
#if defined (CHORDSLOTS_MIDI_FX) && CHORDSLOTS_MIDI_FX
    synthModelLabel.setVisible (false);
    synthModelBox.setVisible (false);
    volumeLabel.setVisible (false);
    volume.setVisible (false);
#endif

    // Buttons
    refeelButton.setTooltip ("Re-humanize groove with a fresh timing and velocity seed");
    refeelButton.onClick = [this] { processor.refeel(); };
    addAndMakeVisible (refeelButton);

    undoButton.getProperties().set ("icon", (int) ui::ButtonIcon::Undo);
    undoButton.onClick = [this] { processor.undo(); };
    addAndMakeVisible (undoButton);

    rollButton.getProperties().set ("primary", true);
    rollButton.setTooltip ("Generate new chords for all unlocked slots");
    rollButton.onClick = [this]
    {
        processor.edit ([] (cs::Progression& pr, cs::Rng& rng) { cs::roll (pr, rng); });
    };
    addAndMakeVisible (rollButton);

    playButton.getProperties().set ("icon", (int) ui::ButtonIcon::Transport);
    playButton.setTooltip ("Toggle built-in preview synth playback");
    playButton.onClick = [this] { processor.setPreviewPlaying (! processor.isPreviewPlaying()); };
    addAndMakeVisible (playButton);

    saveButton.getProperties().set ("icon", (int) ui::ButtonIcon::Save);
    saveButton.setTooltip ("Export progression as standard MIDI file");
    saveButton.onClick = [this] { saveMidi(); };
    addAndMakeVisible (saveButton);

    splitMidiButton.setClickingTogglesState (true);
    splitMidiButton.setTooltip ("Route upper chords to MIDI Ch 1 and monophonic bass to MIDI Ch 2");
    addAndMakeVisible (splitMidiButton);
    addAndMakeVisible (songButton);
    songButton.setToggleState (p.isSongEnabled(), juce::dontSendNotification);
    songButton.setTooltip ("Edit up to 16 song sections: choose variation and transpose, then enable Song playback");
    songButton.onClick = [this] { showSongMenu(); };

    addAndMakeVisible (dragTile);
    addAndMakeVisible (dragChordsTile);
    addAndMakeVisible (dragBassTile);
    addAndMakeVisible (dragSongTile);
    dragTile.setTooltip ("Drag the current progression onto a MIDI track in your DAW.");
    dragChordsTile.setTooltip ("Drag just the upper chord notes onto a MIDI track in your DAW.");
    dragBassTile.setTooltip ("Drag just the bass notes onto a MIDI track in your DAW.");
    dragSongTile.setTooltip ("Drag the full Song chain onto a MIDI track in your DAW.");

    // Context / Hint Bar & Targeted Rolls
    rollRootBtn.setTooltip ("Reroll only the root of this slot");
    rollRootBtn.onClick = [this]
    {
        const int s = timeline.getSelectedSlot();
        if (s >= 0)
            processor.edit ([s] (cs::Progression& pr, cs::Rng& rng) { cs::rollSlotRoot (pr, s, rng); });
    };
    addChildComponent (rollRootBtn);

    rollColorBtn.setTooltip ("Reroll only the chord type/quality of this slot");
    rollColorBtn.onClick = [this]
    {
        const int s = timeline.getSelectedSlot();
        if (s >= 0)
            processor.edit ([s] (cs::Progression& pr, cs::Rng& rng) { cs::rollSlotColor (pr, s, rng); });
    };
    addChildComponent (rollColorBtn);

    rollBassBtn.setTooltip ("Cycle or reroll the bass inversion of this slot");
    rollBassBtn.onClick = [this]
    {
        const int s = timeline.getSelectedSlot();
        if (s >= 0)
            processor.edit ([s] (cs::Progression& pr, cs::Rng&) { cs::rollSlotBass (pr, s); });
    };
    addChildComponent (rollBassBtn);

    inspectBtn.setTooltip ("Open full chord inspector & alternative substitutions");
    inspectBtn.onClick = [this]
    {
        const int s = timeline.getSelectedSlot();
        if (s >= 0)
        {
            pickerOverlay.open (s, false);
            pickerOverlay.setVisible (true);
            pickerOverlay.toFront (true);
        }
    };
    addChildComponent (inspectBtn);

    timeline.onSlotSelected = [this] (int s)
    {
        updateContextBar (s);
        if (s >= 0 && pickerOverlay.isVisible())
            pickerOverlay.setSlot (s);
    };

    hint.setFont (ui::font (12.0f));
    hint.setColour (juce::Label::textColourId, ui::textDim);
    hint.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (hint);

    processor.addChangeListener (this);
    refresh();

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    showGrooveView (false);
    setResizeLimits (900, 570, 1920, 1200);
    setSize (1040, 650);
    startTimerHz (30);
}

ChordSlotsEditor::~ChordSlotsEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ChordSlotsEditor::setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& caption,
                                  const juce::String& tooltip)
{
    box.setTooltip (tooltip);
    label.setTooltip (tooltip);
    label.setText (caption, juce::dontSendNotification);
    label.setFont (ui::font (10.0f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void ChordSlotsEditor::setupKnob (juce::Slider& s, juce::Label& label, const juce::String& caption,
                                 const juce::String& tooltip)
{
    s.setTooltip (tooltip);
    label.setTooltip (tooltip);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (s);

    label.setText (caption, juce::dontSendNotification);
    label.setFont (ui::font (8.5f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void ChordSlotsEditor::showOptionsMenu()
{
    juce::PopupMenu menu;
    const bool enabled = editorPreferences::tooltipsEnabled();
    menu.addItem (1, "Show Tooltips", true, enabled);

    juce::Component::SafePointer<ChordSlotsEditor> editor (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsButton), [editor] (int result)
    {
        if (editor != nullptr && result == 1)
        {
            editorPreferences::setTooltipsEnabled (! editorPreferences::tooltipsEnabled());
            editor->updateTooltips();
        }
    });
}

void ChordSlotsEditor::updateTooltips()
{
    if (editorPreferences::tooltipsEnabled())
    {
        if (tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, 900);
    }
    else
    {
        tooltips.reset();
    }
}

void ChordSlotsEditor::showSongMenu()
{
    const auto song = processor.getSong();
    juce::PopupMenu menu;
    menu.addSectionHeader ("SONG CHAIN  " + juce::String (song.numEntries) + "/" + juce::String (cs::kMaxSongEntries));
    menu.addItem (1, "Song playback", true, processor.isSongEnabled());
    menu.addSeparator();

    for (int index = 0; index < song.numEntries; ++index)
    {
        const auto entry = cs::sanitizeSongEntry (song.entries[index]);
        const auto variationName = juce::String::charToString ((juce::juce_wchar) ('A' + entry.variation));
        const auto transposeName = (entry.transpose > 0 ? "+" : "") + juce::String (entry.transpose) + " st";
        auto entryNumber = juce::String (index + 1);
        if (index < 9) entryNumber = "0" + entryNumber;
        const auto title = entryNumber + "  " + variationName + "  " + transposeName;
        juce::PopupMenu entryMenu;

        juce::PopupMenu variationMenu;
        for (int variation = 0; variation < cs::kNumVariations; ++variation)
        {
            const int command = 1000 + index * cs::kNumVariations + variation;
            const auto name = juce::String::charToString ((juce::juce_wchar) ('A' + variation));
            variationMenu.addItem (command, "Variation " + name, true, variation == entry.variation);
        }
        entryMenu.addSubMenu ("Variation", variationMenu);

        juce::PopupMenu transposeMenu;
        for (int first = cs::kMinSongTranspose; first <= cs::kMaxSongTranspose; first += 12)
        {
            const int last = juce::jmin (cs::kMaxSongTranspose, first + 11);
            juce::PopupMenu groupMenu;
            for (int semitones = first; semitones <= last; ++semitones)
            {
                const int command = 2000 + index * 100 + (semitones - cs::kMinSongTranspose);
                const auto label = (semitones > 0 ? "+" : "") + juce::String (semitones) + " semitones";
                groupMenu.addItem (command, label, true, semitones == entry.transpose);
            }
            const auto rangeLabel = juce::String (first) + " to " + juce::String (last);
            transposeMenu.addSubMenu (rangeLabel, groupMenu);
        }
        entryMenu.addSubMenu ("Transpose", transposeMenu);
        entryMenu.addSeparator();
        entryMenu.addItem (1100 + index, "Remove section");
        menu.addSubMenu (title, entryMenu);
    }

    menu.addSeparator();
    juce::PopupMenu addMenu;
    for (int variation = 0; variation < cs::kNumVariations; ++variation)
    {
        const auto name = juce::String::charToString ((juce::juce_wchar) ('A' + variation));
        addMenu.addItem (10 + variation, "Add variation " + name, song.numEntries < cs::kMaxSongEntries);
    }
    menu.addSubMenu ("Add section", addMenu, song.numEntries < cs::kMaxSongEntries);
    menu.addItem (2, "Clear chain", song.numEntries > 0);

    juce::Component::SafePointer<ChordSlotsEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&songButton), [safeThis] (int result)
    {
        auto* editor = safeThis.getComponent();
        if (editor == nullptr) return;
        if (result == 1)
        {
            editor->processor.setSongEnabled (! editor->processor.isSongEnabled());
            editor->songButton.setToggleState (editor->processor.isSongEnabled(), juce::dontSendNotification);
            editor->resized();
            return;
        }

        auto updated = editor->processor.getSong();
        if (result == 2)
        {
            editor->processor.setSong ({});
            return;
        }
        if (result >= 10 && result < 10 + cs::kNumVariations)
        {
            if (updated.numEntries < cs::kMaxSongEntries)
                updated.entries[updated.numEntries++] = { result - 10, 0 };
            editor->processor.setSong (updated);
            return;
        }
        if (result >= 1000 && result < 1000 + cs::kMaxSongEntries * cs::kNumVariations)
        {
            const int index = (result - 1000) / cs::kNumVariations;
            const int variation = (result - 1000) % cs::kNumVariations;
            if (index < updated.numEntries)
                updated.entries[index].variation = variation;
            editor->processor.setSong (updated);
            return;
        }
        if (result >= 1100 && result < 1100 + cs::kMaxSongEntries)
        {
            const int index = result - 1100;
            if (index < updated.numEntries)
            {
                for (int i = index; i + 1 < updated.numEntries; ++i)
                    updated.entries[i] = updated.entries[i + 1];
                updated.entries[--updated.numEntries] = {};
            }
            editor->processor.setSong (updated);
            return;
        }
        if (result >= 2000 && result < 2000 + cs::kMaxSongEntries * 100)
        {
            const int index = (result - 2000) / 100;
            const int transpose = (result - 2000) % 100 + cs::kMinSongTranspose;
            if (index < updated.numEntries && transpose <= cs::kMaxSongTranspose)
                updated.entries[index].transpose = transpose;
            editor->processor.setSong (updated);
        }
    });
}

void ChordSlotsEditor::showInputMenu()
{
    const cs::TriggerConfig tc = processor.getTriggerConfig();
    juce::PopupMenu menu;
    menu.addSectionHeader ("MIDI TRIGGER MODE");
    menu.addItem (1, "Auto (Priority to live keys)", true, tc.mode == cs::TriggerMode::Auto);
    menu.addItem (2, "Trigger Only (Mute loop)", true, tc.mode == cs::TriggerMode::TriggerOnly);
    menu.addItem (3, "Loop Only (Ignore live keys)", true, tc.mode == cs::TriggerMode::LoopOnly);

    menu.addSeparator();
    menu.addSectionHeader ("NOTE TO SLOT MAPPING");
    const bool isWhite = (tc.layout == cs::TriggerLayout::WhiteKeys);
    const bool isPad = (tc.layout == cs::TriggerLayout::Chromatic && tc.rootNote == 36);
    const bool isChromC3 = (tc.layout == cs::TriggerLayout::Chromatic && tc.rootNote == 60);

    menu.addItem (10, "White Keys (C3 = Slot 0)", true, isWhite);
    menu.addItem (11, "Chromatic (C3 = Slot 0)", true, isChromC3);
    menu.addItem (12, "Pads (C1 = Slot 0)", true, isPad);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&inputButton),
                        [this] (int result)
    {
        if (result == 0) return;
        if (result >= 1 && result <= 3)
        {
            if (auto* param = processor.apvts.getParameter ("triggerMode"))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) (result - 1)));
        }
        else if (result >= 10 && result <= 12)
        {
            int root = 60, layout = 0;
            if (result == 11) { root = 60; layout = 1; }
            else if (result == 12) { root = 36; layout = 1; }

            if (auto* param = processor.apvts.getParameter ("triggerRoot"))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) root));
            if (auto* param = processor.apvts.getParameter ("triggerLayout"))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) layout));
        }
        refresh();
    });
}

void ChordSlotsEditor::rebuildKeyNames (int scale)
{
    if (scale == keyNamesScale)
        return;
    keyNamesScale = scale;
    keyBox.clear (juce::dontSendNotification);
    for (int pc = 0; pc < 12; ++pc)
        keyBox.addItem (utf8 (cs::pitchClassName (pc, pc, scale)), pc + 1);
}

void ChordSlotsEditor::refresh()
{
    const cs::Progression p = processor.getProgression();
    const juce::ScopedValueSetter<bool> guard (refreshing, true);

    rebuildKeyNames (p.scale);
    keyBox.setSelectedId (p.tonic + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (p.scale + 1, juce::dontSendNotification);
    barsBox.setSelectedId (p.bars, juce::dontSendNotification);

    if (processor.isFollowingHostMeter())
    {
        meterBox.setSelectedId (4, juce::dontSendNotification);
    }
    else
    {
        int meterId = 1;
        if (p.timeSigNum == 3 && p.timeSigDen == 4) meterId = 2;
        else if (p.timeSigNum == 6 && p.timeSigDen == 8) meterId = 3;
        meterBox.setSelectedId (meterId, juce::dontSendNotification);
    }

    slotsBox.setSelectedId (p.numSlots, juce::dontSendNotification);
    snapBox.setSelectedId (p.snap, juce::dontSendNotification);
    moodBox.setSelectedId (p.mood + 1, juce::dontSendNotification);
    styleBox.setSelectedId (p.style == -1 ? 1 : p.style + 2, juce::dontSendNotification);
    voicingBox.setSelectedId (p.voicingStyle + 1, juce::dontSendNotification);
    bassPatternBox.setSelectedId ((int) p.bassStyle + 1, juce::dontSendNotification);
    songButton.setToggleState (processor.isSongEnabled(), juce::dontSendNotification);

    lockRhythm.setToggleState (p.locks.rhythm, juce::dontSendNotification);
    lockRoots.setToggleState (p.locks.roots, juce::dontSendNotification);
    lockColors.setToggleState (p.locks.colors, juce::dontSendNotification);
    lockVoicing.setToggleState (p.locks.voicing, juce::dontSendNotification);

    adventure.setValue (p.adventure, juce::dontSendNotification);
    variety.setValue (p.variety, juce::dontSendNotification);
    syncopation.setValue (p.syncopation, juce::dontSendNotification);
    bassMotion.setValue (p.bassMotion, juce::dontSendNotification);
    melodyFit.setValue (p.melodyFit, juce::dontSendNotification);
    melodyFit.setEnabled (! p.melody.empty());
    analyzeMidiBtn.setButtonText (juce::String::fromUTF8 (p.melody.empty() ? "Import MIDI ▾" : "MIDI: Melody ▾"));

    const cs::TriggerConfig tc = processor.getTriggerConfig();
    const char* modeNames[] = { "Auto", "Trig", "Loop" };
    inputButton.setButtonText (juce::String ("In: ") + modeNames[(int) tc.mode] + " ▾");

    const int fit = juce::roundToInt (100.0f * cs::fitScore (p));
    fitLabel.setText (juce::String (fit) + "%", juce::dontSendNotification);
    fitLabel.setColour (juce::Label::textColourId, fit >= 85 ? ui::energy : (fit >= 70 ? ui::brightness : ui::tension));

    undoButton.setEnabled (processor.canUndo());
    const char varLetters[] = { 'A', 'B', 'C', 'D' };
    const int edVar = processor.getEditVariation();
    undoButton.setTooltip (juce::String ("Undo (Variation ") + varLetters[edVar] + ")");

    timeline.setProgression (p);
    lanes.setProgression (p);
    if (pickerOverlay.isVisible())
    {
        const int slot = timeline.getSelectedSlot();
        if (slot >= 0)
            pickerOverlay.setSlot (slot);
        else
            pickerOverlay.setVisible (false);
    }
    updateContextBar (timeline.getSelectedSlot());
}

void ChordSlotsEditor::showGrooveView (bool show)
{
    grooveView = show;
    harmonyViewButton.setToggleState (! show, juce::dontSendNotification);
    grooveViewButton.setToggleState (show, juce::dontSendNotification);

    for (juce::Component* control : std::initializer_list<juce::Component*> {
             &keyBox, &keyLabel, &scaleBox, &scaleLabel, &barsBox, &barsLabel,
             &meterBox, &meterLabel, &slotsBox, &slotsLabel, &snapBox, &snapLabel,
             &fitLabel, &fitCaption, &midiViewBtn, &lanes,
             &moodBox, &moodLabel, &styleBox, &styleLabel, &voicingBox, &voicingLabel,
             &shapeBox, &shapeLabel, &resetCurvesBtn, &analyzeMidiBtn, &melodyFit,
             &melodyFitLabel, &lockCaption, &lockRhythm, &lockRoots, &lockColors,
             &lockVoicing, &adventure, &adventureLabel, &variety, &varietyLabel,
             &syncopation, &syncopationLabel, &bassMotion, &bassMotionLabel, &rollButton })
        control->setVisible (! show);

    for (juce::Component* control : std::initializer_list<juce::Component*> {
             &compBox, &compLabel, &bassPatternBox, &bassPatternLabel, &refeelButton,
             &strum, &strumLabel, &accent, &accentLabel, &humanize, &humanizeLabel,
             &gate, &gateLabel })
        control->setVisible (show);

    pickerOverlay.setVisible (false);
    timeline.setMidiVisible (show || midiViewBtn.getToggleState());
    updateContextBar (timeline.getSelectedSlot());
    repaint();
}

void ChordSlotsEditor::updateContextBar (int slot)
{
    const auto p = processor.getProgression();
    if (slot >= 0 && slot < p.numSlots)
    {
        const cs::Slot& s = p.slots[(size_t) slot];
        juce::String text = "Slot " + juce::String (slot + 1) + ": " + utf8 (cs::chordName (s.chord, p.tonic, p.scale))
                            + " (" + utf8 (cs::romanNumeral (s.chord, p.tonic)) + ")   Safe notes: ";
        const auto safe = cs::safeMelodyNotes (s.chord, p.tonic, p.scale);
        for (size_t i = 0; i < safe.size(); ++i)
        {
            if (i > 0) text << " ";
            text << utf8 (cs::pitchClassName (safe[i], p.tonic, p.scale));
        }
        hint.setText (text, juce::dontSendNotification);
        hint.setJustificationType (juce::Justification::centredLeft);
        rollRootBtn.setVisible (! grooveView);
        rollColorBtn.setVisible (! grooveView);
        rollBassBtn.setVisible (! grooveView);
        inspectBtn.setVisible (! grooveView);
    }
    else
    {
        hint.setText ("Separator strip: click to add, drag to move, right-click to lock or remove. Shift-click a chord to listen.",
                      juce::dontSendNotification);
        hint.setJustificationType (juce::Justification::centred);
        rollRootBtn.setVisible (false);
        rollColorBtn.setVisible (false);
        rollBassBtn.setVisible (false);
        inspectBtn.setVisible (false);
    }
    resized();
}

void ChordSlotsEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void ChordSlotsEditor::updateDragTileVisibility()
{
    const bool song = processor.isSongEnabled();
    const bool split = processor.isSplitMidi();
    dragTile.setVisible (! song && ! split);
    dragChordsTile.setVisible (! song && split);
    dragBassTile.setVisible (! song && split);
    dragSongTile.setVisible (song);
}

void ChordSlotsEditor::timerCallback()
{
    updateTooltips();

    if (processor.isFollowingPlayback())
    {
        const int playingVar = processor.getPlayingVariation();
        if (processor.getEditVariation() != playingVar)
        {
            processor.setEditVariation (playingVar);
            refresh();
        }
    }

    if (processor.isFollowingHostMeter())
    {
        const int hNum = processor.getHostTimeSigNum();
        const int hDen = processor.getHostTimeSigDen();
        const auto p = processor.getProgression();
        if (hNum > 0 && hDen > 0 && (hNum != p.timeSigNum || hDen != p.timeSigDen))
        {
            processor.setMeter (hNum, hDen);
            refresh();
        }
    }

    const int edVar = processor.getEditVariation();
    for (int i = 0; i < cs::kNumVariations; ++i)
        varButtons[i].setToggleState (i == edVar, juce::dontSendNotification);
    followButton.setToggleState (processor.isFollowingPlayback(), juce::dontSendNotification);

    const double pos = processor.getPlayPosition();
    timeline.setPlayhead (pos);
    const juce::String text = processor.isPreviewPlaying() ? "Stop" : "Play";
    if (playButton.getButtonText() != text)
        playButton.setButtonText (text);
    playButton.setToggleState (processor.isPreviewPlaying(), juce::dontSendNotification);
    if (processor.getTriggeredSlot() >= 0)
        timeline.repaint();

    updateDragTileVisibility();
}

void ChordSlotsEditor::saveMidi()
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile ("ChordSlots - " + processor.describeProgression() + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI progression...", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;
        file = file.withFileExtension ("mid");
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk())
        {
            const auto midi = processor.createMidiFile();
            midi.writeTo (out, midi.getNumTracks() == 1 ? 0 : 1);
        }
    });
}

void ChordSlotsEditor::applyShapeStamp (int stampId)
{
    processor.edit ([stampId] (cs::Progression& pr, cs::Rng& rng)
    {
        const int n = pr.numSlots;
        if (n <= 0) return;

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) std::max (1, n - 1);
            float b = 0.5f, tens = 0.5f, e = 0.5f;

            switch (stampId)
            {
                case 1: // Rise
                    b = 0.25f + 0.65f * t;
                    tens = 0.20f + 0.70f * (t * t);
                    e = 0.30f + 0.60f * t;
                    break;
                case 2: // Drop
                    b = 0.85f - 0.60f * t;
                    tens = 0.80f - 0.60f * (t * t);
                    e = 0.85f - 0.65f * t;
                    break;
                case 3: // Arch
                {
                    const float arc = 1.0f - 4.0f * (t - 0.5f) * (t - 0.5f);
                    b = 0.30f + 0.55f * arc;
                    tens = 0.25f + 0.65f * arc;
                    e = 0.35f + 0.50f * arc;
                    break;
                }
                case 4: // Pop Build
                {
                    const float step = std::floor (t * 4.0f) / 3.0f;
                    b = 0.35f + 0.50f * step;
                    tens = 0.30f + 0.60f * step;
                    e = 0.40f + 0.55f * step;
                    break;
                }
                case 5: // Valley
                {
                    const float val = 4.0f * (t - 0.5f) * (t - 0.5f);
                    b = 0.30f + 0.55f * val;
                    tens = 0.25f + 0.60f * val;
                    e = 0.35f + 0.50f * val;
                    break;
                }
                default:
                    break;
            }
            pr.slots[(size_t) i].targetB = juce::jlimit (0.05f, 0.95f, b);
            pr.slots[(size_t) i].targetT = juce::jlimit (0.05f, 0.95f, tens);
            pr.slots[(size_t) i].targetE = juce::jlimit (0.05f, 0.95f, e);
        }
        cs::roll (pr, rng);
    });
}

void ChordSlotsEditor::showMidiImportMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Analyze MIDI");
    menu.addItem (2, "Harmonize Melody");
    menu.addItem (3, "Clear Melody", ! processor.getProgression().melody.empty());
    auto editor = juce::Component::SafePointer<ChordSlotsEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&analyzeMidiBtn), [editor] (int command)
    {
        if (editor == nullptr)
            return;
        if (command == 1 || command == 2)
            editor->importMidi (command == 2);
        else if (command == 3)
            editor->processor.edit ([] (cs::Progression& p, cs::Rng&) { p.melody.clear(); });
    });
}

void ChordSlotsEditor::importMidi (bool harmonize)
{
    chooser = std::make_unique<juce::FileChooser> (harmonize ? "Select a MIDI melody..." : "Select MIDI file to extract curves...",
                                                    juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                    "*.mid;*.midi");
    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    auto editor = juce::Component::SafePointer<ChordSlotsEditor> (this);
    chooser->launchAsync (browserFlags, [editor, harmonize] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (editor == nullptr || ! file.existsAsFile())
            return;

        auto& proc = editor->processor;
        if (harmonize)
        {
            if (! proc.harmonizeMidi (file))
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Harmonize Melody",
                    "Choose a MIDI track with pitched notes and durations (maximum 4096 notes).");
        }
        else if (proc.loadMidiToCurves (file))
            proc.edit ([] (cs::Progression& p, cs::Rng& rng) { cs::roll (p, rng); });
    });
}

bool ChordSlotsEditor::keyPressed (const juce::KeyPress& key)
{
    // Cmd/Ctrl + Z: Undo
    if (key.getKeyCode() == 'Z' && key.getModifiers().isCommandDown())
    {
        processor.undo();
        return true;
    }

    // 1..4: Switch Variation
    if (key.getKeyCode() >= '1' && key.getKeyCode() <= '4' && ! key.getModifiers().isAnyModifierKeyDown())
    {
        const int v = key.getKeyCode() - '1';
        processor.setPlayingVariation (v);
        processor.setEditVariation (v);
        return true;
    }

    // L: Toggle pin on selected slot
    if ((key.getKeyCode() == 'L' || key.getKeyCode() == 'l') && ! key.getModifiers().isAnyModifierKeyDown())
    {
        const int sel = timeline.getSelectedSlot();
        if (sel >= 0)
        {
            processor.edit ([sel] (cs::Progression& pr, cs::Rng&)
            {
                if (sel < pr.numSlots)
                    pr.slots[(size_t) sel].locked = ! pr.slots[(size_t) sel].locked;
            });
            return true;
        }
    }

    // R: Targeted roll on selected slot or full roll
    if (! grooveView && (key.getKeyCode() == 'R' || key.getKeyCode() == 'r')
        && ! key.getModifiers().isAnyModifierKeyDown())
    {
        const int sel = timeline.getSelectedSlot();
        if (sel >= 0)
        {
            processor.edit ([sel] (cs::Progression& pr, cs::Rng& rng)
            {
                cs::rerollSlots (pr, 1u << sel, rng);
            });
        }
        else
        {
            rollButton.triggerClick();
        }
        return true;
    }

    // Space: Toggle playback
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        playButton.triggerClick();
        return true;
    }

    // Escape: close overlay
    if (key.getKeyCode() == juce::KeyPress::escapeKey && pickerOverlay.isVisible())
    {
        pickerOverlay.setVisible (false);
        return true;
    }

    return false;
}

void ChordSlotsEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title, const juce::Colour& titleColour)
    {
        if (r.isEmpty()) return;
        auto rf = r.toFloat().reduced (0.5f);
        g.setColour (ui::panel);
        g.fillRoundedRectangle (rf, 6.0f);
        g.setColour (ui::outline);
        g.drawRoundedRectangle (rf, 6.0f, 1.0f);

        g.setColour (titleColour);
        g.setFont (ui::font (10.0f, true));
        g.drawText (title, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 14, juce::Justification::centredLeft);
    };

    drawPanel (targetCurvesBounds, "TARGET CURVES", ui::accent);
    drawPanel (harmonyPanelBounds, "GENERATION", ui::accent);
    drawPanel (groovePanelBounds, "PERFORMANCE", ui::energy);
    drawPanel (previewPanelBounds, "SYNTH PREVIEW", ui::textDim);
    drawPanel (outputPanelBounds, "MIDI OUTPUT & EXPORT", ui::textDim);
}

void ChordSlotsEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 10);

    auto tabRow = area.removeFromTop (32);
    harmonyViewButton.setBounds (tabRow.removeFromLeft (88));
    tabRow.removeFromLeft (4);
    grooveViewButton.setBounds (tabRow.removeFromLeft (112));
    tabRow.removeFromLeft (8);
    undoButton.setBounds (tabRow.removeFromLeft (62).reduced (0, 2));

    auto sharedControls = tabRow.reduced (0, 2);
    if (optionsButton.isVisible())
    {
        optionsButton.setBounds (sharedControls.removeFromRight (70));
        sharedControls.removeFromRight (4);
    }
    inputButton.setBounds (sharedControls.removeFromRight (92));
    sharedControls.removeFromRight (8);
    auto variations = sharedControls.removeFromRight (326);
    variationLabel.setBounds (variations.removeFromLeft (70));
    for (auto& button : varButtons)
    {
        button.setBounds (variations.removeFromLeft (28));
        variations.removeFromLeft (3);
    }
    variations.removeFromLeft (4);
    followButton.setBounds (variations.removeFromLeft (60));
    variations.removeFromLeft (4);
    copyVarButton.setBounds (variations);

    area.removeFromTop (8);
    if (! grooveView)
    {
        midiViewBtn.setBounds (sharedControls.removeFromRight (94).withTrimmedRight (4));
        auto loopRow = area.removeFromTop (43);
        auto place = [&loopRow] (juce::ComboBox& box, juce::Label& label, int width)
        {
            auto cell = loopRow.removeFromLeft (width);
            loopRow.removeFromLeft (8);
            label.setBounds (cell.removeFromTop (15));
            box.setBounds (cell);
        };

        place (keyBox, keyLabel, 50);
        place (scaleBox, scaleLabel, 96);
        place (barsBox, barsLabel, 60);
        place (meterBox, meterLabel, 60);
        place (slotsBox, slotsLabel, 54);
        place (snapBox, snapLabel, 66);

        loopRow.removeFromLeft (10);
        place (moodBox, moodLabel, 100);
        place (styleBox, styleLabel, 132);
        place (voicingBox, voicingLabel, 106);

        auto fitArea = loopRow.removeFromRight (60);
        fitCaption.setBounds (fitArea.removeFromTop (14));
        fitLabel.setBounds (fitArea);
        area.removeFromTop (6);
    }

    // =========================================================================
    // Footer & Context Bar
    // =========================================================================
    auto ctxArea = area.removeFromBottom (26);
    if (rollRootBtn.isVisible())
    {
        auto btnArea = ctxArea.removeFromRight (320);
        const int bw = (btnArea.getWidth() - 12) / 4;
        rollRootBtn.setBounds (btnArea.removeFromLeft (bw));
        btnArea.removeFromLeft (4);
        rollColorBtn.setBounds (btnArea.removeFromLeft (bw));
        btnArea.removeFromLeft (4);
        rollBassBtn.setBounds (btnArea.removeFromLeft (bw));
        btnArea.removeFromLeft (4);
        inspectBtn.setBounds (btnArea);
        ctxArea.removeFromRight (8);
    }
    hint.setBounds (ctxArea);
    area.removeFromBottom (6);

    // Expanded Footer (104px tall)
    auto footer = area.removeFromBottom (104);
    area.removeFromBottom (10);

    const int gap = 8;
    const auto stagePanel = footer.removeFromLeft (juce::roundToInt ((footer.getWidth() - gap) * 0.56f));
    harmonyPanelBounds = grooveView ? juce::Rectangle<int>{} : stagePanel;
    groovePanelBounds = grooveView ? stagePanel : juce::Rectangle<int>{};
    footer.removeFromLeft (gap);
    previewPanelBounds = footer.removeFromLeft (170);
    footer.removeFromLeft (gap);
    outputPanelBounds = footer;

    struct Knob { juce::Slider* s; juce::Label* l; };

    // --- Panel 1: Harmony Generator ---
    if (! grooveView)
    {
        auto hInner = harmonyPanelBounds.reduced (8, 4);
        hInner.removeFromTop (16);

        auto harmActions = hInner.removeFromRight (74);
        rollButton.setBounds (harmActions.withSizeKeepingCentre (74, 36));
        hInner.removeFromRight (6);

        auto lockButtons = hInner.removeFromTop (20);
        lockCaption.setBounds (lockButtons.removeFromLeft (50));
        const int lw = (lockButtons.getWidth() - 9) / 4;
        for (auto* b : { &lockRhythm, &lockRoots, &lockColors, &lockVoicing })
        {
            b->setBounds (lockButtons.removeFromLeft (lw));
            lockButtons.removeFromLeft (3);
        }
        hInner.removeFromTop (3);

        auto harmKnobs = hInner;
        const int hkw = harmKnobs.getWidth() / 4;
        for (const Knob& k : { Knob { &adventure, &adventureLabel }, Knob { &variety, &varietyLabel },
                               Knob { &syncopation, &syncopationLabel },
                               Knob { &bassMotion, &bassMotionLabel } })
        {
            auto cell = harmKnobs.removeFromLeft (hkw);
            k.l->setBounds (cell.removeFromTop (13));
            k.s->setBounds (cell.reduced (1, 0));
        }
    }

    // --- Panel 2: Groove & Feel ---
    targetCurvesBounds = {};
    if (grooveView)
    {
        auto gInner = groovePanelBounds.reduced (8, 4);
        gInner.removeFromTop (16);

        auto controlRow = gInner.removeFromTop (31);
        auto compCell = controlRow.removeFromLeft (128);
        compLabel.setBounds (compCell.removeFromTop (12));
        compBox.setBounds (compCell.removeFromTop (19));
        controlRow.removeFromLeft (8);
        auto bassCell = controlRow.removeFromLeft (136);
        bassPatternLabel.setBounds (bassCell.removeFromTop (12));
        bassPatternBox.setBounds (bassCell.removeFromTop (19));
        auto refeelArea = controlRow.removeFromRight (62);
        refeelButton.setBounds (refeelArea.withSizeKeepingCentre (60, 28));

        gInner.removeFromTop (2);
        auto grooveKnobs = gInner;
        const int gkw = grooveKnobs.getWidth() / 4;
        for (const Knob& k : { Knob { &strum, &strumLabel }, Knob { &accent, &accentLabel },
                               Knob { &humanize, &humanizeLabel }, Knob { &gate, &gateLabel } })
        {
            auto cell = grooveKnobs.removeFromLeft (gkw);
            k.l->setBounds (cell.removeFromTop (13));
            k.s->setBounds (cell.reduced (1, 0));
        }
    }

    // Synth preview
    {
        auto preview = previewPanelBounds.reduced (8, 4).withTrimmedTop (16);
        auto sound = preview.removeFromLeft (84);
        synthModelLabel.setBounds (sound.removeFromTop (13));
        synthModelBox.setBounds (sound.removeFromTop (26));
        sound.removeFromTop (5);
        playButton.setBounds (sound.removeFromTop (28));

        preview.removeFromLeft (6);
        volumeLabel.setBounds (preview.removeFromTop (13));
        volume.setBounds (preview.reduced (1, 0));
    }

    // MIDI output and export
    {
        auto oInner = outputPanelBounds.reduced (8, 4);
        oInner.removeFromTop (16);

        auto dragArea = oInner.removeFromRight (92).reduced (0, 2);
        dragTile.setBounds (dragArea);
        auto splitDragTop = dragArea.removeFromTop (dragArea.getHeight() / 2).withTrimmedBottom (1);
        dragChordsTile.setBounds (splitDragTop);
        dragBassTile.setBounds (dragArea.withTrimmedTop (1));
        dragSongTile.setBounds (dragArea);

        updateDragTileVisibility();

        auto buttons = oInner.withTrimmedRight (6).reduced (0, 2);
        const int buttonHeight = (buttons.getHeight() - 4) / 3;
        for (auto* button : { &saveButton, &splitMidiButton, &songButton })
        {
            button->setBounds (buttons.removeFromTop (buttonHeight));
            buttons.removeFromTop (2);
        }
    }

    // =========================================================================
    // Main Area: Timeline Cards & Lanes & Lane Toolbar
    // =========================================================================
    pickerOverlay.setBounds (juce::Rectangle<int> (juce::jmin (680, getWidth() - 40), 350)
                                 .withCentre ({ getWidth() / 2, getHeight() / 2 }));

    if (grooveView)
    {
        timeline.setBounds (area);
        return;
    }

    const int timelineH = midiViewBtn.getToggleState() ? 188 : 152;
    timeline.setBounds (area.removeFromTop (timelineH));

    area.removeFromTop (6);

    targetCurvesBounds = area;
    area.removeFromTop (20);
    area.removeFromBottom (4);
    auto curveTools = area.removeFromBottom (31).reduced (8, 0);
    area.removeFromBottom (4);
    lanes.setBounds (area);

    shapeLabel.setBounds (curveTools.removeFromLeft (52));
    shapeBox.setBounds (curveTools.removeFromLeft (133).withSizeKeepingCentre (133, 24));
    curveTools.removeFromLeft (6);
    resetCurvesBtn.setBounds (curveTools.removeFromLeft (72).withSizeKeepingCentre (72, 24));

    auto melodyControl = curveTools.removeFromRight (112);
    melodyFit.setBounds (melodyControl.removeFromRight (34).withSizeKeepingCentre (32, 28));
    melodyFitLabel.setBounds (melodyControl);
    curveTools.removeFromRight (8);
    analyzeMidiBtn.setBounds (curveTools.removeFromRight (120).withSizeKeepingCentre (120, 24));
}
