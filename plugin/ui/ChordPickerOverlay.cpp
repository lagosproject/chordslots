// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ChordPickerOverlay.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
constexpr std::array<int, 3> compactMetricWidths { 38, 39, 42 };
constexpr std::array<int, 3> wideMetricWidths { 62, 76, 78 };
constexpr std::array<const char*, 3> metricNames { "Tension", "Brightness", "Curve Match" };
constexpr int replaceWidth = 57;
constexpr int auditionWidth = 40;
constexpr int rowHeight = 30;
constexpr auto matchDescription = "Match measures how closely the whole progression follows its target tension and brightness curves.";
constexpr auto melodyMatchDescription = "Melody Match weights the notes sounding in this slot by overlap duration and strong beats. Melody Fit controls how much this affects chord selection.";

std::array<juce::Rectangle<int>, 3> metricCells (juce::Rectangle<int>& area, juce::Rectangle<int>* melody)
{
    std::array<juce::Rectangle<int>, 3> cells;
    const auto& widths = area.getWidth() >= 450 ? wideMetricWidths : compactMetricWidths;
    area.removeFromRight (replaceWidth + auditionWidth);
    if (melody != nullptr)
        *melody = area.removeFromRight (140);
    for (int i = 2; i >= 0; --i)
        cells[(size_t) i] = area.removeFromRight (widths[(size_t) i]);
    return cells;
}

void useReplacement (cs::Progression& p, int slot, const cs::Slot& replacement)
{
    auto& current = p.slots[(size_t) slot];
    current.chord = replacement.chord;
    current.voicing = replacement.voicing;
    cs::updateActuals (p, slot);
}
}

ChordPickerOverlay::AuditionButton::AuditionButton (const juce::String& label, std::array<float, 3> changes,
                                                  juce::String notes, const juce::String& scaleDetails,
                                                  int melodyScore, juce::String clashNotes, bool showMelodyColumn,
                                                  std::array<float, 2> previous, std::array<float, 2> goals)
    : juce::TextButton (label), deltas (changes), original (previous), targets (goals), outsideNotes (std::move (notes)),
      melodyPercent (melodyScore), clashes (std::move (clashNotes)), showMelody (showMelodyColumn)
{
    setTooltip (label + scaleDetails + ui::tr ("\nClick to hear this chord. Hold to hear it with the progression and any imported melody."));
    replace.setTooltip (ui::tr ("Replace the current chord with ") + label + ".");
    addAndMakeVisible (replace);
    addAndMakeVisible (audition);
    audition.getProperties().set ("icon", (int) ui::ButtonIcon::Audition);
    audition.setTitle (ui::tr ("Hear this chord with the progression and any imported melody."));
    audition.setTooltip (ui::tr ("Hear this chord with the progression and any imported melody."));
    audition.onClick = [this] { if (onAudition) onAudition (true); };
    onClick = [this]
    {
        if (! fired && ! cancelled && onAudition)
            onAudition (false);
    };
}

void ChordPickerOverlay::AuditionButton::setMatchReasons (bool curve, bool melody)
{
    bestCurve = curve;
    bestMelody = melody;
    juce::StringArray reasons;
    if (curve) reasons.add (ui::tr ("Best curve fit"));
    if (melody) reasons.add (ui::tr ("Best melody fit"));
    if (! reasons.isEmpty())
    {
        setButtonText (getButtonText() + "  |  " + reasons.joinIntoString (" / "));
        setTooltip (getTooltip() + "\n" + reasons.joinIntoString (" / "));
    }
}

void ChordPickerOverlay::AuditionButton::resized()
{
    auto area = getLocalBounds();
    replace.setBounds (area.removeFromRight (replaceWidth).reduced (5, 3));
    audition.setBounds (area.removeFromRight (auditionWidth).reduced (3, 3));
    nameBounds = getLocalBounds();
    cells = metricCells (nameBounds, showMelody ? &melodyBounds : nullptr);
    nameBounds = nameBounds.reduced (9, 0);
}

void ChordPickerOverlay::AuditionButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    g.setColour (down ? ui::panel.brighter (0.24f) : over ? ui::panel.brighter (0.12f) : ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f, 1.0f), 4.0f);
    g.setColour (hasKeyboardFocus (false) ? ui::textDim : ui::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f, 1.0f), 4.0f, 1.0f);
    g.setColour (ui::text);
    const auto nameFont = ui::font (13.0f, true);
    g.setFont (nameFont);
    g.drawFittedText (getButtonText(), nameBounds, juce::Justification::centredLeft, 1);
    if (outsideNotes.isNotEmpty())
    {
        const auto noteFont = ui::font (11.0f);
        const auto noteBounds = nameBounds.withTrimmedLeft (
            juce::GlyphArrangement::getStringWidthInt (nameFont, getButtonText()) + 14);
        if (noteBounds.getWidth() >= juce::GlyphArrangement::getStringWidthInt (noteFont, outsideNotes))
        {
            g.setFont (noteFont);
            g.setColour (ui::textDim);
            g.drawText (outsideNotes, noteBounds, juce::Justification::centredLeft);
        }
    }
    for (size_t i = 0; i < cells.size(); ++i)
    {
        const bool isMatch = i == cells.size() - 1;
        g.setFont (isMatch ? nameFont : ui::font (nameFont.getHeight()));
        g.setColour (isMatch && bestCurve ? ui::accent : isMatch ? ui::text : ui::textDim);
        const int points = juce::roundToInt (100.0f * deltas[i]);
        g.drawText ((points > 0 ? "+" : "") + juce::String (points), cells[i], juce::Justification::centred);
    }
    if (showMelody)
    {
        auto melodyLabel = melodyPercent >= 0 ? juce::String (melodyPercent) + "%" : juce::String::fromUTF8 ("—");
        if (clashes.isNotEmpty())
            melodyLabel += ui::tr ("  Clash: ") + clashes;
        g.setFont (ui::font (11.5f, clashes.isNotEmpty()));
        g.setColour (clashes.isNotEmpty() ? ui::tension.brighter (0.45f) : ui::text);
        if (bestMelody) g.setColour (ui::accent);
        g.drawFittedText (melodyLabel, melodyBounds.reduced (3, 0), juce::Justification::centred, 1, 0.8f);
    }
}

juce::String ChordPickerOverlay::AuditionButton::getTooltip()
{
    auto tooltip = juce::TextButton::getTooltip();
    const auto pos = getMouseXYRelative();
    if (showMelody && melodyBounds.contains (pos))
    {
        if (melodyPercent < 0)
            return ui::tr ("No imported melody notes sound in this slot.");
        auto description = juce::String (melodyPercent) + ui::tr ("% Melody Match. ") + ui::tr (melodyMatchDescription);
        if (clashes.isNotEmpty())
            description += ui::tr ("\nStrongest clash pitches in the imported melody: ") + clashes;
        return description;
    }
    for (size_t i = 0; i < cells.size(); ++i)
    {
        if (! cells[i].contains (pos))
            continue;
        const float points = 100.0f * deltas[i];
        tooltip = ui::tr (metricNames[i]) + ui::tr (" change: ") + (points > 0.0f ? "+" : "")
                + juce::String (points, 3) + ui::tr (" points (0-100 scale)");
        if (i == 2)
            tooltip += "\n" + ui::tr (matchDescription);
        else
        {
            const auto display = [] (float value) { return juce::String (juce::roundToInt (100.0f * value)); };
            tooltip += "\nCurrent " + display (original[i]) + ui::tr (", alternative ") + display (original[i] + deltas[i])
                    + ui::tr (", target ") + display (targets[i]) + ".";
        }
        break;
    }
    return tooltip;
}

void ChordPickerOverlay::AuditionButton::mouseDown (const juce::MouseEvent& e)
{
    fired = false;
    cancelled = false;
    juce::TextButton::mouseDown (e);
    pressed = e.mods.isLeftButtonDown() && isDown();
    if (pressed)
        startTimer (450);
}

void ChordPickerOverlay::AuditionButton::mouseUp (const juce::MouseEvent& e)
{
    stopTimer();
    if (fired && ! cancelled && onRelease)
        onRelease();
    pressed = false;
    juce::TextButton::mouseUp (e);
    fired = false;
    cancelled = false;
}

void ChordPickerOverlay::AuditionButton::mouseDrag (const juce::MouseEvent& e)
{
    juce::TextButton::mouseDrag (e);
    if (pressed && (! getLocalBounds().contains (e.getPosition()) || replace.getBounds().contains (e.getPosition())))
        cancelGesture();
}

void ChordPickerOverlay::AuditionButton::cancelGesture()
{
    stopTimer();
    if (fired && ! cancelled && onRelease)
        onRelease();
    cancelled = cancelled || pressed;
    pressed = false;
}

void ChordPickerOverlay::AuditionButton::timerCallback()
{
    if (! pressed || ! isDown() || ! juce::ModifierKeys::getCurrentModifiersRealtime().isLeftButtonDown() || ! isShowing())
    {
        cancelGesture();
        return;
    }
    if (! fired && onAudition)
    {
        fired = true;
        onAudition (true);
        startTimer (40);
    }
}

ChordPickerOverlay::ChordPickerOverlay (ChordSlotsProcessor& proc)
    : processor (proc)
{
    setName (ui::tr ("Selected-slot inspector"));

    closeBtn.getProperties().set ("icon", (int) ui::ButtonIcon::Close);
    closeBtn.setTitle (ui::tr ("Close the selected-slot inspector."));
    closeBtn.setTooltip (ui::tr ("Close the selected-slot inspector."));

    closeBtn.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (closeBtn);
    addAndMakeVisible (substitutionsTab);
    addAndMakeVisible (inspectorTab);
    addChildComponent (invertUpBtn);
    addChildComponent (invertDownBtn);
    invertUpBtn.setTooltip (ui::tr ("Raise the lowest chord voice one octave. The separate bass stays in place."));
    invertDownBtn.setTooltip (ui::tr ("Lower the highest chord voice one octave. The separate bass stays in place."));
    invertUpBtn.onClick = [this] { invertChord (cs::InversionDirection::Up); };
    invertDownBtn.onClick = [this] { invertChord (cs::InversionDirection::Down); };
    addAndMakeVisible (showOutside);
    showOutside.onClick = [this]
    {
        rebuildCandidates();
        resized();
        repaint();
    };
    alternativesViewport.setViewedComponent (&alternativesContent, false);
    alternativesViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (alternativesViewport);
    outsideHeading.setText (ui::tr ("Outside scale"), juce::dontSendNotification);
    emptyGroup.setText (ui::tr ("No in-scale alternatives"), juce::dontSendNotification);
    for (auto* label : { &outsideHeading, &emptyGroup })
    {
        label->setFont (ui::font (12.0f, true));
        label->setColour (juce::Label::textColourId, ui::textDim);
        alternativesContent.addChildComponent (*label);
    }
    substitutionsTab.onClick = [this] { setSubstitutionsVisible (true); };
    inspectorTab.onClick = [this] { setSubstitutionsVisible (false); };

    titleLabel.setFont (ui::font (13.0f, true));
    titleLabel.setColour (juce::Label::textColourId, ui::text);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // Create 12 root buttons
    const char* pcNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
    {
        rootButtons[(size_t) i] = std::make_unique<juce::TextButton> (ui::noteSpelling (pcNames[i]));
        auto* b = rootButtons[(size_t) i].get();
        b->setClickingTogglesState (false);
        b->setTooltip (ui::tr ("Set the root note of the chord in this slot."));
        b->onClick = [this, i] { selectRoot (i); };
        addAndMakeVisible (b);
    }

    const auto& types = cs::chordTypes();
    for (int t = 0; t < (int) types.size(); ++t)
    {
        const auto suffix = utf8 (types[(size_t) t].suffix);
        qualityBox.addItem (suffix.isEmpty() ? ui::tr ("Major") : suffix, t + 1);
    }
    qualityBox.setName (ui::tr ("Chord quality"));
    qualityBox.setTooltip (ui::tr ("Change the selected chord's quality. Each edit can be undone."));
    qualityBox.onChange = [this] { if (qualityBox.getSelectedId() > 0) selectQuality (qualityBox.getSelectedId() - 1); };
    addAndMakeVisible (qualityBox);
    chordLock.getProperties().set ("icon", (int) ui::ButtonIcon::Lock);
    chordLock.setClickingTogglesState (true);
    chordLock.setTooltip (ui::tr ("Chord lock: preserve this slot's chord when rolling. Does not protect timing boundaries. Manual edits still work."));
    chordLock.onClick = [this]
    {
        processor.edit ([slot = currentSlot] (cs::Progression& p, cs::Rng&)
        {
            if (slot >= 0 && slot < p.numSlots) p.slots[(size_t) slot].locked = ! p.slots[(size_t) slot].locked;
        });
        setSlot (currentSlot);
    };
    rollRoot.setTooltip (ui::tr ("Reroll only the root of the selected slot."));
    rollColor.setTooltip (ui::tr ("Reroll only the quality of the selected slot."));
    changeBass.setTooltip (ui::tr ("Cycle the selected slot's separate bass note."));
    rollRoot.onClick = [this] { processor.edit ([slot = currentSlot] (cs::Progression& p, cs::Rng& rng) { cs::rollSlotRoot (p, slot, rng); }); setSlot (currentSlot); };
    rollColor.onClick = [this] { processor.edit ([slot = currentSlot] (cs::Progression& p, cs::Rng& rng) { cs::rollSlotColor (p, slot, rng); }); setSlot (currentSlot); };
    changeBass.onClick = [this] { processor.edit ([slot = currentSlot] (cs::Progression& p, cs::Rng&) { cs::rollSlotBass (p, slot); }); setSlot (currentSlot); };
    lockScope.setText (ui::tr ("Chord lock: this chord on Roll; separator lock: timing"), juce::dontSendNotification);
    lockScope.setFont (ui::font (11.0f));
    lockScope.setColour (juce::Label::textColourId, ui::textDim);
    lockScope.setTooltip (ui::tr ("Chord lock: this slot. Separator lock: a timing boundary. Generation locks: the whole progression."));
    for (juce::Component* control : std::initializer_list<juce::Component*> { &chordLock, &rollRoot, &rollColor, &changeBass, &lockScope })
        addAndMakeVisible (control);
}

ChordPickerOverlay::~ChordPickerOverlay()
{
    cancelGestures();
}

void ChordPickerOverlay::cancelGestures()
{
    for (auto& candidate : candidateButtons)
        if (auto* row = dynamic_cast<AuditionButton*> (candidate.btn.get()))
            row->cancelGesture();
}

void ChordPickerOverlay::visibilityChanged()
{
    if (! isShowing())
    {
        cancelGestures();
        processor.stopAlternativeAudition();
    }
}

void ChordPickerOverlay::open (int slotIndex, bool showAlternatives)
{
    cancelGestures();
    processor.stopAlternativeAudition();
    alternativesMode = showAlternatives;
    alternativesViewport.setViewPosition (0, 0);
    setSlot (slotIndex);
}

void ChordPickerOverlay::setSlot (int slotIndex)
{
    if (currentSlot != slotIndex || contextVariation != processor.getEditVariation())
    {
        processor.stopAlternativeAudition();
        alternativesViewport.setViewPosition (0, 0);
    }
    currentSlot = slotIndex;
    contextVariation = processor.getEditVariation();
    prog = processor.getProgression();
    const bool validSlot = currentSlot >= 0 && currentSlot < prog.numSlots;
    const auto* voicing = validSlot ? &prog.slots[(size_t) currentSlot].voicing : nullptr;
    invertUpBtn.setEnabled (voicing != nullptr && cs::canInvertChord (*voicing, cs::InversionDirection::Up));
    invertDownBtn.setEnabled (voicing != nullptr && cs::canInvertChord (*voicing, cs::InversionDirection::Down));

    if (validSlot)
    {
        activeChord = prog.slots[(size_t) currentSlot].chord;
        const juce::String chordName = ui::chordName (activeChord, prog.tonic, prog.scale);
        const juce::String roman = utf8 (cs::romanNumeral (activeChord, prog.tonic, prog.scale));
        titleLabel.setText (ui::tr ("Chord ") + juce::String (currentSlot + 1) + ": " + chordName + " (" + roman + ")",
                            juce::dontSendNotification);

        // Update root button highlighting
        const auto safeNotes = cs::safeMelodyNotes (activeChord, prog.tonic, prog.scale);
        for (int i = 0; i < 12; ++i)
        {
            auto& button = *rootButtons[(size_t) i];
            const bool safe = std::binary_search (safeNotes.begin(), safeNotes.end(), i);
            button.getProperties().set ("safeNote", safe);
            button.setTooltip (safe ? ui::tr ("Set chord root. This pitch is a safe melody note for the current chord.")
                                    : ui::tr ("Set chord root."));
            button.setToggleState (i == activeChord.root, juce::dontSendNotification);
            button.repaint();
        }

        qualityBox.setSelectedId (activeChord.type + 1, juce::dontSendNotification);
        chordLock.setToggleState (prog.slots[(size_t) currentSlot].locked, juce::dontSendNotification);

        // Rebuild the separate bass choices.
        bassButtons.clear();
        const auto& t = cs::chordTypes()[(size_t) activeChord.type];
        std::vector<int> bassOptions;
        bassOptions.push_back (0); // Root position
        for (int iv : t.iv)
        {
            const int ivNorm = cs::mod12 (iv);
            if (ivNorm != 0 && std::find (bassOptions.begin(), bassOptions.end(), ivNorm) == bassOptions.end())
                bassOptions.push_back (ivNorm);
        }

        for (int ivNorm : bassOptions)
        {
            const auto name = ui::pitchClassName (cs::mod12 (activeChord.root + ivNorm), prog.tonic, prog.scale);
            auto b = std::make_unique<juce::TextButton> (name);
            b->setTooltip (ui::tr ("Set the note in the separate bass part."));
            b->setToggleState (ivNorm == activeChord.bassInt, juce::dontSendNotification);
            b->onClick = [this, ivNorm] { selectBass (ivNorm); };
            addAndMakeVisible (b.get());
            bassButtons.push_back (std::move (b));
        }

    }

    if (! validSlot)
    {
        cancelGestures();
        processor.stopAlternativeAudition();
        setVisible (false);
        return;
    }
    rebuildCandidates();
    resized();
    repaint();
}

void ChordPickerOverlay::rebuildCandidates()
{
    cancelGestures();
    candidateButtons.clear();
    currentMelodyPercent = -1;
    if (currentSlot < 0 || currentSlot >= prog.numSlots)
        return;

    const float currentFit = cs::fitScore (prog);
    const auto& current = prog.slots[(size_t) currentSlot];
    const auto melody = cs::slotMelodyProfile (prog, currentSlot, true);
    const bool melodySounds = std::any_of (melody.begin(), melody.end(), [] (float duration) { return duration > 0.0f; });
    if (melodySounds)
        currentMelodyPercent = juce::roundToInt (100.0f * cs::melodyMatch (current.chord, melody));
    const auto rng = processor.copyVoicingRng();
    AuditionButton* curveChoice = nullptr;
    AuditionButton* melodyChoice = nullptr;
    float bestCurveFit = -1.0f;
    float bestMelodyFit = -1.0f;

    auto addCandidate = [&] (const cs::Slot& replacement, float fitDelta, cs::ScaleMembership scale)
    {
        auto label = ui::chordName (replacement.chord, prog.tonic, prog.scale);

        label += "   " + utf8 (cs::romanNumeral (replacement.chord, prog.tonic, prog.scale));
        juce::String details, outsideNotes;
        if (scale.outsideChord != 0)
        {
            juce::StringArray notes;
            for (int pc = 0; pc < 12; ++pc)
                if ((scale.outsideChord & (1u << pc)) != 0)
                    notes.add (ui::pitchClassName (pc, prog.tonic, prog.scale));
            const auto pitches = notes.joinIntoString (", ");
            outsideNotes = ui::tr ("Outside: ") + pitches;
            details = ui::tr ("\nChord notes outside scale: ") + pitches;
        }
        if (cs::scaleMembership (replacement.chord, prog.tonic, prog.scale).outsideBass)
            details += ui::tr ("\nSeparate bass outside scale: ")
                     + ui::pitchClassName (replacement.chord.bassPc(), prog.tonic, prog.scale);

        const auto clashMask = cs::melodyClashMask (replacement.chord, melody);
        juce::StringArray clashNotes;
        for (int pc = 0; pc < 12; ++pc)
            if ((clashMask & (1u << pc)) != 0)
                clashNotes.add (ui::pitchClassName (pc, prog.tonic, prog.scale));
        const float melodyFit = melodySounds ? cs::melodyMatch (replacement.chord, melody) : -1.0f;
        const int melodyPercent = melodySounds ? juce::roundToInt (100.0f * melodyFit) : -1;
        auto row = std::make_unique<AuditionButton> (label,
            std::array<float, 3> { replacement.tension - current.tension, replacement.brightness - current.brightness, fitDelta },
            std::move (outsideNotes), details, melodyPercent, clashNotes.joinIntoString (", "), ! prog.melody.empty(),
            std::array<float, 2> { current.tension, current.brightness }, std::array<float, 2> { current.targetT, current.targetB });
        const int slot = currentSlot;
        const int variation = contextVariation;
        row->onAudition = [this, slot, variation, replacement] (bool inContext)
        {
            if (processor.getEditVariation() != variation) return;
            auto preview = prog;
            useReplacement (preview, slot, replacement);
            processor.auditionAlternative (preview, slot, inContext);
        };
        row->onRelease = [this] { processor.stopAlternativeAudition(); };
        row->replace.onClick = [this, slot, variation, replacement]
        {
            if (processor.getEditVariation() != variation) return;
            cancelGestures();
            processor.edit ([slot, replacement] (cs::Progression& p, cs::Rng&)
            {
                if (slot >= 0 && slot < p.numSlots)
                    useReplacement (p, slot, replacement);
            });
            processor.auditionSlot (slot);
            setSlot (slot);
        };
        const float curveFit = cs::slotFit (replacement);
        if (curveFit > bestCurveFit)
        {
            bestCurveFit = curveFit;
            curveChoice = row.get();
        }
        if (prog.melodyFit > 0.0f && melodyFit > bestMelodyFit)
        {
            bestMelodyFit = melodyFit;
            melodyChoice = row.get();
        }
        alternativesContent.addAndMakeVisible (*row);
        candidateButtons.push_back ({ std::move (row), scale.outsideChord != 0 });
    };

    for (const auto& entry : cs::inspectAlternatives (prog, currentSlot, 6, showOutside.getToggleState() ? 3 : 0, rng))
    {
        auto test = prog;
        auto replacement = current;
        replacement.chord = entry.chord;
        replacement.voicing = entry.voicing;
        useReplacement (test, currentSlot, replacement);
        addCandidate (test.slots[(size_t) currentSlot], cs::fitScore (test) - currentFit,
                      cs::scaleMembership (entry.chord, prog.tonic, prog.scale));
    }
    if (curveChoice != nullptr)
        curveChoice->setMatchReasons (true, curveChoice == melodyChoice);
    if (melodyChoice != nullptr && melodyChoice != curveChoice)
        melodyChoice->setMatchReasons (false, true);
}

void ChordPickerOverlay::layoutAlternatives (juce::Rectangle<int> area)
{
    auto filterRow = area.removeFromTop (25);
    showOutside.setBounds (filterRow.removeFromRight (186));
    targetSummaryBounds = filterRow.reduced (3, 0);
    candidateArea = area.removeFromTop (18);
    alternativesViewport.setBounds (area);
    const int count = (int) candidateButtons.size();
    const bool noDiatonic = count == 0 || candidateButtons.front().outside;
    const bool hasOutside = count > 0 && candidateButtons.back().outside;
    const int height = rowHeight * count + (noDiatonic ? rowHeight : 0) + (hasOutside ? 22 : 0);
    const int width = area.getWidth() - alternativesViewport.getScrollBarThickness();
    alternativesContent.setSize (width, height);
    auto rows = alternativesContent.getLocalBounds();
    emptyGroup.setVisible (noDiatonic);
    emptyGroup.setBounds (noDiatonic ? rows.removeFromTop (rowHeight) : juce::Rectangle<int>());
    outsideHeading.setVisible (hasOutside);
    bool startedOutside = false;
    for (auto& candidate : candidateButtons)
    {
        if (candidate.outside && ! startedOutside)
        {
            outsideHeading.setBounds (rows.removeFromTop (22));
            startedOutside = true;
        }
        candidate.btn->setBounds (rows.removeFromTop (rowHeight));
    }
}

void ChordPickerOverlay::selectRoot (int rootPc)
{
    activeChord.root = rootPc;
    applyChord (activeChord);
}

void ChordPickerOverlay::selectQuality (int typeIdx)
{
    activeChord.type = typeIdx;
    applyChord (activeChord);
}

void ChordPickerOverlay::selectBass (int bassInt)
{
    processor.edit ([slot = currentSlot, bassInt] (cs::Progression& p, cs::Rng&) { cs::setBass (p, slot, bassInt); });
    processor.auditionSlot (currentSlot);
    setSlot (currentSlot);
}

void ChordPickerOverlay::invertChord (cs::InversionDirection direction)
{
    processor.edit ([slot = currentSlot, direction] (cs::Progression& p, cs::Rng&)
    {
        cs::invertChord (p, slot, direction);
    });
    processor.auditionSlot (currentSlot);
    setSlot (currentSlot);
}

void ChordPickerOverlay::applyChord (cs::Chord c)
{
    if (currentSlot < 0 || currentSlot >= prog.numSlots)
        return;

    activeChord = c;
    processor.edit ([slot = currentSlot, c] (cs::Progression& p, cs::Rng& rng)
    {
        cs::setChord (p, slot, c, rng);
    });
    processor.auditionSlot (currentSlot);
    setSlot (currentSlot);
}

void ChordPickerOverlay::setSubstitutionsVisible (bool show)
{
    if (alternativesMode == show) return;
    cancelGestures();
    processor.stopAlternativeAudition();
    alternativesMode = show;
    resized();
    repaint();
}

void ChordPickerOverlay::paint (juce::Graphics& g)
{
    auto panel = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (panel, 7.0f);
    g.setColour (ui::outline);
    g.drawRoundedRectangle (panel, 7.0f, 1.0f);
    g.fillRect (tabDivider);

    g.setFont (ui::font (11.5f));
    g.setColour (ui::textDim);
    if (alternativesMode)
    {
        if (currentSlot >= 0 && currentSlot < prog.numSlots)
        {
            const auto& slot = prog.slots[(size_t) currentSlot];
            const auto pair = [] (float target, float actual)
            {
                return juce::String (juce::roundToInt (100.0f * target)) + juce::String::fromUTF8 (" → ")
                    + juce::String (juce::roundToInt (100.0f * actual));
            };
            auto summary = ui::tr (u8"Target \u2192 actual    Brightness ") + pair (slot.targetB, slot.brightness)
                + ui::tr ("    Tension ") + pair (slot.targetT, slot.tension)
                + ui::tr ("    Energy ") + pair (slot.targetE, slot.energy);
            if (currentMelodyPercent >= 0)
                summary += ui::tr ("    Current Melody Match ") + juce::String (currentMelodyPercent) + "%";
            g.drawFittedText (summary, targetSummaryBounds, juce::Justification::centredLeft, 1);
        }

        auto header = candidateArea.withTrimmedRight (alternativesViewport.getScrollBarThickness());
        juce::Rectangle<int> melodyHeader;
        const auto cells = metricCells (header, prog.melody.empty() ? nullptr : &melodyHeader);
        g.setFont (ui::font (11.0f, true));
        g.drawText (ui::tr ("Chord"), header.reduced (9, 0), juce::Justification::centredLeft);
        for (size_t i = 0; i < cells.size(); ++i)
            g.drawText (getWidth() >= 480 ? ui::tr (metricNames[i]) : i == 0 ? ui::tr ("Tens.") : i == 1 ? ui::tr ("Bright") : ui::tr ("Match"),
                        cells[i], juce::Justification::centred);
        if (! melodyHeader.isEmpty())
            g.drawText (ui::tr ("Melody Match"), melodyHeader, juce::Justification::centred);
        return;
    }

    g.setColour (ui::outline);
    g.drawHorizontalLine (rootArea.getY() - 16, static_cast<float> (rootArea.getX()),
                          static_cast<float> (getWidth() - rootArea.getX()));
    g.setColour (ui::textDim);
    for (const auto& [bounds, label] : { std::pair { rootArea, "Root pitch" }, std::pair { qualityArea, "Quality" },
                                        std::pair { voicingArea, "Inversion (upper voices)" },
                                        std::pair { bassArea, "Bass (separate part)" } })
        g.drawFittedText (ui::tr (label), bounds.withY (bounds.getY() - 16).withHeight (15), juce::Justification::centredLeft, 1, 0.70f);
}

void ChordPickerOverlay::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto tabs = area.removeFromTop (30);
    substitutionsTab.setBounds (tabs.removeFromLeft (119));
    tabs.removeFromLeft (4);
    inspectorTab.setBounds (tabs.removeFromLeft (88));
    closeBtn.setBounds (tabs.removeFromRight (36));
    titleLabel.setBounds (tabs.reduced (8, 0));
    titleLabel.setTooltip (titleLabel.getText());
    tabDivider = area.removeFromTop (4).withTrimmedTop (3);
    area.removeFromTop (6);

    substitutionsTab.setToggleState (alternativesMode, juce::dontSendNotification);
    inspectorTab.setToggleState (! alternativesMode, juce::dontSendNotification);

    const bool editing = ! alternativesMode;
    for (auto* control : std::initializer_list<juce::Component*> {
             &chordLock, &rollRoot, &rollColor, &changeBass, &qualityBox, &invertUpBtn, &invertDownBtn, &lockScope })
        control->setVisible (editing);
    for (auto& button : rootButtons) button->setVisible (editing);
    for (auto& button : bassButtons) button->setVisible (editing);
    showOutside.setVisible (! editing);
    alternativesViewport.setVisible (! editing);

    if (! editing)
    {
        layoutAlternatives (area);
        return;
    }

    auto actions = area.removeFromTop (28);
    chordLock.setBounds (actions.removeFromLeft (105).reduced (2));
    rollRoot.setBounds (actions.removeFromLeft (80).reduced (2));
    rollColor.setBounds (actions.removeFromLeft (80).reduced (2));
    changeBass.setBounds (actions.removeFromLeft (96).reduced (2));
    lockScope.setBounds (actions.withTrimmedLeft (8));

    area.removeFromTop (20);
    rootArea = area.removeFromTop (27);
    for (size_t i = 0; i < rootButtons.size(); ++i)
    {
        const int left = rootArea.getX() + (int) i * rootArea.getWidth() / 12;
        const int right = rootArea.getX() + ((int) i + 1) * rootArea.getWidth() / 12;
        rootButtons[i]->setBounds (left, rootArea.getY(), juce::jmax (0, right - left - 4), rootArea.getHeight());
    }

    area.removeFromTop (22);
    auto voicing = area.removeFromTop (28);
    qualityArea = voicing.removeFromLeft (voicing.getWidth() / 3).withTrimmedRight (12);
    qualityBox.setBounds (qualityArea);
    voicingArea = voicing.removeFromLeft (voicing.getWidth() / 2).withTrimmedRight (12);
    auto inversionButtons = voicingArea;
    invertDownBtn.setBounds (inversionButtons.removeFromLeft (inversionButtons.getWidth() / 2).withTrimmedRight (4));
    invertUpBtn.setBounds (inversionButtons);
    bassArea = voicing;
    const int count = (int) bassButtons.size();
    for (int i = 0; i < count; ++i)
        bassButtons[(size_t) i]->setBounds (bassArea.getX() + i * bassArea.getWidth() / count,
                                          bassArea.getY(), juce::jmax (0, bassArea.getWidth() / count - 4), bassArea.getHeight());
}
