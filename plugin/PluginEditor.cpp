// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
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
      gateAttachment (p.apvts, "gate", gate),
      volumeAttachment (p.apvts, "volume", volume),
      strumAttachment (p.apvts, "strum", strum),
      accentAttachment (p.apvts, "accent", accent),
      humanizeAttachment (p.apvts, "humanize", humanize),
      splitMidiAttachment (p.apvts, "splitMidi", splitMidiButton)
{
    setLookAndFeel (&lnf);

    // Header Combos
    setupCombo (keyBox, keyLabel, "KEY");
    setupCombo (scaleBox, scaleLabel, "SCALE");
    setupCombo (barsBox, barsLabel, "BARS");
    setupCombo (meterBox, meterLabel, "METER");
    setupCombo (slotsBox, slotsLabel, "SLOTS");
    setupCombo (snapBox, snapLabel, "GRID");

    for (int pc = 0; pc < 12; ++pc)
        keyBox.addItem (utf8 (cs::pitchClassName (pc, pc, 0)), pc + 1);

    for (int i = 0; i < cs::numScales(); ++i)
        scaleBox.addItem (cs::scales()[(size_t) i].name, i + 1);

    for (int b : { 1, 2, 4, 8 })
        barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);

    meterBox.addItem ("4/4", 1);
    meterBox.addItem ("3/4", 2);
    meterBox.addItem ("6/8", 3);
    meterBox.addItem ("Host", 4);

    for (int n : { 2, 3, 4, 5, 6, 7, 8, 12, 16 })
        slotsBox.addItem (juce::String (n), n);

    snapBox.addItem ("1/4", 24);
    snapBox.addItem ("1/4 T", 16);
    snapBox.addItem ("1/8", 12);
    snapBox.addItem ("1/8 T", 8);
    snapBox.addItem ("1/16", 6);
    snapBox.addItem ("1/16 T", 4);
    snapBox.addItem ("1/32", 3);

    const auto& m = cs::moods();
    for (int i = 0; i < (int) m.size(); ++i)
        moodBox.addItem (m[(size_t) i].name, i + 1);

    // Input / Trigger popover button
    inputButton.setTooltip ("Configure incoming MIDI trigger mode and note mapping");
    inputButton.onClick = [this] { showInputMenu(); };
    addAndMakeVisible (inputButton);

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
            cs::setNumSlots (pr, n);
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
    fitCaption.setText ("FIT", juce::dontSendNotification);
    fitCaption.setFont (ui::font (10.0f, true));
    fitCaption.setColour (juce::Label::textColourId, ui::textDim);
    fitCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fitCaption);

    fitLabel.setFont (ui::font (24.0f, true));
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

    // Lane Toolbar controls
    laneToolbarLabel.setText ("TARGET CURVES", juce::dontSendNotification);
    laneToolbarLabel.setFont (ui::font (10.0f, true));
    laneToolbarLabel.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (laneToolbarLabel);

    setupCombo (moodBox, moodLabel, "MOOD PRESET");
    for (int i = 0; i < (int) m.size(); ++i)
        moodBox.addItem (m[(size_t) i].name, i + 1);

    setupCombo (styleBox, styleLabel, "STYLE PRIOR");
    styleBox.addItem ("Auto (from scale)", 1);
    styleBox.addItem ("Pop/Rock", 2);
    styleBox.addItem ("Minor Loops", 3);
    styleBox.addItem ("Soul / Jazz", 4);
    styleBox.addItem ("Free", 5);
    styleBox.setSelectedId (1, juce::dontSendNotification);

    setupCombo (voicingBox, voicingLabel, "VOICING");
    for (int i = 0; i < cs::kNumVoicingStyles; ++i)
        voicingBox.addItem (cs::voicingStyleName ((cs::VoicingStyle) i), i + 1);
    voicingBox.setSelectedId ((int) cs::VoicingStyle::Piano + 1, juce::dontSendNotification);

    setupCombo (shapeBox, shapeLabel, "SHAPES");
    shapeBox.addItem (juce::String::fromUTF8 ("Shape Stamps ▾"), 1);
    shapeBox.addItem (juce::String::fromUTF8 ("Rise (0.2 → 0.9)"), 2);
    shapeBox.addItem (juce::String::fromUTF8 ("Drop (0.9 → 0.2)"), 3);
    shapeBox.addItem ("Arch (Peak center)", 4);
    shapeBox.addItem ("Pop Build (Step up)", 5);
    shapeBox.addItem ("Valley (Dip center)", 6);
    shapeBox.setSelectedId (1, juce::dontSendNotification);
    shapeBox.onChange = [this]
    {
        const int id = shapeBox.getSelectedId();
        if (id > 1)
        {
            applyShapeStamp (id - 1);
            shapeBox.setSelectedId (1, juce::dontSendNotification);
        }
    };

    resetCurvesBtn.setTooltip ("Reset all target curves to neutral flat (0.50)");
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

    analyzeMidiBtn.setButtonText (juce::String::fromUTF8 ("Analyze MIDI ▾"));
    analyzeMidiBtn.setTooltip ("Import a MIDI file and extract Brightness, Tension, and Energy curves");
    analyzeMidiBtn.onClick = [this] { analyzeMidi(); };
    addAndMakeVisible (analyzeMidiBtn);

    addAndMakeVisible (lanes);

    // Overlay
    pickerOverlay.setVisible (false);
    pickerOverlay.onClose = [this] { pickerOverlay.setVisible (false); };
    addChildComponent (pickerOverlay);

    // Footer Panel 1: Locks
    lockCaption.setText ("LOCKS (Survive Roll)", juce::dontSendNotification);
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
    setupKnob (adventure, adventureLabel, "ADVENTURE");
    setupKnob (syncopation, syncopationLabel, "SYNCOPATION");
    setupKnob (bassMotion, bassMotionLabel, "BASS MOTION");
    setupKnob (strum, strumLabel, "STRUM");
    setupKnob (accent, accentLabel, "ACCENT");
    setupKnob (humanize, humanizeLabel, "HUMANIZE");
    setupKnob (gate, gateLabel, "GATE");
    setupKnob (volume, volumeLabel, "SYNTH VOL");

    adventure.setRange (0.0, 1.0, 0.01);
    syncopation.setRange (0.0, 1.0, 0.01);
    bassMotion.setRange (0.0, 1.0, 0.01);
    strum.setRange (-1.0, 1.0, 0.01);
    accent.setRange (0.0, 1.0, 0.01);
    humanize.setRange (0.0, 1.0, 0.01);

    adventure.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    syncopation.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    bassMotion.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    strum.textFromValueFunction = [] (double v)
    {
        const int ms = juce::roundToInt (v * 80.0);
        return (ms > 0 ? "+" : "") + juce::String (ms) + " ms";
    };
    accent.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    humanize.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 20.0)) + " ms"; };
    gate.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    volume.textFromValueFunction = [] (double v)
    {
        return juce::String (juce::Decibels::gainToDecibels ((float) v, -48.0f), 1) + " dB";
    };

    adventure.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    syncopation.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    bassMotion.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    strum.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 80.0; };
    accent.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    humanize.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 20.0; };
    gate.valueFromTextFunction = [] (const juce::String& s) { return s.getDoubleValue() / 100.0; };
    volume.valueFromTextFunction = [] (const juce::String& s)
    {
        return (double) juce::Decibels::decibelsToGain ((float) s.getDoubleValue(), -48.0f);
    };

    adventure.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
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
    setupCombo (compBox, compLabel, "COMPING");
    compBox.addItem ("Sustain", 1);
    compBox.addItem ("1/4 Pulse", 2);
    compBox.addItem ("1/8 Pulse", 3);
    compBox.addItem ("Offbeat Reggae", 4);
    compBox.addItem ("Ballad Stabs", 5);
    compBox.addItem ("Trap Stabs", 6);
    compBox.addItem ("Triplet Swing", 7);
    compAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "compStyle", compBox);

    // Synth sound model combo
    setupCombo (synthModelBox, synthModelLabel, "SOUND");
    synthModelBox.addItem ("Clean Saw", 1);
    synthModelBox.addItem ("Rhodes EP", 2);
    synthModelBox.addItem ("Felt Piano", 3);
    synthModelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "synthModel", synthModelBox);

    // Buttons
    refeelButton.setTooltip ("Re-humanize groove with a fresh timing and velocity seed");
    refeelButton.onClick = [this] { processor.refeel(); };
    addAndMakeVisible (refeelButton);

    undoButton.onClick = [this] { processor.undo(); };
    addAndMakeVisible (undoButton);

    rollButton.getProperties().set ("primary", true);
    rollButton.setTooltip ("Generate new chords for all unlocked slots");
    rollButton.onClick = [this]
    {
        processor.edit ([] (cs::Progression& pr, cs::Rng& rng) { cs::roll (pr, rng); });
    };
    addAndMakeVisible (rollButton);

    playButton.setTooltip ("Toggle built-in preview synth playback");
    playButton.onClick = [this] { processor.setPreviewPlaying (! processor.isPreviewPlaying()); };
    addAndMakeVisible (playButton);

    saveButton.setTooltip ("Export progression as standard MIDI file");
    saveButton.onClick = [this] { saveMidi(); };
    addAndMakeVisible (saveButton);

    splitMidiButton.setClickingTogglesState (true);
    splitMidiButton.setTooltip ("Route upper chords to MIDI Ch 1 and monophonic bass to MIDI Ch 2");
    addAndMakeVisible (splitMidiButton);

    addAndMakeVisible (dragTile);
    addAndMakeVisible (dragChordsTile);
    addAndMakeVisible (dragBassTile);

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
            pickerOverlay.setSlot (s);
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
    setResizeLimits (900, 540, 1920, 1200);
    setSize (1040, 650);
    startTimerHz (30);
}

ChordSlotsEditor::~ChordSlotsEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ChordSlotsEditor::setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& caption)
{
    label.setText (caption, juce::dontSendNotification);
    label.setFont (ui::font (10.0f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void ChordSlotsEditor::setupKnob (juce::Slider& s, juce::Label& label, const juce::String& caption)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (s);

    label.setText (caption, juce::dontSendNotification);
    label.setFont (ui::font (9.5f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
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

    lockRhythm.setToggleState (p.locks.rhythm, juce::dontSendNotification);
    lockRoots.setToggleState (p.locks.roots, juce::dontSendNotification);
    lockColors.setToggleState (p.locks.colors, juce::dontSendNotification);
    lockVoicing.setToggleState (p.locks.voicing, juce::dontSendNotification);

    adventure.setValue (p.adventure, juce::dontSendNotification);
    syncopation.setValue (p.syncopation, juce::dontSendNotification);
    bassMotion.setValue (p.bassMotion, juce::dontSendNotification);

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
    updateContextBar (timeline.getSelectedSlot());
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
        rollRootBtn.setVisible (true);
        rollColorBtn.setVisible (true);
        rollBassBtn.setVisible (true);
        inspectBtn.setVisible (true);
    }
    else
    {
        hint.setText (juce::String::fromUTF8 ("Card Fill: Brightness  \xc2\xb7  Red Strip: Tension  \xc2\xb7  "
                                              "Click slot to select/audition  \xc2\xb7  Press 'L' to pin  \xc2\xb7  'R' to roll"),
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

void ChordSlotsEditor::timerCallback()
{
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

    const bool isSplit = processor.isSplitMidi();
    if (dragTile.isVisible() == isSplit)
    {
        dragTile.setVisible (! isSplit);
        dragChordsTile.setVisible (isSplit);
        dragBassTile.setVisible (isSplit);
    }
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
            processor.createMidiFile().writeTo (out);
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

void ChordSlotsEditor::analyzeMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Select MIDI file to extract curves...",
                                                    juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                    "*.mid;*.midi");
    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (browserFlags, [this] (const juce::FileChooser& fc)
    {
        const auto result = fc.getResult();
        if (result.existsAsFile())
        {
            if (processor.loadMidiToCurves (result))
            {
                processor.edit ([] (cs::Progression& pr, cs::Rng& rng) { cs::roll (pr, rng); });
            }
        }
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
    if ((key.getKeyCode() == 'R' || key.getKeyCode() == 'r') && ! key.getModifiers().isAnyModifierKeyDown())
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
    g.setColour (ui::text);
    g.setFont (ui::font (20.0f, true));
    g.drawText ("ChordSlots", 16, 14, 130, 26, juce::Justification::centredLeft);
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f));
    g.drawText ("target-driven harmony", 17, 38, 130, 14, juce::Justification::centredLeft);

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

    drawPanel (harmonyPanelBounds, "1. HARMONY GENERATOR", ui::accent);
    drawPanel (groovePanelBounds, "2. GROOVE & FEEL", ui::energy);
    drawPanel (outputPanelBounds, "OUTPUT & EXPORT", ui::textDim);
}

void ChordSlotsEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 10);

    // =========================================================================
    // Header Row (~54px)
    // =========================================================================
    auto header = area.removeFromTop (54);
    header.removeFromLeft (136); // logo area

    // Right side: Fit score badge
    auto fitArea = header.removeFromRight (64);
    fitCaption.setBounds (fitArea.removeFromTop (14));
    fitLabel.setBounds (fitArea);

    header.removeFromRight (10);

    // Variation bank area
    auto varArea = header.removeFromRight (186);
    auto varTop = varArea.removeFromTop (16);
    variationLabel.setBounds (varTop.removeFromLeft (74));
    followButton.setBounds (varTop);

    varArea.removeFromTop (2);
    auto varBottom = varArea.removeFromTop (28);
    const int btnW = 28;
    for (int i = 0; i < cs::kNumVariations; ++i)
    {
        varButtons[i].setBounds (varBottom.removeFromLeft (btnW));
        varBottom.removeFromLeft (3);
    }
    varBottom.removeFromLeft (4);
    copyVarButton.setBounds (varBottom);

    header.removeFromRight (12);

    // Left combos: KEY, SCALE, BARS, METER, SLOTS, GRID, INPUT
    auto place = [&header] (juce::ComboBox& box, juce::Label& label, int width)
    {
        auto cell = header.removeFromLeft (width);
        header.removeFromLeft (8);
        label.setBounds (cell.removeFromTop (15));
        box.setBounds (cell.removeFromTop (28));
    };

    place (keyBox, keyLabel, 56);
    place (scaleBox, scaleLabel, 110);
    place (barsBox, barsLabel, 66);
    place (meterBox, meterLabel, 66);
    place (slotsBox, slotsLabel, 60);
    place (snapBox, snapLabel, 76);

    // Input/Trigger button in remaining header width
    if (header.getWidth() > 60)
    {
        auto inCell = header.removeFromLeft (juce::jmin (92, header.getWidth()));
        inCell.removeFromTop (15);
        inputButton.setBounds (inCell.removeFromTop (28));
    }

    area.removeFromTop (10);

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

    const int totalWidth = footer.getWidth();
    const int gap = 8;
    const int harmWidth = juce::roundToInt ((totalWidth - 2 * gap) * 0.38f);
    const int grooWidth = juce::roundToInt ((totalWidth - 2 * gap) * 0.31f);

    harmonyPanelBounds = footer.removeFromLeft (harmWidth);
    footer.removeFromLeft (gap);
    groovePanelBounds = footer.removeFromLeft (grooWidth);
    footer.removeFromLeft (gap);
    outputPanelBounds = footer;

    struct Knob { juce::Slider* s; juce::Label* l; };

    // --- Panel 1: Harmony Generator ---
    {
        auto hInner = harmonyPanelBounds.reduced (8, 4);
        hInner.removeFromTop (16);

        auto locks = hInner.removeFromLeft (168);
        lockCaption.setBounds (locks.removeFromTop (14));
        locks.removeFromTop (4);
        auto lockButtons = locks.removeFromTop (26);
        const int lw = (lockButtons.getWidth() - 9) / 4;
        for (auto* b : { &lockRhythm, &lockRoots, &lockColors, &lockVoicing })
        {
            b->setBounds (lockButtons.removeFromLeft (lw));
            lockButtons.removeFromLeft (3);
        }
        hInner.removeFromLeft (8);

        auto harmActions = hInner.removeFromRight (74);
        undoButton.setBounds (harmActions.removeFromTop (26));
        harmActions.removeFromTop (4);
        rollButton.setBounds (harmActions.removeFromTop (36));
        hInner.removeFromRight (6);

        auto harmKnobs = hInner;
        const int hkw = harmKnobs.getWidth() / 3;
        for (const Knob& k : { Knob { &adventure, &adventureLabel }, Knob { &syncopation, &syncopationLabel },
                               Knob { &bassMotion, &bassMotionLabel } })
        {
            auto cell = harmKnobs.removeFromLeft (hkw);
            k.l->setBounds (cell.removeFromTop (13));
            k.s->setBounds (cell.reduced (1, 0));
        }
    }

    // --- Panel 2: Groove & Feel ---
    {
        auto gInner = groovePanelBounds.reduced (8, 4);
        gInner.removeFromTop (16);

        auto compCell = gInner.removeFromLeft (86);
        compLabel.setBounds (compCell.removeFromTop (13));
        compBox.setBounds (compCell.removeFromTop (28));
        gInner.removeFromLeft (6);

        auto refeelArea = gInner.removeFromRight (64);
        refeelButton.setBounds (refeelArea.removeFromTop (32).withSizeKeepingCentre (60, 30));
        gInner.removeFromRight (6);

        auto grooveKnobs = gInner;
        const int gkw = grooveKnobs.getWidth() / 3;
        for (const Knob& k : { Knob { &strum, &strumLabel }, Knob { &accent, &accentLabel }, Knob { &humanize, &humanizeLabel } })
        {
            auto cell = grooveKnobs.removeFromLeft (gkw);
            k.l->setBounds (cell.removeFromTop (13));
            k.s->setBounds (cell.reduced (1, 0));
        }
    }

    // --- Panel 3: Output & Export ---
    {
        auto oInner = outputPanelBounds.reduced (8, 4);
        oInner.removeFromTop (16);

        auto synthCell = oInner.removeFromLeft (86);
        synthModelLabel.setBounds (synthCell.removeFromTop (13));
        synthModelBox.setBounds (synthCell.removeFromTop (28));
        oInner.removeFromLeft (6);

        auto dragArea = oInner.removeFromRight (92).reduced (0, 2);
        dragTile.setBounds (dragArea);
        auto splitDragTop = dragArea.removeFromTop (dragArea.getHeight() / 2).withTrimmedBottom (1);
        dragChordsTile.setBounds (splitDragTop);
        dragBassTile.setBounds (dragArea.withTrimmedTop (1));

        const bool isSplit = processor.isSplitMidi();
        dragTile.setVisible (! isSplit);
        dragChordsTile.setVisible (isSplit);
        dragBassTile.setVisible (isSplit);

        oInner.removeFromRight (8);

        auto btnArea = oInner.removeFromRight (88).reduced (0, 2);
        auto topBtns = btnArea.removeFromTop (btnArea.getHeight() / 2).withTrimmedBottom (1);
        playButton.setBounds (topBtns.removeFromLeft (topBtns.getWidth() / 2).withTrimmedRight (1));
        saveButton.setBounds (topBtns.withTrimmedLeft (1));
        splitMidiButton.setBounds (btnArea.withTrimmedTop (1));

        oInner.removeFromRight (6);

        auto outKnobs = oInner;
        const int okw = outKnobs.getWidth() / 2;
        for (const Knob& k : { Knob { &gate, &gateLabel }, Knob { &volume, &volumeLabel } })
        {
            auto cell = outKnobs.removeFromLeft (okw);
            k.l->setBounds (cell.removeFromTop (13));
            k.s->setBounds (cell.reduced (1, 0));
        }
    }

    // =========================================================================
    // Main Area: Timeline Cards & Lanes & Lane Toolbar
    // =========================================================================
    const int timelineH = 135;
    timeline.setBounds (area.removeFromTop (timelineH));

    area.removeFromTop (6);

    // Lane Toolbar (~26px)
    auto laneBar = area.removeFromTop (26);
    laneToolbarLabel.setBounds (laneBar.removeFromLeft (88).withTrimmedTop (4));
    laneBar.removeFromLeft (6);

    auto placeLaneBarCombo = [&laneBar] (juce::ComboBox& box, juce::Label& label, int width)
    {
        auto cell = laneBar.removeFromLeft (width);
        laneBar.removeFromLeft (6);
        label.setBounds (cell.removeFromTop (12));
        box.setBounds (cell.removeFromTop (24));
    };

    placeLaneBarCombo (moodBox, moodLabel, 120);
    placeLaneBarCombo (styleBox, styleLabel, 130);
    placeLaneBarCombo (voicingBox, voicingLabel, 110);
    placeLaneBarCombo (shapeBox, shapeLabel, 120);

    resetCurvesBtn.setBounds (laneBar.removeFromRight (58).withSizeKeepingCentre (56, 24));
    laneBar.removeFromRight (6);
    analyzeMidiBtn.setBounds (laneBar.removeFromRight (112).withSizeKeepingCentre (110, 24));

    area.removeFromTop (4);
    lanes.setBounds (area);

    if (pickerOverlay.isVisible())
    {
        pickerOverlay.setBounds (juce::Rectangle<int> (juce::jmin (680, getWidth() - 40), 320)
                                     .withCentre ({ getWidth() / 2, getHeight() / 2 }));
    }
}
