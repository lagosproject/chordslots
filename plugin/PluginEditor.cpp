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
      songPanel (p),
      gateAttachment (p.apvts, "gate", gate),
      volumeAttachment (p.apvts, "synthVol", volume),
      strumAttachment (p.apvts, "strum", strum),
      accentAttachment (p.apvts, "accent", accent),
      humanizeAttachment (p.apvts, "humanize", humanize),
      splitMidiAttachment (p.apvts, "splitMidi", splitMidiButton)
{
    setLookAndFeel (&lnf);

    for (auto* button : { &loopSummary, &moreOptions, &playAfterRoll, &gestures })
    {
        button->setClickingTogglesState (true);
        addAndMakeVisible (*button);
    }
    addChildComponent (gestureHelp);
    gestureHelp.setText ("Shift-click a chord to listen. Click the timing strip to split a slot. Drag a handle to move it, double-click to lock it, or right-click for more actions.", juce::dontSendNotification);
    gestureHelp.setFont (ui::font (11.0f));
    gestureHelp.setColour (juce::Label::textColourId, ui::textDim);
    gestureHelp.setJustificationType (juce::Justification::topLeft);
    for (auto* button : { &loopSummary, &moreOptions, &gestures })
        button->onClick = [this] { resized(); repaint(); };
    playAfterRoll.setToggleState (true, juce::dontSendNotification);
    loopSummary.setTooltip ("Show tonic, scale, length, meter, slots and grid settings.");

    // Header Combos
    setupCombo (keyBox, keyLabel, "TONIC / SCALE",
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
    fitLabel.setTooltip ("How closely the slots match their brightness and tension targets.");
    fitLabel.setFont (ui::font (14.0f, true));
    fitLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (fitLabel);

    // Variation bank

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
    midiViewBtn.setTooltip ("Inspect voiced pitches beside the targets. Open Groove & Feel for timing, comping and bass patterns.");
    midiViewBtn.onClick = [this]
    {
        timeline.setMidiVisible (midiViewBtn.getToggleState());
        resized();
    };
    timeline.onOpenAlternatives = [this] (int slot)
    {
        if (grooveView || songPanel.isVisible()) return;
        if (pickerOverlay.isVisible() && inspectedSlot == slot)
        {
            pickerOverlay.onClose();
            return;
        }
        inspectedSlot = slot;
        if (pickerOverlay.isVisible())
            pickerOverlay.setSlot (slot);
        else
            pickerOverlay.open (slot, true);
        pickerOverlay.setVisible (true);
        pickerOverlay.toFront (false);
        resized();
        repaint();
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

    addAndMakeVisible (shapeBox);
    shapeBox.setTooltip ("Apply a shape to all three target curves and roll chords to fit it.");
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
    melodyFit.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
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
    pickerOverlay.onClose = [this]
    {
        pickerOverlay.setVisible (false);
        inspectedSlot = -1;
        timeline.setSelectedSlot (-1);
        lanes.setSelectedSlot (-1);
        resized();
        repaint();
    };
    addChildComponent (pickerOverlay);

    // Footer Panel 1: Locks
    lockCaption.setText ("Keep when rolling", juce::dontSendNotification);
    lockCaption.setTooltip ("Generation locks apply across the progression when rolling. Chord locks preserve one chord; separator locks protect one timing boundary.");
    lockCaption.setFont (ui::font (12.0f, true));
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
        d.button->setTooltip (juce::String ("Across the progression: ") + d.tip);
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
    setupKnob (volume, volumeLabel, "VOL",
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

    volume.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
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
    refeelButton.getProperties().set ("primary", true);
    refeelButton.setColour (juce::TextButton::textColourOffId, ui::background);
    refeelButton.onClick = [this] { processor.refeel(); };
    addAndMakeVisible (refeelButton);

    undoButton.setTitle ("Undo");
    undoButton.getProperties().set ("icon", (int) ui::ButtonIcon::Undo);
    undoButton.onClick = [this] { processor.undo(); };
    addAndMakeVisible (undoButton);

    redoButton.setTitle ("Redo");
    redoButton.getProperties().set ("icon", (int) ui::ButtonIcon::Redo);
    redoButton.onClick = [this] { processor.redo(); };
    addAndMakeVisible (redoButton);

    rollButton.getProperties().set ("primary", true);
    rollButton.setColour (juce::TextButton::textColourOffId, ui::background);
    rollButton.setTooltip ("Generate new chords for all unlocked slots");
    rollButton.onClick = [this]
    {
        processor.edit ([] (cs::Progression& pr, cs::Rng& rng) { cs::roll (pr, rng); });
        if (playAfterRoll.getToggleState())
            processor.setPreviewPlaying (true);
        updateTransport();
    };
    addAndMakeVisible (rollButton);

    playButton.setComponentID ("preview-play");
    playButton.getProperties().set ("icon", (int) ui::ButtonIcon::Transport);
    playButton.setTooltip ("Toggle built-in preview synth playback");
    playButton.onClick = [this]
    {
        const bool starting = ! processor.isPreviewPlaying();
        if (starting && songPanel.isVisible() && processor.getSong().numEntries == 0) return;
        if (starting && songPanel.isVisible())
            processor.setSongEnabled (true);
        processor.setPreviewPlaying (starting);
        updateTransport();
    };
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
    songButton.setTooltip ("Arrange variations into a Song chain");
    songButton.onClick = [this] { showSongMenu(); };
    addChildComponent (songPanel);
    songPanel.onChange = [this] { updateTransport(); };

    addAndMakeVisible (dragTile);
    addAndMakeVisible (dragChordsTile);
    addAndMakeVisible (dragBassTile);
    addAndMakeVisible (dragSongTile);
    dragTile.setTooltip ("Drag the current progression onto a MIDI track in your DAW.");
    dragChordsTile.setTooltip ("Drag just the upper chord notes onto a MIDI track in your DAW.");
    dragBassTile.setTooltip ("Drag just the bass notes onto a MIDI track in your DAW.");
    dragSongTile.setTooltip ("Drag the full Song chain onto a MIDI track in your DAW.");

    timeline.onSlotSelected = [this] (int s)
    {
        resized();
        lanes.setSelectedSlot (s);
        if (pickerOverlay.isVisible() && s >= 0 && s != inspectedSlot)
        {
            inspectedSlot = s;
            pickerOverlay.setSlot (s);
        }

    };


    rollSettingsButton.setTitle ("Roll settings");
    rollSettingsButton.setTooltip ("Show or hide the settings for Roll.");
    rollSettingsButton.getProperties().set ("icon", (int) ui::ButtonIcon::Settings);
    rollSettingsButton.onClick = [this] { setRollSettingsOpen (! rollSettingsButton.getToggleState()); };
    addAndMakeVisible (rollSettingsButton);
    rollSettingsPanel.setName ("Roll settings");
    rollSettingsPanel.setComponentID ("roll-settings-panel");
    addChildComponent (rollSettingsPanel);
    for (auto* control : std::initializer_list<juce::Component*> {
             &moodBox, &moodLabel, &lockCaption, &lockRhythm, &lockRoots, &lockColors, &lockVoicing,
             &adventure, &adventureLabel, &variety, &varietyLabel, &syncopation, &syncopationLabel,
             &moreOptions, &playAfterRoll, &styleBox, &styleLabel, &voicingBox, &voicingLabel,
             &bassMotion, &bassMotionLabel, &endingBox, &endingLabel })
        rollSettingsPanel.addChildComponent (*control);
    addMouseListener (this, true);

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
    removeMouseListener (this);
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
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 51, 20);

    addAndMakeVisible (s);

    label.setText (caption, juce::dontSendNotification);
    label.setFont (ui::font (10.0f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    label.setJustificationType (juce::Justification::centredLeft);
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
    setRollSettingsOpen (false);
    pickerOverlay.setVisible (false);
    songPanel.setVisible (! songPanel.isVisible());
    songPanel.refresh();
    songPanel.toFront (false);
    resized();
    repaint();
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
    const auto keyName = keyBox.getText() + " " + (p.scale == 1 ? "Natural Minor" : cs::scales()[(size_t) p.scale].name);
    loopSummary.setButtonText (keyName + juce::String::fromUTF8 ("  ·  ")
        + juce::String (p.bars) + (p.bars == 1 ? " bar" : " bars") + juce::String::fromUTF8 ("  ·  ")
        + juce::String (p.timeSigNum) + "/" + juce::String (p.timeSigDen) + juce::String::fromUTF8 ("  ·  ")
        + juce::String (p.numSlots) + juce::String::fromUTF8 (" slots  ▾"));
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
    songButton.setToggleState (songPanel.isVisible(), juce::dontSendNotification);
    songPanel.refresh();

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
    fitLabel.setText ("Match " + juce::String (fit) + "%", juce::dontSendNotification);
    fitLabel.setColour (juce::Label::textColourId, ui::text);

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
        {
            inspectedSlot = slot;
            pickerOverlay.setSlot (slot);
        }
        else
            pickerOverlay.setVisible (false);
    }
    lanes.setSelectedSlot (timeline.getSelectedSlot());
    resized();
}

void ChordSlotsEditor::showGrooveView (bool show)
{
    setRollSettingsOpen (false);
    songPanel.setVisible (false);
    grooveView = show;
    timeline.setFeelView (show);
    harmonyViewButton.setToggleState (! show, juce::dontSendNotification);
    grooveViewButton.setToggleState (show, juce::dontSendNotification);

    pickerOverlay.setVisible (false);
    timeline.setMidiVisible (show || midiViewBtn.getToggleState());
    resized();
    repaint();
}

void ChordSlotsEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void ChordSlotsEditor::updateDragTileVisibility()
{
    const bool song = songPanel.isVisible();
    const bool visible = ! song || processor.getSong().numEntries > 0;
    const bool split = processor.isSplitMidi();
    dragTile.setVisible (visible && ! song && ! split);
    dragChordsTile.setVisible (visible && ! song && split);
    dragBassTile.setVisible (visible && ! song && split);
    dragSongTile.setVisible (visible && song);
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
    updateTransport();
    refreshCompingSelection();
    if (processor.getTriggeredSlot() >= 0)
        timeline.repaint();
}

void ChordSlotsEditor::updateTransport()
{
    const bool song = songPanel.isVisible();
    const bool playing = processor.isPreviewPlaying();
    const bool canExport = ! song || processor.getSong().numEntries > 0;
    playButton.setButtonText (playing ? "Stop" : song ? "Play song" : "Play");
    playButton.setToggleState (playing, juce::dontSendNotification);
    playButton.setEnabled (playing || canExport);
    saveButton.setButtonText (song ? "Save song MIDI" : "Save MIDI");
    saveButton.setEnabled (canExport);
    updateDragTileVisibility();
}

void ChordSlotsEditor::saveMidi()
{
    const bool song = songPanel.isVisible();
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile (song ? "ChordSlots Song.mid" : "ChordSlots - " + processor.describeProgression() + ".mid");
    chooser = std::make_unique<juce::FileChooser> (song ? "Save Song MIDI" : "Save progression MIDI", start, "*.mid");
    juce::Component::SafePointer<ChordSlotsEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode, [safe, song] (const juce::FileChooser& fc)
    {
        if (! safe || fc.getResult() == juce::File()) return;
        const auto file = fc.getResult().withFileExtension ("mid");
        auto write = [safe, song, file]
        {
            if (! safe) return;
            const auto mode = song ? ChordSlotsProcessor::MidiExportMode::Song : ChordSlotsProcessor::MidiExportMode::Master;
            const auto midi = safe->processor.createMidiFile (mode);
            const auto result = SongPanel::saveMidiTo (file, midi, song || midi.getNumTracks() > 1 ? 1 : 0);
            if (result.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save MIDI", result.getErrorMessage());
        };
        if (file.exists())
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                "Replace MIDI?", "Replace " + file.getFullPathName() + "?", "Replace", "Cancel", safe.getComponent(),
                juce::ModalCallbackFunction::create ([write] (int result) { if (result == 1) write(); }));
        else
            write();
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
    if (! songPanel.isVisible() && (key.getKeyCode() == 'L' || key.getKeyCode() == 'l') && ! key.getModifiers().isAnyModifierKeyDown())
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
    if (! grooveView && ! songPanel.isVisible() && (key.getKeyCode() == 'R' || key.getKeyCode() == 'r')
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
    if (key.getKeyCode() == juce::KeyPress::spaceKey && playButton.isEnabled())
    {
        playButton.triggerClick();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey && rollSettingsPanel.isVisible())
    {
        setRollSettingsOpen (false);
        if (rollSettingsButton.isShowing()) rollSettingsButton.grabKeyboardFocus();
        return true;
    }

    // Escape: close overlay
    if (key.getKeyCode() == juce::KeyPress::escapeKey && pickerOverlay.isVisible())
    {
        pickerOverlay.onClose();
        return true;
    }

    return false;
}

void ChordSlotsEditor::setRollSettingsOpen (bool open)
{
    if (rollSettingsButton.getToggleState() == open) return;
    rollSettingsButton.setToggleState (open, juce::dontSendNotification);
    resized();
    if (open)
        rollSettingsPanel.toFront (false);
}

void ChordSlotsEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto* clicked = event.eventComponent;
    if (rollSettingsPanel.isVisible() && clicked != &rollSettingsButton && clicked != &rollSettingsPanel
        && ! rollSettingsPanel.isParentOf (clicked))
        setRollSettingsOpen (false);
}

void ChordSlotsEditor::RollSettingsPanel::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (area, 7.0f);
    g.setColour (ui::accent.withAlpha (0.72f));
    g.drawRoundedRectangle (area, 7.0f, 1.0f);
    g.setColour (ui::text);
    g.setFont (ui::font (16.0f, true));
    g.drawText ("Roll settings", getLocalBounds().reduced (12, 0).withHeight (36),
                juce::Justification::centredLeft);
}

void ChordSlotsEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);
    for (auto bounds : { railBounds, targetCurvesBounds, footerBounds })
    {
        if (bounds.isEmpty()) continue;
        g.setColour (ui::panel);
        g.fillRoundedRectangle (bounds.toFloat(), 7.0f);
        g.setColour (ui::outline);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 7.0f, 1.0f);
    }

    g.setColour (ui::text);
    if (! railBounds.isEmpty() && ! pickerOverlay.isVisible())
    {
        g.setFont (ui::font (16.0f, true));
        g.drawText ("Feel", railBounds.reduced (12, 0).withHeight (36),
                    juce::Justification::centredLeft);
    }
    g.setColour (ui::outline);
    g.fillRect (footerSeparator);
    g.setColour (ui::text);
    if (! targetCurvesBounds.isEmpty())
    {
        auto legend = targetCurvesBounds.reduced (10, 0).withHeight (26);
        g.setFont (ui::font (12.0f, true));
        g.drawText ("Target curves", legend.removeFromLeft (116), juce::Justification::centredLeft);
        g.setFont (ui::font (11.0f));
        g.setColour (ui::textDim);
        g.drawText ("Colored line: target    White mark: actual", legend, juce::Justification::centredLeft);
    }
}

void ChordSlotsEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    const bool song = songPanel.isVisible();
    const bool harmony = ! song && ! grooveView;
    const bool feel = ! song && grooveView;
    const bool inspecting = harmony && pickerOverlay.isVisible();
    const bool drawer = harmony && loopSummary.getToggleState();
    const bool rollOptions = harmony && rollSettingsButton.getToggleState();
    const bool curves = harmony && ! inspecting;

    auto setVisible = [] (bool show, std::initializer_list<juce::Component*> controls)
    {
        for (auto* control : controls)
            control->setVisible (show);
    };

    setVisible (drawer, { &keyBox, &keyLabel, &scaleBox, &keyActionsButton, &barsBox, &barsLabel,
                          &meterBox, &meterLabel, &slotsBox, &slotsLabel, &snapBox, &snapLabel });
    setVisible (harmony, { &loopSummary, &midiViewBtn, &rollButton, &rollSettingsButton });
    setVisible (curves, { &lanes, &shapeBox, &resetCurvesBtn, &analyzeMidiBtn, &melodyFit, &melodyFitLabel, &gestures });
    rollSettingsPanel.setVisible (rollOptions);
    fitLabel.setVisible (! song);
    setVisible (rollOptions, { &moodBox, &moodLabel, &lockCaption, &lockRhythm, &lockRoots, &lockColors,
                               &lockVoicing, &adventure, &adventureLabel, &variety, &varietyLabel,
                               &syncopation, &syncopationLabel, &moreOptions, &playAfterRoll });
    gestureHelp.setVisible (curves && gestures.getToggleState());
    setVisible (rollOptions && moreOptions.getToggleState(),
                { &styleBox, &styleLabel, &voicingBox, &voicingLabel, &bassMotion, &bassMotionLabel,
                  &endingBox, &endingLabel });
    setVisible (feel, { &compBox, &compLabel, &bassPatternBox, &bassPatternLabel, &refeelButton,
                        &strum, &strumLabel, &accent, &accentLabel, &humanize, &humanizeLabel, &gate, &gateLabel });
    setVisible (! song, { &timeline, &splitMidiButton });


    songButton.setToggleState (song, juce::dontSendNotification);
    harmonyViewButton.setToggleState (harmony, juce::dontSendNotification);
    grooveViewButton.setToggleState (feel, juce::dontSendNotification);

    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int width)
    {
        c.setBounds (row.removeFromLeft (width));
        row.removeFromLeft (5);
    };
    auto header = area.removeFromTop (32);
    place (header, harmonyViewButton, 77);
    place (header, grooveViewButton, 54);
    place (header, songButton, 54);
    header.removeFromLeft (14);
    place (header, undoButton, 32);
    place (header, redoButton, 32);
    if (optionsButton.isVisible())
        optionsButton.setBounds (header.removeFromRight (65));
    header.removeFromRight (5);
    inputButton.setBounds (header.removeFromRight (85));
    header.removeFromRight (5);
    auto variations = header.removeFromRight (255);
    for (auto& button : varButtons)
        place (variations, button, 27);
    place (variations, followButton, 59);
    place (variations, copyVarButton, 62);
    area.removeFromTop (12);

    footerBounds = area.removeFromBottom (45);
    auto footer = footerBounds.reduced (8, 7);
    auto exports = footer.removeFromRight (song ? 217 : 292);
    footerSeparator = {};
    if (! song)
    {
        place (footer, fitLabel, 85);
        auto rollGroup = footer.removeFromLeft (109);
        if (harmony)
        {
            rollButton.setBounds (rollGroup.removeFromLeft (77));
            rollSettingsButton.setBounds (rollGroup.withTrimmedLeft (2));
        }
        else
        {
            refeelButton.setBounds (rollGroup.withWidth (88));
        }
        auto separator = footer.removeFromLeft (15);
        footerSeparator = separator.withSizeKeepingCentre (1, 21);
    }
    place (footer, playButton, 86);
    place (footer, synthModelBox, 124);
    place (footer, volumeLabel, 29);
    place (footer, volume, 82);
    place (exports, saveButton, song ? 115 : 77);
    if (! song) place (exports, splitMidiButton, 85);
    dragTile.setBounds (exports);
    dragSongTile.setBounds (exports);
    dragChordsTile.setBounds (exports.removeFromLeft (exports.getWidth() / 2).withTrimmedRight (2));
    dragBassTile.setBounds (exports);
    updateTransport();

    area.removeFromBottom (9);
    railBounds = targetCurvesBounds = {};
    if (song)
    {
        songPanel.setBounds (area);
        return;
    }


    auto field = [] (juce::Rectangle<int>& column, juce::Component& label, juce::Component& control)
    {
        auto row = column.removeFromTop (27);
        label.setBounds (row.removeFromLeft (98));
        control.setBounds (row.reduced (0, 2));
    };
    if (rollOptions)
    {
        const int height = moreOptions.getToggleState() ? 366 : 258;
        rollSettingsPanel.setBounds (rollButton.getX(), footerBounds.getY() - height - 5, 328, height);
        auto rail = rollSettingsPanel.getLocalBounds().reduced (12, 7).withTrimmedTop (29);
        field (rail, moodLabel, moodBox);
        lockCaption.setBounds (rail.removeFromTop (22));
        auto keep = rail.removeFromTop (27);
        for (auto* button : { &lockRhythm, &lockRoots, &lockColors, &lockVoicing })
            place (keep, *button, 69);
        rail.removeFromTop (5);
        field (rail, adventureLabel, adventure);
        field (rail, varietyLabel, variety);
        field (rail, syncopationLabel, syncopation);
        playAfterRoll.setBounds (rail.removeFromTop (26));
        moreOptions.setBounds (rail.removeFromTop (27).withWidth (132));
        if (moreOptions.getToggleState())
        {
            field (rail, bassMotionLabel, bassMotion);
            field (rail, endingLabel, endingBox);
            field (rail, styleLabel, styleBox);
            field (rail, voicingLabel, voicingBox);
        }
    }
    if (feel)
    {
        railBounds = area.removeFromRight (328);
        area.removeFromRight (10);
        auto rail = railBounds.reduced (12, 7).withTrimmedTop (29);
        field (rail, compLabel, compBox);
        field (rail, bassPatternLabel, bassPatternBox);
        rail.removeFromTop (10);
        field (rail, strumLabel, strum);
        field (rail, accentLabel, accent);
        field (rail, humanizeLabel, humanize);
        field (rail, gateLabel, gate);
        timeline.setBounds (area);
        return;
    }

    auto stageHeader = area.removeFromTop (30);
    loopSummary.setBounds (stageHeader.removeFromLeft (juce::jmin (350, stageHeader.getWidth() - 77)));
    midiViewBtn.setBounds (stageHeader.removeFromRight (70));
    if (drawer)
    {
        auto key = area.removeFromTop (40).withTrimmedTop (3);
        keyLabel.setBounds (key.removeFromTop (13));
        place (key, keyBox, 54);
        place (key, scaleBox, 206);
        place (key, keyActionsButton, 92);
        auto settings = area.removeFromTop (40).withTrimmedTop (3);
        auto setting = [&settings] (juce::Label& label, juce::ComboBox& box, int width)
        {
            auto cell = settings.removeFromLeft (width).withTrimmedRight (4);
            label.setBounds (cell.removeFromTop (14));
            box.setBounds (cell);
        };
        setting (barsLabel, barsBox, 59);
        setting (meterLabel, meterBox, 63);
        setting (slotsLabel, slotsBox, 55);
        setting (snapLabel, snapBox, 62);

    }
    area.removeFromTop (8);
    const int timelineHeight = midiViewBtn.getToggleState() ? juce::jlimit (137, 210, area.getHeight() - 173) : 120;
    timeline.setBounds (area.removeFromTop (timelineHeight));
    area.removeFromTop (8);
    if (inspecting)
    {
        pickerOverlay.setBounds (area);
        if (rollOptions) rollSettingsPanel.toFront (false);
        return;
    }
    targetCurvesBounds = area;
    area.removeFromTop (26);
    auto tools = area.removeFromBottom (38).reduced (8, 5);
    if (gestures.getToggleState())
        gestureHelp.setBounds (area.removeFromBottom (26).reduced (8, 0));
    place (tools, shapeBox, 134);
    place (tools, resetCurvesBtn, 68);
    place (tools, analyzeMidiBtn, 134);
    tools.removeFromLeft (10);
    place (tools, melodyFitLabel, 86);
    place (tools, melodyFit, 138);
    gestures.setBounds (tools.removeFromRight (92));
    area.removeFromBottom (4);
    lanes.setBounds (midiViewBtn.getToggleState() ? area.withTrimmedLeft (44) : area);
}
