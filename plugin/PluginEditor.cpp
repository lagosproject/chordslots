// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "EditorPreferences.h"
#include "ui/ProgressionLibrary.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

constexpr auto showTooltipsKey = "showTooltips";
constexpr auto languageKey = "language";
constexpr auto progressionLibraryKey = "progressionLibrary";

class ChordImportPreview final : public juce::Component
{
public:
    ChordImportPreview (ChordSlotsProcessor::ChordImport data,
                        std::function<void (const cs::Progression&)> apply)
        : proposal (std::move (data))
    {
        setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (help);
        addAndMakeVisible (scroll);
        addAndMakeVisible (error);
        addAndMakeVisible (confirm);
        addAndMakeVisible (cancel);
        const auto& p = proposal.progression;
        help.setJustificationType (juce::Justification::topLeft);
        help.setText (ui::tr ("Choose chord names and edit slot starts in steps (24 per quarter note).")
                      + "\n" + juce::String (p.bars) + " " + ui::tr ("bars") + "  /  "
                      + juce::String (p.timeSigNum) + "/" + juce::String (p.timeSigDen)
                      + (proposal.adjustsTiming
                         ? "\n" + ui::tr ("Slot timing fills rests or rounds to the loop grid.")
                         : juce::String {}), juce::dontSendNotification);
        scroll.setViewedComponent (&rows, false);
        for (int i = 0; i < p.numSlots; ++i)
        {
            auto& row = controls[(size_t) i];
            for (auto* component : std::initializer_list<juce::Component*> { &row.label, &row.cut, &row.chord })
                rows.addAndMakeVisible (*component);
            const auto slot = ui::tr ("Slot") + " " + juce::String (i + 1);
            row.label.setText (slot, juce::dontSendNotification);
            row.cut.setTitle (slot + " " + ui::tr ("Start step"));
            row.cut.setInputRestrictions (4, "0123456789");
            row.cut.setText (juce::String (p.slots[(size_t) i].start), false);
            row.cut.setReadOnly (i == 0);
            const auto& choices = proposal.choices[(size_t) i];
            row.chord.setTitle (slot + " " + ui::tr ("Chord"));
            for (size_t j = 0; j < choices.size(); ++j)
                row.chord.addItem (ui::chordName (choices[j], p.tonic, p.scale), (int) j + 1);
            row.chord.setSelectedId (1, juce::dontSendNotification);
            if (choices.size() > 1)
                row.label.setText (slot + " (" + ui::tr ("Ambiguous") + ")", juce::dontSendNotification);
        }
        cancel.onClick = [this] { close(); };
        confirm.onClick = [this, apply = std::move (apply)]
        {
            auto p = proposal.progression;
            int previous = -1;
            for (int i = 0; i < p.numSlots; ++i)
            {
                const auto text = controls[(size_t) i].cut.getText();
                const int cut = text.getIntValue();
                if (text.isEmpty() || cut <= previous || cut >= p.lengthSteps())
                {
                    error.setText (ui::tr ("Enter increasing starts inside the loop."), juce::dontSendNotification);
                    return;
                }
                p.slots[(size_t) i].start = cut;
                previous = cut;
            }
            cs::Rng rng (1);
            for (int i = 0; i < p.numSlots; ++i)
            {
                const auto selected = (size_t) (controls[(size_t) i].chord.getSelectedId() - 1);
                cs::setChord (p, i, proposal.choices[(size_t) i][selected], rng);
            }
            apply (p);
            close();
        };
        setSize (540, 370);
    }

    ~ChordImportPreview() override
    {
        scroll.setViewedComponent (nullptr);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override { g.fillAll (ui::background); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        help.setBounds (area.removeFromTop (65));
        auto buttons = area.removeFromBottom (32);
        confirm.setBounds (buttons.removeFromRight (112));
        buttons.removeFromRight (8);
        cancel.setBounds (buttons.removeFromRight (88));
        error.setBounds (area.removeFromBottom (25));
        scroll.setBounds (area);
        rows.setSize (area.getWidth() - scroll.getScrollBarThickness(), proposal.progression.numSlots * 34);
        for (int i = 0; i < proposal.progression.numSlots; ++i)
        {
            auto line = juce::Rectangle<int> (0, i * 34, rows.getWidth(), 29);
            auto& row = controls[(size_t) i];
            row.label.setBounds (line.removeFromLeft (155));
            row.cut.setBounds (line.removeFromLeft (65));
            line.removeFromLeft (8);
            row.chord.setBounds (line);
        }
    }

private:
    void close()
    {
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);
    }

    struct Row
    {
        juce::Label label;
        juce::TextEditor cut;
        juce::ComboBox chord;
    };

    ChordSlotsProcessor::ChordImport proposal;
    ChordSlotsLookAndFeel lookAndFeel;
    juce::Component rows;
    juce::Viewport scroll;
    std::array<Row, cs::kMaxSlots> controls;
    juce::Label help, error;
    juce::TextButton confirm { ui::tr ("Import Chords") }, cancel { ui::tr ("Cancel") };
};

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

bool saveProgressionLibrary (const juce::ValueTree& bank)
{
    auto& settings = editorSettings();
    const auto previous = settings.getValue (progressionLibraryKey);
    const auto xml = bank.createXml();
    settings.setValue (progressionLibraryKey, xml.get());
    if (settings.saveIfNeeded())
        return true;

    if (previous.isEmpty())
        settings.removeValue (progressionLibraryKey);
    else
        settings.setValue (progressionLibraryKey, previous);
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
        ui::tr ("Progression Library"), ui::tr ("The progression library could not be saved."));
    return false;
}

juce::ValueTree loadProgressionLibrary()
{
    if (const auto xml = editorSettings().getXmlValue (progressionLibraryKey))
        if (auto tree = juce::ValueTree::fromXml (*xml); tree.hasType ("ProgressionLibrary"))
            return tree;
    return juce::ValueTree ("ProgressionLibrary");
}

constexpr const char* bassPatternNames[] { "Sustain", "Octave pump", "Offbeat 8ths", "808 legato", "Walking" };

struct PreviewNote
{
    double start, duration;
    float velocity;
    int pitch = -1;
};

struct PatternPreview
{
    int steps;
    std::vector<PreviewNote> notes;
};

juce::String compingName (cs::CompStyle style)
{
    if (cs::isArpeggio (style))
        return ui::tr ("Arp");
    return cs::isPulse (style) ? ui::tr ("Pulse") : ui::tr (cs::compStyleName (static_cast<int> (style)));
}

PatternPreview compPreview (cs::CompStyle style, int voices, const cs::PerformanceParams& settings)
{
    PatternPreview preview { cs::compPreviewBars (style) * cs::kStepsPerBar, {} };
    cs::CompHit hits[cs::kMaxCompHitsPerSlot];
    const int count = cs::getCompHits (style, preview.steps, hits, cs::kMaxCompHitsPerSlot,
                                      4, 4, 0, settings.arpRateSteps, settings.arpLength);
    auto params = settings;
    params.compStyle = style;
    const bool pulse = cs::isPulse (style);
    const int pulseRate = pulse ? cs::pulseRateFor (params) : 1;
    if (! pulse)
        cs::shapeCompHits (hits, count, params);
    const int numHits = pulse ? (preview.steps + pulseRate - 1) / pulseRate : count;
    for (int i = 0; i < numHits; ++i)
    {
        const auto hit = pulse ? cs::pulseHit (i, preview.steps, pulseRate, params.compHitLength,
                                              params.compPatternAccent) : hits[i];
        const int pitch = hit.arpVoice != 0 && voices > 0
            ? cs::arpSequenceIndex (hit.arpVoice, voices * settings.arpOctaves, style == cs::CompStyle::ArpUpDown) : -1;
        preview.notes.push_back ({ (double) hit.stepOffset, (double) hit.durationSteps, hit.velScale, pitch });
    }
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
            preview.notes.push_back ({ (double) slot.start, (double) (slot.end - slot.start), 1.0f, slot.primaryBass });
        else
            for (int h = 0; h < slot.numBassHits; ++h)
            {
                const auto& hit = slot.bassHits[h];
                const int start = slot.start + hit.stepOffset;
                preview.notes.push_back ({ (double) start, (double) std::min (hit.durationSteps, play.lengthSteps - start),
                                           hit.velocityScale, hit.pitch });
            }
    }
    return preview;
}

PatternPreview melodyRhythmPreview (const cs::Progression& progression, cs::PerformanceParams settings)
{
    settings.compStyle = cs::CompStyle::FollowMelody;
    const auto play = cs::toPlayData (progression, settings);
    PatternPreview preview { play.lengthSteps, {} };
    for (int i = 0; i < play.numSlots; ++i)
    {
        const auto& slot = play.slots[i];
        for (int h = 0; h < slot.numHits; ++h)
        {
            const auto hit = play.hitAt (slot, h);
            preview.notes.push_back ({ slot.start + hit.stepOffset, hit.durationSteps, hit.velScale });
        }
    }
    return preview;
}

class PerformancePreviewButton final : public juce::Button
{
public:
    PerformancePreviewButton (juce::String name, PatternPreview pattern)
        : juce::Button (ui::tr ("Audition ") + name), preview (std::move (pattern))
    {
        setTooltip (ui::tr ("Click to audition at the current tempo while playback is stopped."));
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
        const auto xAt = [&] (double step) { return grid.getX() + grid.getWidth() * (float) (step / preview.steps); };
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
    PerformanceBrowser (int selected, int numStyles, const std::function<juce::String (int)>& nameFor,
                        const std::function<PatternPreview (int)>& patternFor,
                        std::function<void (int)> apply, std::function<void (int)> audition)
    {
        for (int i = 0; i < numStyles; ++i)
        {
            const juce::String label (nameFor (i));
            auto* name = names.add (new juce::TextButton (i == selected
                ? juce::String::charToString (0x2713) + " " + label : label));
            name->setToggleState (i == selected, juce::dontSendNotification);
            name->setTooltip (ui::tr ("Apply this performance to the progression"));
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
            names[i]->setBounds (row.removeFromLeft (170));
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
      locale (ui::languageFromCode (editorSettings().getValue (languageKey))),
      timeline (p),
      pickerOverlay (p),
      lanes (p),
      dragTile (p, ChordSlotsProcessor::MidiExportMode::Master, ui::tr ("Drag MIDI")),
      dragChordsTile (p, ChordSlotsProcessor::MidiExportMode::ChordsOnly, ui::tr ("Chords")),
      dragBassTile (p, ChordSlotsProcessor::MidiExportMode::BassOnly, ui::tr ("Bass")),
      dragSongTile (p, ChordSlotsProcessor::MidiExportMode::Song, ui::tr ("Drag Song")),
      dragArpTile (p, ChordSlotsProcessor::MidiExportMode::ArpOnly, ui::tr ("Drag arp")),
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
    gestureHelp.setText (ui::tr ("Shift-click a chord to listen. Click the timing strip to split a slot. Drag a handle to move it, double-click to lock it, or right-click for more actions."), juce::dontSendNotification);
    gestureHelp.setFont (ui::font (11.0f));
    gestureHelp.setColour (juce::Label::textColourId, ui::textDim);
    gestureHelp.setJustificationType (juce::Justification::topLeft);
    for (auto* button : { &loopSummary, &moreOptions, &gestures })
        button->onClick = [this] { resized(); repaint(); };
    playAfterRoll.setToggleState (true, juce::dontSendNotification);
    loopSummary.setTooltip (ui::tr ("Show tonic, scale, length, meter, slots and grid settings."));

    // Header Combos
    setupCombo (keyBox, keyLabel, ui::tr ("TONIC / SCALE"),
              ui::tr ("Choose the tonic for scale highlighting, suggestions and Roll."));
    keyBox.setName (ui::tr ("Tonic"));
    scaleBox.setName (ui::tr ("Scale"));
    scaleBox.setTooltip (ui::tr ("Choose the scale for highlighting, suggestions and Roll."));
    scaleBox.setTextWhenNothingSelected (ui::tr ("Scale"));
    addAndMakeVisible (scaleBox);
    keyActionsButton.setTooltip (ui::tr ("Transpose the progression or go to the relative key."));
    keyActionsButton.onClick = [this] { showKeyActions(); };
    addAndMakeVisible (keyActionsButton);
    setupCombo (barsBox, barsLabel, ui::tr ("BARS"),
              ui::tr ("Set the length of the progression in bars."));
    setupCombo (meterBox, meterLabel, ui::tr ("METER"),
              ui::tr ("Choose the time signature. Host follows your DAW."));
    setupCombo (slotsBox, slotsLabel, ui::tr ("SLOTS"),
              ui::tr ("Set the number of chord slots. Use the timeline to add or remove individual cuts."));
    setupCombo (snapBox, snapLabel, ui::tr ("GRID"),
              ui::tr ("Set the note division used to snap timeline cuts."));

    for (int pc = 0; pc < 12; ++pc)
        keyBox.addItem (ui::pitchClassName (pc, pc, 0), pc + 1);

    for (int i = 0; i < cs::numScales(); ++i)
    {
        const char* name = cs::scales()[(size_t) i].name;
        if (i == 0) name = "Major (Ionian)";
        if (i == 1) name = "Natural Minor (Aeolian)";
        scaleBox.addItem (ui::tr (name), i + 1);
    }

    for (int b : { 1, 2, 4, 8 })
        barsBox.addItem (juce::String (b), b);

    meterBox.addItem ("4/4", 1);
    meterBox.addItem ("3/4", 2);
    meterBox.addItem ("6/8", 3);
    meterBox.addItem (ui::tr ("Host"), 4);

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
    inputButton.setTooltip (ui::tr ("Configure incoming MIDI trigger mode and note mapping"));
    inputButton.onClick = [this] { showInputMenu(); };
    addAndMakeVisible (inputButton);

    optionsButton.setTooltip (ui::tr ("Change editor preferences such as hover help."));
    optionsButton.onClick = [this] { showOptionsMenu(); };
    addAndMakeVisible (optionsButton);
    progressionLibraryButton.setTooltip (ui::tr ("Browse, preview and save chord progression presets."));
    progressionLibraryButton.onClick = [this] { showProgressionLibrary(); };
    addAndMakeVisible (progressionLibraryButton);
    updateTooltips();

    keyBox.onChange = [this]
    {
        if (refreshing) return;
        const int tonic = keyBox.getSelectedId() - 1;
        if (tonic >= 0)
            processor.edit ([tonic] (cs::Progression& pr, cs::Rng&)
            {
                pr.autoMelodyKey = false;
                cs::setTonalContext (pr, tonic, pr.scale);
            });
    };

    scaleBox.onChange = [this]
    {
        if (refreshing) return;
        const int scale = scaleBox.getSelectedId() - 1;
        if (scale >= 0)
            processor.edit ([scale] (cs::Progression& pr, cs::Rng&)
            {
                pr.autoMelodyKey = false;
                cs::setTonalContext (pr, pr.tonic, scale);
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
    fitLabel.setTooltip (ui::tr ("How closely the slots match their brightness and tension targets."));
    fitLabel.setFont (ui::font (14.0f, true));
    fitLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (fitLabel);

    // Variation bank

    followButton.setButtonText (ui::tr ("Follow"));
    followButton.setClickingTogglesState (true);
    followButton.setTooltip (ui::tr ("When enabled, editing follows the playing variation"));
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
        varButtons[i].setTooltip (juce::String (ui::tr ("Switch to Variation ")) + varNames[i]);
        varButtons[i].onClick = [this, i] { selectVariation (i); };
        addAndMakeVisible (varButtons[i]);
    }

    variationBox.setTooltip (ui::tr ("Select a variation."));
    variationBox.onChange = [this]
    {
        if (variationBox.getSelectedId() > 0)
            selectVariation (variationBox.getSelectedId() - 1);
    };
    addChildComponent (variationBox);
    addVariationButton.setTitle (ui::tr ("Add variation"));
    addVariationButton.setTooltip (ui::tr ("Add a variation by copying the current one."));
    addVariationButton.onClick = [this]
    {
        if (const int index = processor.addVariation(); index >= 0)
            selectVariation (index);
    };
    addAndMakeVisible (addVariationButton);

    copyVarButton.setTooltip (ui::tr ("Copy current variation to another slot"));
    copyVarButton.onClick = [this]
    {
        const int cur = processor.getEditVariation();
        juce::PopupMenu menu;
        for (int i = 0; i < processor.getVariationCount(); ++i)
        {
            if (i != cur)
                menu.addItem (i + 1, juce::String (ui::tr ("Copy ")) + juce::String::charToString ('A' + cur)
                                     + ui::tr (" to ") + juce::String::charToString ('A' + i));
        }
        const juce::Component::SafePointer<ChordSlotsEditor> editor (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&copyVarButton),
                            [editor, cur] (int result)
        {
            if (editor != nullptr && result > 0)
                editor->processor.copyVariation (cur, result - 1);
        });
    };
    addAndMakeVisible (copyVarButton);

    // Views
    addAndMakeVisible (timeline);
    addAndMakeVisible (harmonyViewButton);
    addAndMakeVisible (grooveViewButton);
    harmonyViewButton.setTooltip (ui::tr ("Show target curves, chord generation controls and locks."));
    grooveViewButton.setTooltip (ui::tr ("Show controls for playing patterns, timing and dynamics."));
    harmonyViewButton.onClick = [this] { showGrooveView (false); };
    grooveViewButton.onClick = [this] { showGrooveView (true); };
    addAndMakeVisible (midiViewBtn);
    midiViewBtn.setClickingTogglesState (true);
    midiViewBtn.setTooltip (ui::tr ("Inspect voiced pitches beside the targets. Open Groove & Feel for timing, comping and bass patterns."));
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

    setupCombo (moodBox, moodLabel, ui::tr ("MOOD PRESET"),
              ui::tr ("Open the menu to see what each preset applies before choosing it."));
    moodBox.setTextWhenNothingSelected (ui::tr ("Custom"));
    moodBox.getRootMenu()->addSectionHeader (ui::tr ("All presets replace Brightness, Tension and Energy target curves and generate a progression (also runs a Roll)"));
    const auto& m = cs::moods();
    for (int i = 0; i < (int) m.size(); ++i)
        moodBox.addItem (ui::tr (m[(size_t) i].name), i + 1);

    setupCombo (styleBox, styleLabel, ui::tr ("CHORD TRANSITIONS"),
              ui::tr ("Choose which chord changes Roll favors. Auto follows the mood and scale. Free removes that preference."));
    styleBox.addItem (ui::tr ("Auto"), 1);
    styleBox.addItem (ui::tr ("Major"), 2);
    styleBox.addItem (ui::tr ("Minor"), 3);
    styleBox.addItem (ui::tr ("Extended harmony"), 4);
    styleBox.addItem (ui::tr ("Free"), 5);
    styleBox.setSelectedId (1, juce::dontSendNotification);

    setupCombo (voicingBox, voicingLabel, ui::tr ("VOICING"),
              ui::tr ("Choose how chord notes are spread across registers."));
    for (int i = 0; i < cs::kNumVoicingStyles; ++i)
        {
        const auto style = (cs::VoicingStyle) i;
        voicingBox.addItem (style == cs::VoicingStyle::Close && ui::EditorLocale::language() != ui::Language::english
                                ? ui::tr ("Close voicing")
                                                           : ui::tr (cs::voicingStyleName (style)), i + 1);
    }
    voicingBox.setSelectedId ((int) cs::VoicingStyle::Piano + 1, juce::dontSendNotification);

    addAndMakeVisible (shapeBox);
    shapeBox.setTooltip (ui::tr ("Apply a shape to all three target curves and roll chords to fit it."));
    shapeBox.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem (1, ui::tr ("Rise (Gradual build)"));
        menu.addItem (2, ui::tr ("Drop (Gradual release)"));
        menu.addItem (3, ui::tr ("Arch (Peak center)"));
        menu.addItem (4, ui::tr ("Pop Build (Step up)"));
        menu.addItem (5, ui::tr ("Valley (Dip center)"));
        juce::Component::SafePointer<ChordSlotsEditor> editor (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapeBox), [editor] (int stamp)
        {
            if (editor != nullptr && stamp != 0)
                editor->applyShapeStamp (stamp);
        });
    };

    bassPatternLabel.setText (ui::tr ("BASS PATTERN"), juce::dontSendNotification);
    bassPatternLabel.setFont (ui::font (10.0f, true));
    bassPatternLabel.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (bassPatternLabel);
    addAndMakeVisible (bassPatternBox);
    bassPatternBox.setTooltip (ui::tr ("Browse and audition bass patterns"));
    bassPatternBox.onClick = [this] { showBassPatternBrowser(); };
    setupCombo (arpDirectionBox, arpDirectionLabel, ui::tr ("DIRECTION"), ui::tr ("Choose the order in which the arp plays the chord voices"));
    arpDirectionBox.setComponentID ("arp-direction");
    int arpDirectionId = 1;
    for (const auto* direction : cs::kArpDirectionNames)
        arpDirectionBox.addItem (ui::tr (direction), arpDirectionId++);
    arpDirectionAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("arpDirection"), nullptr);
    arpDirectionBox.onChange = [this]
    {
        if (const int direction = arpDirectionBox.getSelectedId(); direction > 0)
        {
            if (processor.isArpAuditioning())
                processor.stopAlternativeAudition();
            arpDirectionAttachment->setValueAsCompleteGesture ((float) direction);
        }
    };
    setupCombo (arpRateBox, arpRateLabel, ui::tr ("ARP RATE"), ui::tr ("Set the interval between arpeggio notes"));
    arpRateBox.setComponentID ("arp-rate");
    int arpRateId = 1;
    for (const auto* rate : cs::kArpRateNames)
        arpRateBox.addItem (rate, arpRateId++);
    arpRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "arpRate", arpRateBox);
    setupKnob (arpLength, arpLengthLabel, ui::tr ("ARP LENGTH"), ui::tr ("Set each arp note's length as a percentage of its rate interval"));
    arpLength.setComponentID ("arp-length");
    arpLength.setTextValueSuffix ("%");
    arpLengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "arpLength", arpLength);
    setupCombo (arpOctavesBox, arpOctavesLabel, ui::tr ("OCTAVE RANGE"), ui::tr ("Extend the upper chord voices into higher octaves"));
    arpOctavesBox.setComponentID ("arp-octaves");
    for (int octave = 1; octave <= cs::kMaxArpOctaves; ++octave)
        arpOctavesBox.addItem (juce::String (octave) + (octave == 1 ? ui::tr (" octave") : ui::tr (" octaves")), octave);
    arpOctavesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "arpOctaves", arpOctavesBox);
    setupCombo (pulseRateBox, pulseRateLabel, ui::tr ("PULSE RATE"), ui::tr ("Set the subdivision between repeated chord hits"));
    pulseRateBox.setComponentID ("pulse-rate");
    int pulseRateId = 1;
    for (const auto* rate : cs::kArpRateNames)
        pulseRateBox.addItem (rate, pulseRateId++);
    pulseRateAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("pulseRate"), nullptr);
    pulseRateBox.onChange = [this]
    {
        const int rate = pulseRateBox.getSelectedId();
        if (rate > 0)
            pulseRateAttachment->setValueAsCompleteGesture ((float) rate);
    };
    setupKnob (compPatternAccent, compPatternAccentLabel, ui::tr ("PATTERN ACCENT"), ui::tr ("Adjust velocity contrast between accents in the comping pattern"));
    compPatternAccent.setComponentID ("comp-pattern-accent");
    compPatternAccent.setTextValueSuffix ("%");
    compPatternAccentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "compPatternAccent", compPatternAccent);
    arpAuditionButton.getProperties().set ("icon", (int) ui::ButtonIcon::Transport);
    arpAuditionButton.setComponentID ("hear-arp");
    arpAuditionButton.setTooltip (ui::tr ("Preview only the arp over the current loop while the DAW is stopped"));
    arpAuditionButton.onClick = [this]
    {
        if (processor.isArpAuditioning())
            processor.stopAlternativeAudition();
        else
            processor.auditionArp();
    };
    arpSaveButton.getProperties().set ("icon", (int) ui::ButtonIcon::Save);
    arpSaveButton.setTooltip (ui::tr ("Save the arp by itself as a MIDI file"));
    arpSaveButton.onClick = [this] { saveMidi (ChordSlotsProcessor::MidiExportMode::ArpOnly); };
    dragArpTile.setTooltip (ui::tr ("Drag the arp by itself onto a MIDI track in your DAW"));
    addChildComponent (arpAuditionButton);
    addChildComponent (arpSaveButton);
    addChildComponent (dragArpTile);
    resetCurvesBtn.getProperties().set ("quiet", true);
    resetCurvesBtn.setTooltip (ui::tr ("Set all three target curves to 50% and roll chords to fit them."));
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

    startMelodyButton.getProperties().set ("icon", (int) ui::ButtonIcon::Import);
    startMelodyButton.setComponentID ("start-with-melody");
    startMelodyButton.setTooltip (ui::tr ("Choose a MIDI melody to harmonize, or drop a MIDI file in Harmony"));
    startMelodyButton.onClick = [this] { importMidi (MidiImport::Melody); };
    addAndMakeVisible (startMelodyButton);
    analyzeMidiBtn.setTooltip (ui::tr ("Extract target curves from MIDI or clear the imported melody"));
    analyzeMidiBtn.onClick = [this] { showMidiImportMenu(); };
    addAndMakeVisible (analyzeMidiBtn);
    melodyAuditionButton.getProperties().set ("icon", (int) ui::ButtonIcon::Transport);
    melodyAuditionButton.setComponentID ("midi-hear-melody");
    melodyAuditionButton.setTooltip (ui::tr ("Preview melody, chords and bass over the current loop while the DAW is stopped"));
    melodyAuditionButton.onClick = [this]
    {
        if (processor.isMelodyAuditioning())
            processor.stopAlternativeAudition();
        else
            processor.auditionMelody();
    };
    addAndMakeVisible (melodyAuditionButton);
    melodyTimingButton.setComponentID ("midi-timing");
    melodyTimingButton.setTooltip (ui::tr ("Switch between the file's original beat positions and scaling the phrase to the loop"));
    melodyTimingButton.onClick = [this]
    {
        processor.stopAlternativeAudition();
        processor.edit ([] (cs::Progression& p, cs::Rng&) { p.fitMelodyToLoop = ! p.fitMelodyToLoop; });
        refresh();
    };
    midiDetailsButton.setComponentID ("midi-details");
    midiDetailsButton.setClickingTogglesState (true);
    midiDetailsButton.onClick = [this] { resized(); repaint(); };
    autoMelodyKeyButton.setComponentID ("midi-auto-key");
    autoMelodyKeyButton.setClickingTogglesState (true);
    autoMelodyKeyButton.setTooltip (ui::tr ("Use the detected key and scale to harmonize the melody"));
    autoMelodyKeyButton.onClick = [this]
    {
        processor.stopAlternativeAudition();
        processor.setAutoMelodyKey (autoMelodyKeyButton.getToggleState());
        refresh();
    };
    for (auto* button : { &melodyTimingButton, &midiDetailsButton, &autoMelodyKeyButton })
        addChildComponent (*button);
    midiFilename.setComponentID ("midi-filename");
    midiKeyDetails.setComponentID ("midi-key-details");
    midiTempoDetails.setComponentID ("midi-tempo-details");
    midiFilename.setFont (ui::font (13.0f, true));
    midiFilename.setColour (juce::Label::textColourId, ui::text);
    addChildComponent (midiFilename);
    for (auto* label : { &midiKeyDetails, &midiTempoDetails })
    {
        label->setFont (ui::font (12.0f));
        label->setColour (juce::Label::textColourId, ui::textDim);
        addChildComponent (*label);
    }
    if (processor.usesStandaloneTempo())
    {
        standaloneTempo.setComponentID ("standalone-bpm");
        standaloneTempo.setName (ui::tr ("Tempo"));
        standaloneTempo.setSliderStyle (juce::Slider::IncDecButtons);
        standaloneTempo.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 66, 27);
        standaloneTempo.setRange (20.0, 999.0, 0.1);
        standaloneTempo.setTextValueSuffix (" BPM");
        standaloneTempo.setTooltip (ui::tr ("Standalone tempo. To stretch an imported phrase, use Fit to Loop in Melody MIDI."));
        standaloneTempo.onValueChange = [this]
        {
            if (processor.isMelodyAuditioning())
                processor.stopAlternativeAudition();
            processor.setAutoMidiTempoEnabled (false);
            autoMidiTempoButton.setToggleState (false, juce::dontSendNotification);
            processor.setStandaloneBpm (standaloneTempo.getValue());
        };
        addAndMakeVisible (standaloneTempo);
        autoMidiTempoButton.setComponentID ("auto-midi-tempo");
        autoMidiTempoButton.setClickingTogglesState (true);
        autoMidiTempoButton.onClick = [this]
        {
            if (processor.isMelodyAuditioning())
                processor.stopAlternativeAudition();
            processor.setAutoMidiTempoEnabled (autoMidiTempoButton.getToggleState());
            standaloneTempo.setValue (processor.getStandaloneBpm(), juce::dontSendNotification);
        };
        addAndMakeVisible (autoMidiTempoButton);
    }

    setupKnob (melodyFit, melodyFitLabel, ui::tr ("MELODY FIT"),
              ui::tr ("Set how strongly Roll favors chords that fit the imported melody. Set to 0% to ignore it."));
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
    lockCaption.setText (ui::tr ("Keep when rolling"), juce::dontSendNotification);
    lockCaption.setTooltip (ui::tr ("Generation locks apply across the progression when rolling. Chord locks preserve one chord; separator locks protect one timing boundary."));
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
        d.button->setTooltip (ui::tr ("Across the progression: ") + ui::tr (d.tip));
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
    setupCombo (endingBox, endingLabel, ui::tr ("Ending"), ui::tr ("Harmonic ending preference for the next Roll."));
    endingBox.setComponentID ("ending");
    endingBox.addItem (ui::tr ("Free"), 1);
    endingBox.addItem (ui::tr ("Resolve to Tonic"), 2);
    endingBox.addItem (ui::tr ("Lead Back to Start"), 3);
    endingBox.onChange = [this]
    {
        const int id = endingBox.getSelectedId();
        if (! refreshing && id != 0)
            processor.edit ([id] (cs::Progression& p, cs::Rng&) { p.ending = (cs::Ending) (id - 1); }, false);
    };

    setupKnob (adventure, adventureLabel, ui::tr ("ADVENTURE"),
              ui::tr ("Raise this to let Roll use more borrowed chords and extensions."));
    setupKnob (variety, varietyLabel, ui::tr ("VARIETY"),
              ui::tr ("Raise this to explore more chord choices each time you Roll."));
    setupKnob (syncopation, syncopationLabel, ui::tr ("SYNCOPATION"),
              ui::tr ("Raise this to move more unlocked chord changes off the regular beat during Roll."));
    setupKnob (bassMotion, bassMotionLabel, ui::tr ("BASS MOTION"),
              ui::tr ("Raise this to let Roll use inversions for smoother movement between bass notes."));
    setupKnob (strum, strumLabel, ui::tr ("STRUM"),
              ui::tr ("Spread the upper chord notes in time. Positive plays low to high. Negative plays high to low."));
    setupKnob (accent, accentLabel, ui::tr ("ACCENT"),
              ui::tr ("Raise this to emphasize the highest chord note and soften the inner notes."));
    setupKnob (humanize, humanizeLabel, ui::tr ("HUMANIZE"),
              ui::tr ("Add small variations in note timing and velocity for a more natural feel."));
    setupKnob (gate, gateLabel, ui::tr ("GATE"),
              ui::tr ("Set how long notes are held relative to their rhythmic duration."));
    setupKnob (volume, volumeLabel, ui::tr ("VOL"),
              ui::tr ("Adjust the loudness of the built-in preview instrument."));

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

    compLabel.setText (ui::tr ("COMPING"), juce::dontSendNotification);
    compLabel.setFont (ui::font (10.0f, true));
    compLabel.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (compLabel);
    addAndMakeVisible (compBox);
    compBox.setTooltip (ui::tr ("Browse comping performances and audition their rhythms"));
    compBox.onClick = [this] { showPerformanceBrowser(); };
    compAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("compStyle"), nullptr);
    followMelodyRhythmAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("followMelodyRhythm"), nullptr);
    chordRhythmAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("chordRhythm"), nullptr);
    refreshCompingSelection();

    // Synth sound model combo
    setupCombo (synthModelBox, synthModelLabel, ui::tr ("SOUND"),
              ui::tr ("Choose the instrument sound for the built-in preview."));
    synthModelBox.addItem (ui::tr ("Clean Saw"), 1);
    synthModelBox.addItem ("Rhodes EP", 2);
    synthModelBox.addItem (ui::tr ("Felt Piano"), 3);
    synthModelBox.addItem (ui::tr ("Acoustic Piano"), 4);
    synthModelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "synthModel", synthModelBox);
#if defined (CHORDSLOTS_MIDI_FX) && CHORDSLOTS_MIDI_FX
    synthModelLabel.setVisible (false);
    synthModelBox.setVisible (false);
    volumeLabel.setVisible (false);
    volume.setVisible (false);
#endif

    // Buttons
    refeelButton.getProperties().set ("icon", (int) ui::ButtonIcon::Dice);
    refeelButton.setTooltip (ui::tr ("Roll a different chord rhythm while keeping the harmony and bass pattern."));
    refeelButton.getProperties().set ("primary", true);
    refeelButton.setColour (juce::TextButton::textColourOffId, ui::background);
    refeelButton.onClick = [this]
    {
        const auto active = processor.getPerformanceParams().compStyle;
        std::vector<cs::CompStyle> choices;
        for (int i = 1; i < cs::kNumCompStyles + cs::kNumExtraCompStyles; ++i)
        {
            const auto style = static_cast<cs::CompStyle> (i);
            if (style != active && style != cs::CompStyle::PulseEighth && ! cs::isArpeggio (style)
                && ! (cs::isPulse (style) && cs::isPulse (active)))
                choices.push_back (style);
        }
        const auto style = choices[(size_t) juce::Random::getSystemRandom().nextInt ((int) choices.size())];
        applyCompStyle (style);
        processor.refeel();
        if (! processor.isPreviewPlaying() && processor.getPlayPosition() < 0.0)
            processor.auditionCompStyle (style);
    };
    addAndMakeVisible (refeelButton);

    undoButton.setTitle (ui::tr ("Undo"));
    undoButton.getProperties().set ("quiet", true);
    undoButton.getProperties().set ("icon", (int) ui::ButtonIcon::Undo);
    undoButton.onClick = [this] { processor.undo(); };
    addAndMakeVisible (undoButton);

    redoButton.setTitle (ui::tr ("Redo"));
    redoButton.getProperties().set ("quiet", true);
    redoButton.getProperties().set ("icon", (int) ui::ButtonIcon::Redo);
    redoButton.onClick = [this] { processor.redo(); };
    addAndMakeVisible (redoButton);

    rollButton.getProperties().set ("primary", true);
    rollButton.setColour (juce::TextButton::textColourOffId, ui::background);
    rollButton.setComponentID ("start-with-chords");
    rollButton.getProperties().set ("icon", (int) ui::ButtonIcon::Dice);
    rollButton.setTooltip (ui::tr ("Start with chords or roll new chords for all unlocked slots"));
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
    playButton.setTooltip (ui::tr ("Toggle built-in preview synth playback"));
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
    saveButton.setTooltip (ui::tr ("Export progression as standard MIDI file"));
    saveButton.onClick = [this]
    {
        saveMidi (songPanel.isVisible() ? ChordSlotsProcessor::MidiExportMode::Song
                                        : ChordSlotsProcessor::MidiExportMode::Master);
    };
    addAndMakeVisible (saveButton);

    splitMidiButton.setClickingTogglesState (true);
    splitMidiButton.setTooltip (ui::tr ("Route upper chords to MIDI Ch 1 and monophonic bass to MIDI Ch 2"));
    addAndMakeVisible (splitMidiButton);
    addAndMakeVisible (songButton);
    songButton.setToggleState (p.isSongEnabled(), juce::dontSendNotification);
    songButton.setTooltip (ui::tr ("Arrange variations into a Song chain"));
    songButton.onClick = [this] { showSongView(); };
    harmonyViewButton.getProperties().set ("workspaceTabAccent", ui::harmonyAccent.toString());
    grooveViewButton.getProperties().set ("workspaceTabAccent", ui::feelAccent.toString());
    songButton.getProperties().set ("workspaceTabAccent", ui::songAccent.toString());
    addChildComponent (songPanel);
    songPanel.onChange = [this]
    {
        dragSongTile.setIncludeSongMelody (songPanel.includesMelody());
        updateTransport();
    };

    addAndMakeVisible (dragTile);
    addAndMakeVisible (dragChordsTile);
    addAndMakeVisible (dragBassTile);
    addAndMakeVisible (dragSongTile);
    dragTile.setTooltip (ui::tr ("Drag the current progression onto a MIDI track in your DAW."));
    dragChordsTile.setTooltip (ui::tr ("Drag just the upper chord notes onto a MIDI track in your DAW."));
    dragBassTile.setTooltip (ui::tr ("Drag just the bass notes onto a MIDI track in your DAW."));
    dragSongTile.setTooltip (ui::tr ("Drag the full Song chain onto a MIDI track in your DAW."));

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


    rollSettingsButton.setTitle (ui::tr ("Roll settings"));
    rollSettingsButton.setTooltip (ui::tr ("Show or hide the settings for Roll."));
    rollSettingsButton.getProperties().set ("quiet", true);
    rollSettingsButton.getProperties().set ("icon", (int) ui::ButtonIcon::Settings);
    rollSettingsButton.onClick = [this] { setRollSettingsOpen (! rollSettingsButton.getToggleState()); };
    addAndMakeVisible (rollSettingsButton);
    rollSettingsPanel.setName (ui::tr ("Roll settings"));
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
    midiViewBtn.setToggleState (! processor.getProgression().melody.empty(), juce::dontSendNotification);
    showGrooveView (false);
    setResizeLimits (900, 570, 1920, 1200);
    setSize (1040, 650);
    startTimerHz (30);
}

ChordSlotsEditor::~ChordSlotsEditor()
{
    if (progressionLibraryWindow != nullptr)
    {
        progressionLibraryWindow->exitModalState (0);
        processor.stopAlternativeAudition();
    }
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
    const auto settings = processor.getPerformanceParams();
    std::vector<cs::CompStyle> styles;
    for (int i = 0; i < cs::kNumCompStyles + cs::kNumExtraCompStyles; ++i)
    {
        const auto style = static_cast<cs::CompStyle> (i);
        if (style == cs::CompStyle::ArpUp)
            styles.push_back (settings.arpDirection);
        else if (cs::isArpeggio (style))
            continue;
        else if (style == cs::CompStyle::PulseQuarter && settings.compStyle == cs::CompStyle::PulseEighth)
            styles.push_back (cs::CompStyle::PulseEighth);
        else if (style != cs::CompStyle::PulseEighth)
            styles.push_back (style);
    }
    if (! progression.melody.empty())
        styles.insert (styles.begin() + 1, cs::CompStyle::FollowMelody);
    const auto effectiveStyle = settings.compStyle == cs::CompStyle::FollowMelody && progression.melody.empty()
        ? cs::CompStyle::Sustain : settings.compStyle;
    const auto selected = std::find (styles.begin(), styles.end(), effectiveStyle);
    auto browser = std::make_unique<PerformanceBrowser> (static_cast<int> (selected - styles.begin()), (int) styles.size(),
        [styles] (int row) { return compingName (styles[(size_t) row]); },
        [&progression, voices, settings, styles] (int row)
        {
            const auto style = styles[(size_t) row];
            return style == cs::CompStyle::FollowMelody ? melodyRhythmPreview (progression, settings)
                                                       : compPreview (style, voices, settings);
        },
        [safe, styles] (int row)
        {
            if (auto* editor = safe.getComponent())
            {
                editor->applyCompStyle (styles[(size_t) row]);
            }
        },
        [safe, styles] (int row)
        {
            if (auto* editor = safe.getComponent())
                editor->processor.auditionCompStyle (styles[(size_t) row]);
        });
    auto& callout = juce::CallOutBox::launchAsynchronously (std::move (browser), compBox.getBounds(), this);
    callout.setDismissalMouseClicksAreAlwaysConsumed (true);
}

void ChordSlotsEditor::applyCompStyle (cs::CompStyle style)
{
    const int index = (int) style;
    if (style == cs::CompStyle::FollowMelody)
    {
        followMelodyRhythmAttachment->setValueAsCompleteGesture (1.0f);
    }
    else
    {
        if (index < cs::kNumCompStyles)
        {
            compAttachment->setValueAsCompleteGesture ((float) index);
            chordRhythmAttachment->setValueAsCompleteGesture (0.0f);
        }
        else
        {
            chordRhythmAttachment->setValueAsCompleteGesture ((float) (index - cs::kNumCompStyles + 1));
        }
        followMelodyRhythmAttachment->setValueAsCompleteGesture (0.0f);
    }
    refresh();
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
        [] (int index) { return ui::tr (bassPatternNames[index]); },
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
    const auto settings = processor.getPerformanceParams();
    const auto style = settings.compStyle == cs::CompStyle::FollowMelody && processor.getProgression().melody.empty()
        ? cs::CompStyle::Sustain : settings.compStyle;
    const auto name = compingName (style);
    if (cs::isArpeggio (settings.compStyle))
        arpDirectionBox.setSelectedId ((int) settings.arpDirection - (int) cs::CompStyle::ArpUp + 1, juce::dontSendNotification);
    if (cs::isPulse (settings.compStyle))
    {
        const auto rate = std::find (cs::kArpRateSteps.begin(), cs::kArpRateSteps.end(), cs::pulseRateFor (settings));
        pulseRateBox.setSelectedId (1 + static_cast<int> (rate - cs::kArpRateSteps.begin()), juce::dontSendNotification);
    }
    if (compBox.getButtonText() != name)
    {
        if (processor.isArpAuditioning())
            processor.stopAlternativeAudition();
        compBox.setButtonText (name);
    }
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

void ChordSlotsEditor::selectVariation (int index)
{
    processor.setEditVariation (index);
    if (processor.isFollowingPlayback())
        processor.setPlayingVariation (index);
    updateVariationControls();
    refresh();
}

void ChordSlotsEditor::updateVariationControls()
{
    const int count = processor.getVariationCount();
    const int selected = processor.getEditVariation();
    if (variationBox.getNumItems() != count)
    {
        variationBox.clear (juce::dontSendNotification);
        for (int i = 0; i < count; ++i)
            variationBox.addItem (ui::tr ("Variation ") + juce::String::charToString ('A' + i), i + 1);
    }
    variationBox.setSelectedId (selected + 1, juce::dontSendNotification);
    addVariationButton.setEnabled (count < cs::kMaxVariations);
    addVariationButton.setTooltip (count < cs::kMaxVariations ? ui::tr ("Add a variation by copying the current one.")
                                                              : ui::tr ("All eight variations are in use."));
    for (int i = 0; i < cs::kNumVariations; ++i)
        varButtons[i].setToggleState (i == selected, juce::dontSendNotification);

    if (variationBox.isVisible() != (count > cs::kNumVariations))
        resized();
}

void ChordSlotsEditor::showProgressionLibrary()
{
    const auto safe = juce::Component::SafePointer<ChordSlotsEditor> (this);
    auto browser = std::make_unique<ui::ProgressionLibrary> (loadProgressionLibrary, saveProgressionLibrary,
        [safe] { return safe != nullptr ? safe->processor.getProgression() : cs::Progression {}; },
        [safe] (const cs::Progression& preset)
        {
            if (safe == nullptr)
                return;
            safe->processor.edit ([&preset] (cs::Progression& current, cs::Rng&) { current = preset; });
            safe->showGrooveView (false);
            safe->refresh();
        },
        [safe] (const cs::Progression& preset)
        {
            if (safe != nullptr)
                safe->processor.auditionAlternative (preset, 0, true);
        },
        [safe]
        {
            if (safe != nullptr)
                safe->processor.stopAlternativeAudition();
        });
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = ui::tr ("Progression Library");
    options.dialogBackgroundColour = ui::background;
    options.content.setOwned (browser.release());
    options.componentToCentreAround = this;
    options.associatedComponent = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    progressionLibraryWindow = options.launchAsync();
}

void ChordSlotsEditor::showOptionsMenu()
{
    juce::PopupMenu menu;
    const bool enabled = editorPreferences::tooltipsEnabled();
    menu.addItem (1, ui::tr ("Show Tooltips"), true, enabled);
    const auto selected = ui::languageFromCode (editorSettings().getValue (languageKey));
    juce::PopupMenu languages;
    languages.addItem (10, "English", true, selected == ui::Language::english);
    languages.addItem (11, juce::String::fromUTF8 (u8"Espa\u00f1ol"), true, selected == ui::Language::spanish);
    languages.addItem (12, juce::String::fromUTF8 (u8"Fran\u00e7ais"), true, selected == ui::Language::french);
    menu.addSubMenu (ui::tr ("Language"), languages);

    juce::Component::SafePointer<ChordSlotsEditor> editor (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsButton), [editor] (int result)
    {
        if (editor == nullptr)
            return;
        if (result == 1)
        {
            editorPreferences::setTooltipsEnabled (! editorPreferences::tooltipsEnabled());
            editor->updateTooltips();
        }
        else if (result >= 10 && result <= 12)
        {
            constexpr const char* codes[] { "en", "es", "fr" };
            auto& settings = editorSettings();
            const auto* code = codes[result - 10];
            settings.setValue (languageKey, code);
            const bool saved = settings.saveIfNeeded();
            if (! saved || ui::languageFromCode (code) != ui::EditorLocale::language())
                juce::AlertWindow::showMessageBoxAsync (
                    saved ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                    ui::tr ("Language"),
                    ui::tr (saved ? "Close and reopen all ChordSlots windows to apply the language."
                                  : "Language could not be saved."),
                    ui::tr ("OK"), editor.getComponent());
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
        transpose.addSubMenu (label (start) + ui::tr (" to ") + label (last) + " st", range, rangeEnabled);
        canTranspose |= rangeEnabled;
    }
    menu.addSubMenu (ui::tr ("Transpose"), transpose, canTranspose);
    menu.addItem (1, ui::tr ("Go to Relative"), cs::hasRelativeKey (progression));
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

void ChordSlotsEditor::showSongView()
{
    if (songPanel.isVisible())
        return;

    setRollSettingsOpen (false);
    pickerOverlay.setVisible (false);
    songPanel.setVisible (true);
    songPanel.refresh();
    songPanel.toFront (false);
    resized();
    repaint();
}

void ChordSlotsEditor::showInputMenu()
{
    const cs::TriggerConfig tc = processor.getTriggerConfig();
    juce::PopupMenu menu;
    menu.addSectionHeader (ui::tr ("MIDI TRIGGER MODE"));
    menu.addItem (1, ui::tr ("Auto (Priority to live keys)"), true, tc.mode == cs::TriggerMode::Auto);
    menu.addItem (2, ui::tr ("Trigger Only (Mute loop)"), true, tc.mode == cs::TriggerMode::TriggerOnly);
    menu.addItem (3, ui::tr ("Loop Only (Ignore live keys)"), true, tc.mode == cs::TriggerMode::LoopOnly);

    menu.addSeparator();
    menu.addSectionHeader (ui::tr ("NOTE TO SLOT MAPPING"));
    const bool isWhite = (tc.layout == cs::TriggerLayout::WhiteKeys);
    const bool isPad = (tc.layout == cs::TriggerLayout::Chromatic && tc.rootNote == 36);
    const bool isChromC3 = (tc.layout == cs::TriggerLayout::Chromatic && tc.rootNote == 60);

    const auto mappingLabel = [] (const char* type, int octave)
    {
        return ui::tr (type) + " (" + ui::noteSpelling ("C") + juce::String (octave)
             + " = " + ui::tr ("Slot ") + "0)";
    };
    menu.addItem (10, mappingLabel ("White Keys", 3), true, isWhite);
    menu.addItem (11, mappingLabel ("Chromatic", 3), true, isChromC3);
    menu.addItem (12, mappingLabel ("Pads", 1), true, isPad);

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
        keyBox.addItem (ui::pitchClassName (pc, pc, scale), pc + 1);
}

void ChordSlotsEditor::refresh()
{
    standaloneTempo.setValue (processor.getStandaloneBpm(), juce::dontSendNotification);
    const cs::Progression p = processor.getProgression();
    if (processor.usesStandaloneTempo())
    {
        autoMidiTempoButton.setToggleState (processor.isAutoMidiTempoEnabled(), juce::dontSendNotification);
        const juce::String source = p.melodySourceBpm > 0.0
            ? ui::tr ("Imported tempo: ") + juce::String (p.melodySourceBpm, 1) + " BPM. "
            : ui::tr ("No file tempo available. ");
        autoMidiTempoButton.setTooltip (source + ui::tr ("Use the MIDI file's first tempo when importing. Enter a BPM to switch to manual tempo."));
    }
    const juce::ScopedValueSetter<bool> guard (refreshing, true);

    rebuildKeyNames (p.scale);
    keyBox.setSelectedId (p.tonic + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (p.scale + 1, juce::dontSendNotification);
    const auto keyName = keyBox.getText() + " " + ui::tr (p.scale == 1 ? "Natural Minor" : cs::scales()[(size_t) p.scale].name);
    loopSummary.setButtonText (keyName + juce::String::fromUTF8 ("  ·  ")
        + juce::String (p.bars) + (p.bars == 1 ? ui::tr (" bar") : ui::tr (" bars")) + juce::String::fromUTF8 ("  ·  ")
        + juce::String (p.timeSigNum) + "/" + juce::String (p.timeSigDen) + juce::String::fromUTF8 ("  ·  ")
        + juce::String (p.numSlots) + ui::tr (u8" slots  \u25be"));
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
        item.shortcutKeyDescription = ui::tr (cs::scales()[(size_t) preset.scale].name)
            + ui::tr ("  |  Adventure ") + juce::String (juce::roundToInt (preset.adventure * 100.0f))
            + ui::tr ("%  |  Syncopation ") + juce::String (juce::roundToInt (preset.syncopation * 100.0f))
            + "%  |  " + (p.style < 0 ? ui::tr ("Auto uses ") : ui::tr ("Keeps ")) + transition;
    }
    voicingBox.setSelectedId (p.voicingStyle + 1, juce::dontSendNotification);
    bassPatternBox.setButtonText (ui::tr (bassPatternNames[static_cast<int> (p.bassStyle)]));
    refreshCompingSelection();
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
    updateMidiImportLabel (p);
    melodyAuditionButton.setEnabled (! p.melody.empty());
    const auto filename = p.melodyFilename.empty() ? juce::String (ui::tr ("Imported melody")) : utf8 (p.melodyFilename);
    midiFilename.setText (p.melody.empty() ? juce::String() : filename, juce::dontSendNotification);
    midiFilename.setTooltip (filename);
    melodyTimingButton.setEnabled (p.melodySourceSteps > 0.0);
    melodyTimingButton.setButtonText (p.melodySourceSteps <= 0.0 ? ui::tr ("Loop timing")
                                    : p.fitMelodyToLoop ? ui::tr ("Fit to Loop") : ui::tr ("Original timing"));
    autoMelodyKeyButton.setToggleState (p.autoMelodyKey, juce::dontSendNotification);
    const auto detectedKey = p.melodySourceTonic >= 0
        ? ui::pitchClassName (p.melodySourceTonic, p.melodySourceTonic, p.melodySourceScale)
            + (p.melodySourceScale == 0 ? ui::tr (" Major") : ui::tr (" Natural Minor"))
        : juce::String (ui::tr ("Unclear"));
    midiKeyDetails.setText (ui::tr ("Detected key: ") + detectedKey + ui::tr ("   |   Using: ") + keyName, juce::dontSendNotification);
    const auto fileTempo = p.melodySourceBpm > 0.0 ? juce::String (p.melodySourceBpm, 1) + " BPM" : juce::String (ui::tr ("Unavailable"));
    midiTempoDetails.setText (ui::tr ("File tempo: ") + fileTempo + ui::tr ("   |   Playback: ")
                              + juce::String (processor.getLastBpm(), 1) + " BPM", juce::dontSendNotification);

    const cs::TriggerConfig tc = processor.getTriggerConfig();
    const char* modeNames[] = { "Auto", "Trig", "Loop" };
    inputButton.setButtonText (juce::String (ui::tr ("In: ")) + ui::tr (modeNames[(int) tc.mode]) + " ▾");

    const int fit = juce::roundToInt (100.0f * cs::fitScore (p));
    fitLabel.setText (ui::tr ("Match ") + juce::String (fit) + "%", juce::dontSendNotification);
    fitLabel.setColour (juce::Label::textColourId, ui::text);

    undoButton.setEnabled (processor.canUndo());
    const auto variationName = juce::String::charToString ('A' + processor.getEditVariation());
    undoButton.setTooltip (juce::String (ui::tr ("Undo (Variation ")) + variationName + ")");
    redoButton.setEnabled (processor.canRedo());
    redoButton.setTooltip (juce::String (ui::tr ("Redo (Variation ")) + variationName + ")");

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

    updateVariationControls();
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
    const bool hearingMelody = processor.isMelodyAuditioning();
    melodyAuditionButton.getProperties().set ("auditionPlaying", hearingMelody);
    melodyAuditionButton.setButtonText (hearingMelody ? ui::tr ("Stop melody") : ui::tr ("Hear melody"));
    const bool hearingArp = processor.isArpAuditioning();
    arpAuditionButton.getProperties().set ("auditionPlaying", hearingArp);
    arpAuditionButton.setButtonText (hearingArp ? ui::tr ("Stop arp") : ui::tr ("Hear arp"));
    const bool canExport = ! song || processor.getSong().numEntries > 0;
    playButton.setButtonText (playing ? ui::tr ("Stop") : song ? ui::tr ("Play song") : ui::tr ("Play"));
    playButton.setToggleState (playing, juce::dontSendNotification);
    const bool featuredPlay = song && canExport && ! playing;
    if (playButton.getProperties().getWithDefault ("primary", false) != juce::var (featuredPlay))
    {
        playButton.getProperties().set ("primary", featuredPlay);
        playButton.repaint();
    }
    playButton.setEnabled (playing || canExport);
    saveButton.setButtonText (song ? ui::tr ("Save song MIDI") : ui::tr ("Save MIDI"));
    saveButton.setEnabled (canExport);
    updateDragTileVisibility();
}

void ChordSlotsEditor::saveMidi (ChordSlotsProcessor::MidiExportMode mode)
{
    const bool song = mode == ChordSlotsProcessor::MidiExportMode::Song;
    const bool arp = mode == ChordSlotsProcessor::MidiExportMode::ArpOnly;
    const auto midi = processor.createMidiFile (mode, song && songPanel.includesMelody());
    if (midi.getNumTracks() == 0)
        return;
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile (song ? "ChordSlots Song.mid" : arp ? "ChordSlots Arp.mid"
                     : "ChordSlots - " + processor.describeProgression() + ".mid");
    chooser = std::make_unique<juce::FileChooser> (song ? ui::tr ("Save Song MIDI") : arp ? ui::tr ("Save Arp MIDI") : ui::tr ("Save progression MIDI"), start, "*.mid");
    juce::Component::SafePointer<ChordSlotsEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode, [safe, song, midi] (const juce::FileChooser& fc)
    {
        if (! safe || fc.getResult() == juce::File()) return;
        const auto file = fc.getResult().withFileExtension ("mid");
        auto write = [safe, song, file, midi]
        {
            if (! safe) return;
            const auto result = SongPanel::saveMidiTo (file, midi, song || midi.getNumTracks() > 1 ? 1 : 0);
            if (result.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, ui::tr ("Save MIDI"), result.getErrorMessage());
        };
        if (file.exists())
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                ui::tr ("Replace MIDI?"), ui::tr ("Replace ") + file.getFullPathName() + "?", ui::tr ("Replace"), ui::tr ("Cancel"), safe.getComponent(),
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
    const auto p = processor.getProgression();
    juce::PopupMenu menu;
    menu.addItem (1, ui::tr ("Analyze MIDI"));
    menu.addItem (4, ui::tr ("Import Chords"));
    menu.addItem (3, ui::tr ("Clear Melody"), ! p.melody.empty());
    auto editor = juce::Component::SafePointer<ChordSlotsEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&analyzeMidiBtn), [editor] (int command)
    {
        if (editor == nullptr)
            return;
        if (command == 1)
            editor->importMidi (MidiImport::Curves);
        else if (command == 4)
            editor->importMidi (MidiImport::Chords);
        else if (command == 3)
        {
            editor->processor.stopAlternativeAudition();
            editor->processor.edit ([] (cs::Progression& p, cs::Rng&)
            {
                p.melody.clear();
                p.melodyFilename.clear();
                p.melodySourceSteps = 0.0;
                p.melodySourceBpm = 0.0;
                p.melodySourceTonic = -1;
                p.melodySourceScale = -1;
                p.fitMelodyToLoop = false;
            });
        }
    });
}

void ChordSlotsEditor::updateMidiImportLabel (const cs::Progression& p)
{
    startMelodyButton.setButtonText (midiDragOver ? ui::tr ("Release MIDI")
                                                : p.melody.empty() ? ui::tr ("Start with a Melody") : ui::tr ("Replace Melody"));
}

bool ChordSlotsEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (grooveView || songPanel.isVisible() || files.size() != 1)
        return false;
    const auto extension = files[0].fromLastOccurrenceOf (".", true, false);
    return extension.equalsIgnoreCase (".mid") || extension.equalsIgnoreCase (".midi");
}

void ChordSlotsEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    midiDragOver = true;
    updateMidiImportLabel (processor.getProgression());
}

void ChordSlotsEditor::fileDragExit (const juce::StringArray&)
{
    midiDragOver = false;
    updateMidiImportLabel (processor.getProgression());
}

void ChordSlotsEditor::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragExit (files);
    if (! isInterestedInFileDrag (files))
        return;

    juce::PopupMenu menu;
    menu.addItem (2, ui::tr ("Harmonize Melody"));
    menu.addItem (1, ui::tr ("Analyze MIDI"));
    menu.addItem (4, ui::tr ("Import Chords"));
    auto editor = juce::Component::SafePointer<ChordSlotsEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&startMelodyButton),
                        [editor, file = juce::File (files[0])] (int command)
    {
        if (editor == nullptr)
            return;
        if (command == 1)
            editor->importMidiFile (file, MidiImport::Curves);
        else if (command == 2)
            editor->importMidiFile (file, MidiImport::Melody);
        else if (command == 4)
            editor->importMidiFile (file, MidiImport::Chords);
    });
}

void ChordSlotsEditor::importMidiFile (const juce::File& file, MidiImport mode)
{
    if (mode == MidiImport::Chords)
    {
        showChordImport (file);
        return;
    }
    if (mode == MidiImport::Curves)
    {
        if (processor.loadMidiToCurves (file))
            processor.edit ([] (cs::Progression& p, cs::Rng& rng) { cs::roll (p, rng); });
        return;
    }

    if (! processor.harmonizeMidi (file))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, ui::tr ("Harmonize Melody"),
            ui::tr (u8"Choose a beat-based MIDI file with a pitched track containing 1\u20134096 notes."));
        return;
    }

    processor.stopAlternativeAudition();
    midiViewBtn.setToggleState (true, juce::dontSendNotification);
    midiDetailsButton.setToggleState (true, juce::dontSendNotification);
    timeline.setMidiVisible (true);
    refresh();
    resized();
}

void ChordSlotsEditor::showChordImport (const juce::File& file)
{
    auto proposal = processor.analyzeChordMidi (file);
    if (! proposal)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, ui::tr ("Import Chords"),
            ui::tr ("Choose a beat-based MIDI clip with one pitched track and channel, up to eight bars and 16 supported block chords."),
            {}, this);
        return;
    }

    auto preview = std::make_unique<ChordImportPreview> (std::move (*proposal),
        [safe = juce::Component::SafePointer<ChordSlotsEditor> (this)] (const cs::Progression& imported)
        {
            if (safe == nullptr)
                return;
            safe->processor.stopAlternativeAudition();
            safe->processor.edit ([&] (cs::Progression& p, cs::Rng&) { p = imported; });
            safe->refresh();
        });
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = ui::tr ("Preview Chord Import");
    options.dialogBackgroundColour = ui::background;
    options.content.setOwned (preview.release());
    options.componentToCentreAround = this;
    options.associatedComponent = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void ChordSlotsEditor::importMidi (MidiImport mode)
{
    const auto title = mode == MidiImport::Chords ? ui::tr ("Select a block-chord MIDI file...")
                     : mode == MidiImport::Melody ? ui::tr ("Select a MIDI melody...")
                     : ui::tr ("Select MIDI file to extract curves...");
    chooser = std::make_unique<juce::FileChooser> (title,
                                                    juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                    "*.mid;*.midi");
    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    auto editor = juce::Component::SafePointer<ChordSlotsEditor> (this);
    chooser->launchAsync (browserFlags, [editor, mode] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (editor == nullptr || ! file.existsAsFile())
            return;

        editor->importMidiFile (file, mode);
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
    g.setColour (ui::harmonyAccent.withAlpha (0.72f));
    g.drawRoundedRectangle (area, 7.0f, 1.0f);
    g.setColour (ui::text);
    g.setFont (ui::font (16.0f, true));
    g.drawText (ui::tr ("Roll settings"), getLocalBounds().reduced (12, 0).withHeight (36),
                juce::Justification::centredLeft);
}

void ChordSlotsEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);
    g.setGradientFill (juce::ColourGradient::vertical (ui::panelHi, (float) workspaceTabBounds.getY(),
                                                       ui::panel, (float) workspaceTabBounds.getBottom()));
    g.fillRoundedRectangle (workspaceTabBounds.toFloat(), 8.0f);
    auto brand = workspaceTabBounds.reduced (13, 0).withWidth (139);
    auto mark = brand.removeFromLeft (27);
    int markRow = 0;
    for (const auto ink : { ui::harmonyAccent, ui::feelAccent, ui::songAccent })
    {
        g.setColour (ink);
        g.fillRoundedRectangle ((float) mark.getX() + 3.0f * markRow,
                                (float) mark.getCentreY() - 6.0f + 5.0f * markRow, 15.0f, 2.5f, 1.0f);
        ++markRow;
    }
    g.setColour (ui::text);
    g.setFont (ui::font (19.0f, true));
    g.drawText ("ChordSlots", brand, juce::Justification::centredLeft);
    const auto workspaceAccent = songPanel.isVisible() ? ui::songAccent
                                                       : grooveView ? ui::feelAccent : ui::harmonyAccent;
    g.setColour (ui::outline.interpolatedWith (workspaceAccent, 0.30f));
    g.fillRect (workspaceTabBounds.withTop (workspaceTabBounds.getBottom() - 1));

    for (auto bounds : { railBounds, targetCurvesBounds, footerBounds, midiPanelBounds })
    {
        if (bounds.isEmpty()) continue;
        const bool isWorkspacePanel = bounds == railBounds || bounds == targetCurvesBounds;
        g.setColour (ui::panel.interpolatedWith (workspaceAccent, isWorkspacePanel ? 0.045f : 0.0f));
        g.fillRoundedRectangle (bounds.toFloat(), 7.0f);
        g.setColour (ui::outline.interpolatedWith (workspaceAccent, isWorkspacePanel ? 0.24f : 0.0f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 7.0f, 1.0f);
    }

    g.setColour (workspaceAccent);
    if (! railBounds.isEmpty())
    {
        g.setFont (ui::font (16.0f, true));
        g.drawText (ui::tr ("Performance"), railBounds.reduced (12, 0).withHeight (27),
                    juce::Justification::centredLeft);
    }
    g.setFont (ui::font (15.0f, true));
    if (! stageTitleBounds.isEmpty())
        g.drawFittedText (ui::tr ("Chord progression"), stageTitleBounds.reduced (4, 0), juce::Justification::centredLeft, 1);
    g.setColour (ui::outline);
    g.fillRect (footerSeparator);
    g.setColour (ui::text);
    if (! targetCurvesBounds.isEmpty())
    {
        auto legend = targetCurvesBounds.reduced (10, 0).withHeight (26);
        g.setColour (workspaceAccent);
        g.setFont (ui::font (12.0f, true));
        g.drawText (ui::tr ("Target curves"), legend.removeFromLeft (116), juce::Justification::centredLeft);
        g.setFont (ui::font (11.0f));
        g.setColour (ui::textDim);
        g.drawText (ui::tr ("Colored line: target    White mark: actual"), legend, juce::Justification::centredLeft);
    }
}

void ChordSlotsEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    const bool song = songPanel.isVisible();
    const bool harmony = ! song && ! grooveView;
    const bool feel = ! song && grooveView;
    lnf.setWorkspaceAccent (song ? ui::songAccent : feel ? ui::feelAccent : ui::harmonyAccent);
    const bool inspecting = harmony && pickerOverlay.isVisible();
    const bool drawer = harmony && loopSummary.getToggleState();
    const bool rollOptions = harmony && rollSettingsButton.getToggleState();
    const bool curves = harmony && ! inspecting;
    stageTitleBounds = {};
    const bool midiPanel = harmony && ! midiFilename.getText().isEmpty();
    const bool midiDetails = midiPanel && midiDetailsButton.getToggleState();

    auto setVisible = [] (bool show, std::initializer_list<juce::Component*> controls)
    {
        for (auto* control : controls)
            control->setVisible (show);
    };

    setVisible (drawer, { &keyBox, &keyLabel, &scaleBox, &keyActionsButton, &barsBox, &barsLabel,
                          &meterBox, &meterLabel, &slotsBox, &slotsLabel, &snapBox, &snapLabel });
    setVisible (harmony, { &loopSummary, &midiViewBtn, &rollButton, &rollSettingsButton,
                           &startMelodyButton, &analyzeMidiBtn });
    setVisible (midiPanel, { &midiFilename, &melodyAuditionButton, &melodyTimingButton, &midiDetailsButton });
    setVisible (midiDetails, { &midiKeyDetails, &midiTempoDetails, &autoMelodyKeyButton });
    setVisible (curves, { &lanes, &shapeBox, &resetCurvesBtn, &gestures });
    setVisible (curves && midiPanel, { &melodyFit, &melodyFitLabel });
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
    const auto comping = processor.getPerformanceParams();
    const auto compingStyle = comping.compStyle;
    const bool showArp = feel && cs::isArpeggio (compingStyle);
    const bool showPulse = feel && cs::isPulse (compingStyle);
    const bool showPatternAccent = feel && (showPulse ? cs::pulseRateFor (comping) != 24 : cs::hasCompPatternAccents (compingStyle));
    setVisible (showArp, { &arpDirectionBox, &arpDirectionLabel, &arpRateBox, &arpRateLabel, &arpLength, &arpLengthLabel, &arpOctavesBox, &arpOctavesLabel,
                          &arpAuditionButton, &arpSaveButton, &dragArpTile });
    setVisible (showPulse, { &pulseRateBox, &pulseRateLabel });
    setVisible (showPatternAccent, { &compPatternAccent, &compPatternAccentLabel });
    gateLabel.setText (showArp ? ui::tr ("BASS GATE") : ui::tr ("GATE"), juce::dontSendNotification);
    setVisible (! song, { &timeline, &splitMidiButton });


    songButton.setToggleState (song, juce::dontSendNotification);
    harmonyViewButton.setToggleState (harmony, juce::dontSendNotification);
    grooveViewButton.setToggleState (feel, juce::dontSendNotification);

    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int width)
    {
        c.setBounds (row.removeFromLeft (width));
        row.removeFromLeft (5);
    };
    workspaceTabBounds = area.removeFromTop (42);
    auto header = workspaceTabBounds.reduced (8, 5);
    auto tabs = header.withSizeKeepingCentre (244, header.getHeight());
    place (tabs, harmonyViewButton, 85);
    place (tabs, grooveViewButton, 72);
    place (tabs, songButton, 72);
    playButton.setBounds (header.removeFromRight (110));
    header.removeFromRight (5);
    redoButton.setBounds (header.removeFromRight (32));
    undoButton.setBounds (header.removeFromRight (32));

    area.removeFromTop (7);
    auto context = area.removeFromTop (31);
    if (optionsButton.isVisible())
    {
        optionsButton.setBounds (context.removeFromRight (70));
        context.removeFromRight (5);
    }
    inputButton.setBounds (context.removeFromRight (90));
    context.removeFromRight (5);
    progressionLibraryButton.setBounds (context.removeFromRight (112));
    context.removeFromRight (5);

    const bool useDropdown = processor.getVariationCount() > cs::kNumVariations;
    variationBox.setVisible (useDropdown);
    if (useDropdown)
        place (context, variationBox, 116);
    for (auto& button : varButtons)
    {
        button.setVisible (! useDropdown);
        if (! useDropdown)
            place (context, button, 29);
    }
    place (context, addVariationButton, 29);
    place (context, followButton, 62);
    place (context, copyVarButton, 68);
    if (standaloneTempo.isVisible())
    {
        place (context, standaloneTempo, 118);
        place (context, autoMidiTempoButton, 43);
    }
    area.removeFromTop (9);

    footerBounds = area.removeFromBottom (45);
    auto footer = footerBounds.reduced (8, 7);
    auto exports = footer.removeFromRight (song ? 217 : 292);
    footerSeparator = {};
    if (! song)
    {
        place (footer, fitLabel, 85);
        auto feelAction = footer.removeFromLeft (109);
        if (feel)
            refeelButton.setBounds (feelAction.withWidth (104));
        auto separator = footer.removeFromLeft (15);
        footerSeparator = separator.withSizeKeepingCentre (1, 21);
    }
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
    railBounds = targetCurvesBounds = midiPanelBounds = {};
    if (song)
    {
        songPanel.setBounds (area);
        return;
    }


    if (rollOptions)
    {
        const bool expanded = moreOptions.getToggleState();
        rollSettingsPanel.setSize (area.getWidth(), expanded ? 120 : 92);
        auto settings = rollSettingsPanel.getLocalBounds().reduced (12, 6);
        auto top = settings.removeFromTop (26);
        moreOptions.setBounds (top.removeFromRight (122));
        top.removeFromRight (6);
        playAfterRoll.setBounds (top.removeFromRight (125));
        auto row = settings.removeFromTop (26);
        place (row, moodLabel, 67);
        place (row, moodBox, 130);
        place (row, lockCaption, 110);
        for (auto* button : { &lockRhythm, &lockRoots, &lockColors, &lockVoicing })
            place (row, *button, 75);

        auto sliderField = [&place] (juce::Rectangle<int>& line, juce::Component& label, juce::Component& control, int width)
        {
            place (line, label, 85);
            place (line, control, width);
        };
        row = settings.removeFromTop (26);
        sliderField (row, adventureLabel, adventure, 107);
        sliderField (row, varietyLabel, variety, 107);
        sliderField (row, syncopationLabel, syncopation, 107);
        if (expanded)
        {
            row = settings.removeFromTop (26);
            sliderField (row, bassMotionLabel, bassMotion, 105);
            place (row, endingLabel, 53);
            place (row, endingBox, 96);
            place (row, styleLabel, 122);
            place (row, styleBox, 96);
            place (row, voicingLabel, 68);
            place (row, voicingBox, 96);
        }
    }
    if (feel)
    {
        const int extraRows = (showArp ? 2 : 0) + (showPulse || showPatternAccent ? 1 : 0);
        railBounds = area.removeFromBottom (104 + 28 * extraRows);
        area.removeFromBottom (8);
        timeline.setBounds (area);

        auto strip = railBounds.reduced (12, 6).withTrimmedTop (23);
        auto row = strip.removeFromTop (27);
        auto horizontalField = [&] (juce::Rectangle<int>& line, juce::Component& label,
                                    juce::Component& control, int labelWidth, int width)
        {
            place (line, label, labelWidth);
            place (line, control, width);
        };
        horizontalField (row, compLabel, compBox, 76, 174);
        horizontalField (row, bassPatternLabel, bassPatternBox, 99, 174);
        row = strip.removeFromTop (34);
        int remaining = 4;
        for (auto pair : { std::pair { &strumLabel, &strum }, { &accentLabel, &accent },
                           { &humanizeLabel, &humanize }, { &gateLabel, &gate } })
        {
            auto cell = row.removeFromLeft (row.getWidth() / remaining--).withTrimmedRight (10);
            pair.first->setBounds (cell.removeFromTop (12));
            pair.second->setBounds (cell);
        }
        if (showArp)
        {
            row = strip.removeFromTop (28).reduced (0, 2);
            horizontalField (row, arpDirectionLabel, arpDirectionBox, 74, 118);
            horizontalField (row, arpRateLabel, arpRateBox, 52, 75);
            horizontalField (row, arpOctavesLabel, arpOctavesBox, 72, 63);
            horizontalField (row, arpLengthLabel, arpLength, 75, 110);
            row = strip.removeFromTop (28).reduced (0, 2);
            place (row, arpAuditionButton, 102);
            place (row, arpSaveButton, 101);
            place (row, dragArpTile, 112);
        }
        if (showPulse || showPatternAccent)
        {
            row = strip.removeFromTop (28).reduced (0, 2);
            if (showPulse)
                horizontalField (row, pulseRateLabel, pulseRateBox, 84, 104);
            if (showPatternAccent)
                horizontalField (row, compPatternAccentLabel, compPatternAccent, 110, 145);
        }
        return;
    }

    auto stageHeader = area.removeFromTop (30);
    midiViewBtn.setBounds (stageHeader.removeFromRight (70));
    stageHeader.removeFromRight (6);
    analyzeMidiBtn.setBounds (stageHeader.removeFromRight (92));
    stageHeader.removeFromRight (6);
    stageTitleBounds = stageHeader.removeFromLeft (158);
    loopSummary.setBounds (stageHeader.removeFromLeft (juce::jmin (365, stageHeader.getWidth())));
    auto actions = area.removeFromBottom (34);
    place (actions, rollButton, 145);
    place (actions, rollSettingsButton, 30);
    place (actions, startMelodyButton, 145);
    if (curves)
    {
        place (actions, shapeBox, 134);
        place (actions, resetCurvesBtn, 68);
        gestures.setBounds (actions.removeFromRight (92));
    }
    area.removeFromBottom (7);
    if (drawer)
    {
        auto settings = area.removeFromTop (42).withTrimmedTop (3);
        auto key = settings.removeFromLeft (362);
        keyLabel.setBounds (key.removeFromTop (13));
        place (key, keyBox, 54);
        place (key, scaleBox, 195);
        place (key, keyActionsButton, 92);
        auto setting = [&settings] (juce::Label& label, juce::ComboBox& box, int width)
        {
            auto cell = settings.removeFromLeft (width).withTrimmedRight (4);
            label.setBounds (cell.removeFromTop (13));
            box.setBounds (cell);
        };
        setting (barsLabel, barsBox, 59);
        setting (meterLabel, meterBox, 63);
        setting (slotsLabel, slotsBox, 55);
        setting (snapLabel, snapBox, 62);

    }
    area.removeFromTop (8);
    if (midiPanel)
    {
        midiPanelBounds = area.removeFromTop (midiDetails ? 63 : 34);
        auto panel = midiPanelBounds.reduced (7, 4);
        auto row = panel.removeFromTop (26);
        midiDetailsButton.setButtonText (midiDetails ? ui::tr ("Hide details") : ui::tr ("Details"));
        midiDetailsButton.setBounds (row.removeFromRight (87));
        row.removeFromRight (5);
        melodyTimingButton.setBounds (row.removeFromRight (121));
        row.removeFromRight (5);
        melodyAuditionButton.setBounds (row.removeFromRight (108));
        midiFilename.setBounds (row);
        if (midiDetails)
        {
            panel.removeFromTop (3);
            autoMelodyKeyButton.setBounds (panel.removeFromRight (139));
            midiKeyDetails.setBounds (panel.removeFromLeft (panel.getWidth() * 3 / 5));
            midiTempoDetails.setBounds (panel);
        }
        area.removeFromTop (6);
    }
    if (rollOptions)
    {
        const int panelHeight = rollSettingsPanel.getHeight();
        if (area.getHeight() >= panelHeight + (inspecting ? 252 : 225))
        {
            rollSettingsPanel.setBounds (area.removeFromBottom (panelHeight));
            area.removeFromBottom (7);
        }
        else
        {
            // Keep the inspector and the melody/loop details available in compact layouts.
            rollSettingsPanel.setTopLeftPosition (rollButton.getX(), actions.getY() - panelHeight - 5);
        }
    }
    const int timelineHeight = midiViewBtn.getToggleState() ? juce::jlimit (94, 210, area.getHeight() - 158)
                                                         : juce::jlimit (82, 136, area.getHeight() - 146);
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
    if (gestures.getToggleState())
        gestureHelp.setBounds (area.removeFromBottom (21).reduced (8, 0));
    if (midiPanel)
    {
        auto tools = area.removeFromBottom (25).reduced (8, 0);
        place (tools, melodyFitLabel, 86);
        place (tools, melodyFit, 138);
    }
    area.removeFromBottom (4);
    lanes.setBounds (midiViewBtn.getToggleState() ? area.withTrimmedLeft (44) : area);
}
