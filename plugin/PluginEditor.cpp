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

constexpr const char* bassPatternNames[] { "Sustain", "Octave pump", "Offbeat 8ths", "808 legato", "Walking" };

struct PreviewNote
{
    int start, duration;
    float velocity;
    int pitch = -1;
};

struct PatternPreview
{
    int steps;
    std::vector<PreviewNote> notes;
};

PatternPreview compPreview (cs::CompStyle style, int voices)
{
    PatternPreview preview { cs::compPreviewBars (style) * cs::kStepsPerBar, {} };
    for (const auto& hit : cs::getCompHits (style, preview.steps))
        preview.notes.push_back ({ hit.stepOffset, hit.durationSteps, hit.velScale, hit.selectedVoice (0, voices) });
    return preview;
}

PatternPreview bassPreview (const cs::Progression& progression)
{
    const auto play = cs::toPlayData (progression);
    PatternPreview preview { play.lengthSteps, {} };
    for (int i = 0; i < play.numSlots; ++i)
    {
        const auto& slot = play.slots[i];
        if (play.bassStyle == cs::BassStyle::Sustain)
            preview.notes.push_back ({ slot.start, slot.end - slot.start, 1.0f, slot.primaryBass });
        else
            for (int h = 0; h < slot.numBassHits; ++h)
            {
                const auto& hit = slot.bassHits[h];
                const int start = slot.start + hit.stepOffset;
                preview.notes.push_back ({ start, std::min (hit.durationSteps, play.lengthSteps - start),
                                           hit.velocityScale, hit.pitch });
            }
    }
    return preview;
}

class PerformancePreviewButton final : public juce::Button
{
public:
    PerformancePreviewButton (juce::String name, PatternPreview pattern)
        : juce::Button ("Audition " + name), preview (std::move (pattern))
    {
        setTooltip ("Click to audition at the current tempo while playback is stopped.");
        for (const auto& note : preview.notes)
        {
            lowPitch = std::min (lowPitch, note.pitch);
            highPitch = std::max (highPitch, note.pitch);
        }
    }

    void paintButton (juce::Graphics& g, bool hovered, bool pressed) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour (ui::energy.withAlpha (pressed ? 0.24f : hovered ? 0.15f : 0.07f));
        g.fillRoundedRectangle (area, 4.0f);

        g.setColour (ui::energy);
        juce::Path play;
        play.addTriangle (9.0f, area.getCentreY() - 4.5f, 16.0f, area.getCentreY(),
                          9.0f, area.getCentreY() + 4.5f);
        g.fillPath (play);

        const auto grid = area.withTrimmedLeft (25.0f).withTrimmedRight (8.0f).reduced (0.0f, 5.0f);
        const auto xAt = [&] (int step) { return grid.getX() + grid.getWidth() * step / preview.steps; };
        for (int beat = 0; beat <= preview.steps; beat += cs::kStepsPerQuarter)
        {
            g.setColour (ui::textDim.withAlpha (beat % cs::kStepsPerBar == 0 ? 0.45f : 0.22f));
            g.drawVerticalLine (juce::roundToInt (xAt (beat)), grid.getY(), grid.getBottom());
        }
        for (const auto& note : preview.notes)
        {
            const auto height = note.pitch < 0 ? grid.getHeight() * note.velocity : 3.0f;
            const auto y = note.pitch < 0 ? grid.getBottom() - height
                : grid.getY() + (grid.getHeight() - height)
                    * (lowPitch == highPitch ? 0.5f : (float) (highPitch - note.pitch) / (highPitch - lowPitch));
            const auto width = xAt (note.start + note.duration) - xAt (note.start);
            g.setColour (ui::energy.withAlpha (note.velocity));
            g.fillRoundedRectangle (xAt (note.start), y,
                                    std::max (1.0f, width - 1.0f), height, 2.0f);
        }
        if (hasKeyboardFocus (false))
        {
            g.setColour (ui::energy);
            g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
        }
    }

private:
    const PatternPreview preview;
    int lowPitch = 128, highPitch = -1;
};

class PerformanceBrowser final : public juce::Component
{
public:
    PerformanceBrowser (int selected, int numStyles, const std::function<const char* (int)>& nameFor,
                        const std::function<PatternPreview (int)>& patternFor,
                        std::function<void (int)> apply, std::function<void (int)> audition)
    {
        for (int i = 0; i < numStyles; ++i)
        {
            const juce::String label (nameFor (i));
            auto* name = names.add (new juce::TextButton (i == selected
                ? juce::String::charToString (0x2713) + " " + label : label));
            name->setToggleState (i == selected, juce::dontSendNotification);
            name->setTooltip ("Apply this performance to the progression");
            name->onClick = [this, apply, i]
            {
                apply (i);
                if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
                    callout->dismiss();
            };
            addAndMakeVisible (*name);

            auto* preview = previews.add (new PerformancePreviewButton (label, patternFor (i)));
            preview->onClick = [audition, i] { audition (i); };
            addAndMakeVisible (*preview);
        }
        setSize (350, 12 + 28 * numStyles);
    }

    void resized() override
    {
        auto rows = getLocalBounds().reduced (6);
        for (int i = 0; i < names.size(); ++i)
        {
            auto row = rows.removeFromTop (28).reduced (0, 2);
            names[i]->setBounds (row.removeFromLeft (122));
            previews[i]->setBounds (row.withTrimmedLeft (8));
        }
    }

private:
    juce::OwnedArray<juce::TextButton> names;
    juce::OwnedArray<PerformancePreviewButton> previews;
};
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
    setupCombo (keyBox, keyLabel, "TONIC",
              "Choose the tonic for scale highlighting, suggestions and Roll.");
    keyBox.setName ("Tonic");
    scaleBox.setName ("Scale");
    scaleBox.setTooltip ("Choose the scale for highlighting, suggestions and Roll.");
    scaleBox.setTextWhenNothingSelected ("Scale");
    addAndMakeVisible (scaleBox);
    keyActionsButton.setTooltip ("Transpose the progression or go to the relative key.");
    keyActionsButton.onClick = [this] { showKeyActions(); };
    addAndMakeVisible (keyActionsButton);
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
    {
        const char* name = cs::scales()[(size_t) i].name;
        if (i == 0) name = "Major (Ionian)";
        if (i == 1) name = "Natural Minor (Aeolian)";
        scaleBox.addItem (name, i + 1);
    }

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
        if (tonic >= 0)
            processor.edit ([tonic] (cs::Progression& pr, cs::Rng&) { cs::setTonalContext (pr, tonic, pr.scale); });
    };

    scaleBox.onChange = [this]
    {
        if (refreshing) return;
        const int scale = scaleBox.getSelectedId() - 1;
        if (scale >= 0)
            processor.edit ([scale] (cs::Progression& pr, cs::Rng&) { cs::setTonalContext (pr, pr.tonic, scale); });
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
    fitCaption.setTooltip ("How closely the generated slots match their Brightness and Tension targets.");
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
              "Open the menu to see what each preset applies before choosing it.");
    moodBox.setTextWhenNothingSelected ("Custom");
    moodBox.getRootMenu()->addSectionHeader ("All presets replace Brightness, Tension and Energy target curves and generate a progression (also runs a Roll)");
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

    bassPatternLabel.setText ("BASS PATTERN", juce::dontSendNotification);
    bassPatternLabel.setFont (ui::font (10.0f, true));
    bassPatternLabel.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (bassPatternLabel);
    addAndMakeVisible (bassPatternBox);
    bassPatternBox.setTooltip ("Browse and audition bass patterns");
    bassPatternBox.onClick = [this] { showBassPatternBrowser(); };
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
    setupCombo (endingBox, endingLabel, "Ending", "Harmonic ending preference for the next Roll.");
    endingBox.setComponentID ("ending");
    endingBox.addItem ("Free", 1);
    endingBox.addItem ("Resolve to Tonic", 2);
    endingBox.addItem ("Lead Back to Start", 3);
    endingBox.onChange = [this]
    {
        const int id = endingBox.getSelectedId();
        if (! refreshing && id != 0)
            processor.edit ([id] (cs::Progression& p, cs::Rng&) { p.ending = (cs::Ending) (id - 1); }, false);
    };

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

    compLabel.setText ("COMPING", juce::dontSendNotification);
    compLabel.setFont (ui::font (10.0f, true));
    compLabel.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (compLabel);
    addAndMakeVisible (compBox);
    compBox.setTooltip ("Browse comping performances and audition their rhythms");
    compBox.onClick = [this] { showPerformanceBrowser(); };
    compAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("compStyle"), nullptr);
    chordRhythmAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("chordRhythm"), nullptr);
    refreshCompingSelection();

    // Synth sound model combo
    setupCombo (synthModelBox, synthModelLabel, "SOUND",
              "Choose the instrument sound for the built-in preview.");
    synthModelBox.addItem ("Clean Saw", 1);
    synthModelBox.addItem ("Rhodes EP", 2);
    synthModelBox.addItem ("Felt Piano", 3);
    synthModelBox.addItem ("Acoustic Piano", 4);
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

    redoButton.getProperties().set ("icon", (int) ui::ButtonIcon::Redo);
    redoButton.onClick = [this] { processor.redo(); };
    addAndMakeVisible (redoButton);

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

    rollBassBtn.setTooltip ("Cycle the note in this slot's separate bass part");
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
            pickerOverlay.open (s, true);
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

void ChordSlotsEditor::showPerformanceBrowser()
{
    refreshCompingSelection();
    const auto progression = processor.getProgression();
    const auto& voicing = progression.slots.front().voicing;
    const int bass = voicing.bassCount > 0 ? voicing.bassCount : voicing.computeBassCount();
    const int voices = std::min (voicing.n, cs::kPerfMaxNotes) - bass;
    const auto safe = juce::Component::SafePointer<ChordSlotsEditor> (this);
    auto browser = std::make_unique<PerformanceBrowser> (static_cast<int> (processor.getPerformanceParams().compStyle),
        cs::kNumCompStyles + cs::kNumExtraCompStyles, cs::compStyleName,
        [voices] (int index) { return compPreview (static_cast<cs::CompStyle> (index), voices); },
        [safe] (int index)
        {
            if (auto* editor = safe.getComponent())
            {
                if (index < cs::kNumCompStyles)
                {
                    editor->compAttachment->setValueAsCompleteGesture ((float) index);
                    editor->chordRhythmAttachment->setValueAsCompleteGesture (0.0f);
                }
                else
                {
                    editor->chordRhythmAttachment->setValueAsCompleteGesture ((float) (index - cs::kNumCompStyles + 1));
                }
                editor->refreshCompingSelection();
            }
        },
        [safe] (int index)
        {
            if (auto* editor = safe.getComponent())
                editor->processor.auditionCompStyle (static_cast<cs::CompStyle> (index));
        });
    auto& callout = juce::CallOutBox::launchAsynchronously (std::move (browser), compBox.getBounds(), this);
    callout.setDismissalMouseClicksAreAlwaysConsumed (true);
}

void ChordSlotsEditor::showBassPatternBrowser()
{
    auto preview = processor.getProgression();
    if (preview.numSlots <= 0)
        return;

    const int selected = static_cast<int> (preview.bassStyle);
    if (preview.numSlots == 1)
        preview.slots[1] = preview.slots[0];
    preview.numSlots = 2;
    preview.bars = 2;
    preview.timeSigNum = preview.timeSigDen = 4;
    preview.slots[0].start = 0;
    preview.slots[1].start = cs::kStepsPerBar;

    const auto safe = juce::Component::SafePointer<ChordSlotsEditor> (this);
    auto browser = std::make_unique<PerformanceBrowser> (selected, (int) std::size (bassPatternNames),
        [] (int index) { return bassPatternNames[index]; },
        [&preview] (int index)
        {
            auto pattern = preview;
            pattern.bassStyle = static_cast<cs::BassStyle> (index);
            return bassPreview (pattern);
        },
        [safe] (int index)
        {
            if (auto* editor = safe.getComponent())
            {
                editor->processor.edit ([index] (cs::Progression& p, cs::Rng&) { p.bassStyle = static_cast<cs::BassStyle> (index); });
                editor->refresh();
            }
        },
        [safe, preview] (int index) mutable
        {
            if (auto* editor = safe.getComponent())
            {
                preview.bassStyle = static_cast<cs::BassStyle> (index);
                editor->processor.auditionBassPattern (preview);
            }
        });
    auto& callout = juce::CallOutBox::launchAsynchronously (std::move (browser), bassPatternBox.getBounds(), this);
    callout.setDismissalMouseClicksAreAlwaysConsumed (true);
}

void ChordSlotsEditor::refreshCompingSelection()
{
    const auto* name = cs::compStyleName (static_cast<int> (processor.getPerformanceParams().compStyle));
    if (compBox.getButtonText() != name)
        compBox.setButtonText (name);
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

void ChordSlotsEditor::showKeyActions()
{
    const auto progression = processor.getProgression();
    juce::PopupMenu menu;
    juce::PopupMenu transpose;
    static constexpr int minShift = -24, maxShift = 24, transposeId = 100;
    bool canTranspose = false;
    const auto label = [] (int shift) { return (shift > 0 ? "+" : "") + juce::String (shift); };
    for (int start : { -24, -12, 1, 13 })
    {
        const int last = start + 11;
        juce::PopupMenu range;
        bool rangeEnabled = false;
        for (int shift = start; shift <= last; ++shift)
        {
            const bool enabled = cs::canTransposeProgression (progression, shift);
            range.addItem (transposeId + shift, label (shift) + " st", enabled);
            rangeEnabled |= enabled;
        }
        transpose.addSubMenu (label (start) + " to " + label (last) + " st", range, rangeEnabled);
        canTranspose |= rangeEnabled;
    }
    menu.addSubMenu ("Transpose", transpose, canTranspose);
    menu.addItem (1, "Go to Relative", cs::hasRelativeKey (progression));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyActionsButton),
        [editor = juce::Component::SafePointer<ChordSlotsEditor> (this)] (int result)
        {
            if (editor == nullptr)
                return;
            if (result == 1)
                editor->processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::goToRelative (p); });
            else if (result >= transposeId + minShift && result <= transposeId + maxShift)
                editor->processor.transposeProgression (result - transposeId);
        });
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
    keyLabel.setText (keyBox.getText() + " " + (p.scale == 1 ? "Natural Minor" : cs::scales()[(size_t) p.scale].name),
                      juce::dontSendNotification);
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
    cs::Progression preset;
    preset.style = p.style;
    for (juce::PopupMenu::MenuItemIterator it (*moodBox.getRootMenu()); it.next();)
    {
        auto& item = it.getItem();
        if (item.itemID == 0)
            continue;

        cs::applyMood (preset, item.itemID - 1);
        const auto transition = styleBox.getItemText (1 + (int) cs::styleForProgression (preset));
        // JUCE renders this as secondary text in the popup, separate from the combo's selected text.
        item.shortcutKeyDescription = juce::String (cs::scales()[(size_t) preset.scale].name)
            + "  |  Adventure " + juce::String (juce::roundToInt (preset.adventure * 100.0f))
            + "%  |  Syncopation " + juce::String (juce::roundToInt (preset.syncopation * 100.0f))
            + "%  |  " + (p.style < 0 ? "Auto uses " : "Keeps ") + transition;
    }
    voicingBox.setSelectedId (p.voicingStyle + 1, juce::dontSendNotification);
    bassPatternBox.setButtonText (bassPatternNames[static_cast<int> (p.bassStyle)]);
    songButton.setToggleState (processor.isSongEnabled(), juce::dontSendNotification);

    lockRhythm.setToggleState (p.locks.rhythm, juce::dontSendNotification);
    lockRoots.setToggleState (p.locks.roots, juce::dontSendNotification);
    lockColors.setToggleState (p.locks.colors, juce::dontSendNotification);
    lockVoicing.setToggleState (p.locks.voicing, juce::dontSendNotification);

    adventure.setValue (p.adventure, juce::dontSendNotification);
    endingBox.setSelectedId (1 + (int) p.ending, juce::dontSendNotification);
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
    redoButton.setEnabled (processor.canRedo());
    redoButton.setTooltip (juce::String ("Redo (Variation ") + varLetters[edVar] + ")");

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
             &keyBox, &keyLabel, &scaleBox, &keyActionsButton, &barsBox, &barsLabel,
             &meterBox, &meterLabel, &slotsBox, &slotsLabel, &snapBox, &snapLabel,
             &fitLabel, &fitCaption, &midiViewBtn, &lanes,
             &moodBox, &moodLabel, &styleBox, &styleLabel, &voicingBox, &voicingLabel,
             &shapeBox, &shapeLabel, &resetCurvesBtn, &analyzeMidiBtn, &melodyFit,
             &melodyFitLabel, &lockCaption, &lockRhythm, &lockRoots, &lockColors,
             &lockVoicing, &adventure, &adventureLabel, &variety, &varietyLabel,
             &syncopation, &syncopationLabel, &bassMotion, &bassMotionLabel, &endingBox, &endingLabel, &rollButton })
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
    refreshCompingSelection();
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
    // Cmd/Ctrl + Shift + Z or Cmd/Ctrl + Y: Redo
    if (key.getModifiers().isCommandDown()
        && ((key.getKeyCode() == 'Z' && key.getModifiers().isShiftDown()) || key.getKeyCode() == 'Y'))
    {
        processor.redo();
        return true;
    }

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
                cs::rollSelectedSlot (pr, sel, rng);
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
    tabRow.removeFromLeft (4);
    redoButton.setBounds (tabRow.removeFromLeft (62).reduced (0, 2));

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
        static constexpr int loopGap = 4;
        auto place = [&loopRow] (juce::ComboBox& box, juce::Label& label, int width)
        {
            auto cell = loopRow.removeFromLeft (width);
            loopRow.removeFromLeft (loopGap);
            label.setBounds (cell.removeFromTop (15));
            box.setBounds (cell);
        };

        auto keyArea = loopRow.removeFromLeft (210);
        keyLabel.setBounds (keyArea.removeFromTop (15));
        keyBox.setBounds (keyArea.removeFromLeft (42));
        keyArea.removeFromLeft (8);
        scaleBox.setBounds (keyArea);
        loopRow.removeFromLeft (loopGap);
        keyActionsButton.setBounds (loopRow.removeFromLeft (86).withTrimmedTop (15));
        loopRow.removeFromLeft (loopGap);
        place (barsBox, barsLabel, 42);
        place (meterBox, meterLabel, 48);
        place (slotsBox, slotsLabel, 46);
        place (snapBox, snapLabel, 54);

        loopRow.removeFromLeft (8);
        place (moodBox, moodLabel, 80);
        place (styleBox, styleLabel, 100);
        place (voicingBox, voicingLabel, 102);

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
    previewPanelBounds = footer.removeFromLeft (192);
    footer.removeFromLeft (gap);
    outputPanelBounds = footer;

    struct Knob { juce::Slider* s; juce::Label* l; };

    // --- Panel 1: Harmony Generator ---
    if (! grooveView)
    {
        auto hInner = harmonyPanelBounds.reduced (8, 4);
        hInner.removeFromTop (16);

        auto harmActions = hInner.removeFromRight (142);
        endingLabel.setBounds (harmActions.removeFromTop (13));
        endingBox.setBounds (harmActions.removeFromTop (20));
        harmActions.removeFromTop (3);
        rollButton.setBounds (harmActions.withSizeKeepingCentre (74, 34));
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
        auto sound = preview.removeFromLeft (106);
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
    pickerOverlay.setBounds (juce::Rectangle<int> (juce::jmin (680, getWidth() - 40), 394)
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
