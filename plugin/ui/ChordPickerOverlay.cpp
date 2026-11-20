// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ChordPickerOverlay.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
constexpr std::array<int, 3> metricWidths { 76, 84, 76 };
constexpr std::array<float, 3> thresholds { 0.005f, 0.005f, 0.0005f };
constexpr std::array<const char*, 3> metricNames { "Tension", "Brightness", "Match" };
constexpr int replaceWidth = 78;
constexpr int rowHeight = 30;
constexpr auto matchDescription = "Match measures how closely the whole progression follows its target tension and brightness curves.";

std::array<juce::Rectangle<int>, 3> metricCells (juce::Rectangle<int>& area)
{
    std::array<juce::Rectangle<int>, 3> cells;
    area.removeFromRight (replaceWidth);
    for (int i = 2; i >= 0; --i)
        cells[(size_t) i] = area.removeFromRight (metricWidths[(size_t) i]);
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
                                                  juce::String notes, const juce::String& scaleDetails)
    : juce::TextButton (label), deltas (changes), outsideNotes (std::move (notes))
{
    setTooltip (label + scaleDetails + "\nClick to hear this chord. Hold to hear it in the progression.");
    replace.setTooltip ("Replace the current chord with " + label + ".");
    addAndMakeVisible (replace);
    onClick = [this]
    {
        if (! fired && ! cancelled && onAudition)
            onAudition (false);
    };
}

void ChordPickerOverlay::AuditionButton::resized()
{
    auto area = getLocalBounds();
    replace.setBounds (area.removeFromRight (replaceWidth).reduced (5, 3));
    nameBounds = getLocalBounds();
    cells = metricCells (nameBounds);
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
    g.setFont (nameFont);
    g.setColour (ui::text);
    for (size_t i = 0; i < cells.size(); ++i)
    {
        const auto symbol = deltas[i] >= thresholds[i] ? "\xe2\x96\xb2"
                          : deltas[i] <= -thresholds[i] ? "\xe2\x96\xbc" : "\xe2\x80\x94";
        g.drawText (juce::String::fromUTF8 (symbol), cells[i], juce::Justification::centred);
    }
}

juce::String ChordPickerOverlay::AuditionButton::getTooltip()
{
    auto tooltip = juce::TextButton::getTooltip();
    const auto pos = getMouseXYRelative();
    for (size_t i = 0; i < cells.size(); ++i)
    {
        if (! cells[i].contains (pos))
            continue;
        const float points = 100.0f * deltas[i];
        tooltip = juce::String (metricNames[i]) + " change: " + (points > 0.0f ? "+" : "")
                + juce::String (points, 3) + " points (0-100 scale)";
        if (i == 2)
            tooltip += "\n" + juce::String (matchDescription);
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
    setAlwaysOnTop (true);

    closeBtn.setTooltip ("Close the chord picker.");
    switchViewBtn.setTooltip ("Switch between suggested alternatives and manual chord editing.");
    closeBtn.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (closeBtn);
    addAndMakeVisible (switchViewBtn);
    addChildComponent (invertUpBtn);
    addChildComponent (invertDownBtn);
    invertUpBtn.setTooltip ("Raise the lowest chord voice one octave. The separate bass stays in place.");
    invertDownBtn.setTooltip ("Lower the highest chord voice one octave. The separate bass stays in place.");
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
    outsideHeading.setText ("Outside scale", juce::dontSendNotification);
    emptyGroup.setText ("No in-scale alternatives", juce::dontSendNotification);
    for (auto* label : { &outsideHeading, &emptyGroup })
    {
        label->setFont (ui::font (12.0f, true));
        label->setColour (juce::Label::textColourId, ui::textDim);
        alternativesContent.addChildComponent (*label);
    }
    switchViewBtn.onClick = [this] { open (currentSlot, ! alternativesMode); };

    titleLabel.setFont (ui::font (13.0f, true));
    titleLabel.setColour (juce::Label::textColourId, ui::text);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // Create 12 root buttons
    const char* pcNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
    {
        rootButtons[(size_t) i] = std::make_unique<juce::TextButton> (pcNames[i]);
        auto* b = rootButtons[(size_t) i].get();
        b->setClickingTogglesState (false);
        b->setTooltip ("Set the root note of the chord in this slot.");
        b->onClick = [this, i] { selectRoot (i); };
        addAndMakeVisible (b);
    }

    // Common Chord Qualities
    const auto& types = cs::chordTypes();
    for (int t = 0; t < (int) types.size(); ++t)
    {
        const char* sfx = types[(size_t) t].suffix;
        auto b = std::make_unique<juce::TextButton> (sfx[0] == '\0' ? juce::String ("Maj") : juce::String::fromUTF8 (sfx));
        b->setTooltip ("Set the chord quality, such as major, minor or a seventh chord.");
        b->onClick = [this, t] { selectQuality (t); };
        addAndMakeVisible (b.get());
        qualityButtons.emplace_back (std::move (b), t);
    }
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
        cancelGestures();
}

void ChordPickerOverlay::open (int slotIndex, bool showAlternatives)
{
    alternativesMode = showAlternatives;
    alternativesViewport.setViewPosition (0, 0);
    setSlot (slotIndex);
}

void ChordPickerOverlay::setSlot (int slotIndex)
{
    currentSlot = slotIndex;
    prog = processor.getProgression();
    const bool validSlot = currentSlot >= 0 && currentSlot < prog.numSlots;
    const auto* voicing = validSlot ? &prog.slots[(size_t) currentSlot].voicing : nullptr;
    invertUpBtn.setEnabled (voicing != nullptr && cs::canInvertChord (*voicing, cs::InversionDirection::Up));
    invertDownBtn.setEnabled (voicing != nullptr && cs::canInvertChord (*voicing, cs::InversionDirection::Down));

    if (validSlot)
    {
        activeChord = prog.slots[(size_t) currentSlot].chord;
        const juce::String chordName = utf8 (cs::chordName (activeChord, prog.tonic, prog.scale));
        const juce::String roman = utf8 (cs::romanNumeral (activeChord, prog.tonic, prog.scale));
        titleLabel.setText ((alternativesMode ? "Inspect slot " : "Edit chord - slot ") + juce::String (currentSlot + 1)
                               + "    Current: " + chordName + " (" + roman + ")",
                            juce::dontSendNotification);

        // Update root button highlighting
        for (int i = 0; i < 12; ++i)
            rootButtons[(size_t) i]->setToggleState (i == activeChord.root, juce::dontSendNotification);

        // Update quality button highlighting
        for (auto& qb : qualityButtons)
            qb.btn->setToggleState (qb.typeIndex == activeChord.type, juce::dontSendNotification);

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
            const auto name = utf8 (cs::pitchClassName (cs::mod12 (activeChord.root + ivNorm), prog.tonic, prog.scale));
            auto b = std::make_unique<juce::TextButton> (name);
            b->setTooltip ("Set the note in the separate bass part.");
            b->setToggleState (ivNorm == activeChord.bassInt, juce::dontSendNotification);
            b->onClick = [this, ivNorm] { selectBass (ivNorm); };
            addAndMakeVisible (b.get());
            bassButtons.push_back (std::move (b));
        }

    }

    rebuildCandidates();
    resized();
    repaint();
}

void ChordPickerOverlay::rebuildCandidates()
{
    cancelGestures();
    candidateButtons.clear();
    if (currentSlot < 0 || currentSlot >= prog.numSlots)
        return;

    const float currentFit = cs::fitScore (prog);
    const auto& current = prog.slots[(size_t) currentSlot];
    const auto rng = processor.copyVoicingRng();

    auto addCandidate = [&] (const cs::Slot& replacement, float fitDelta, cs::ScaleMembership scale)
    {
        auto label = utf8 (cs::chordName (replacement.chord, prog.tonic, prog.scale));
        if (! alternativesMode)
        {
            label += "  " + juce::String (fitDelta >= 0.0f ? "+" : "") + juce::String (fitDelta, 2);
            auto button = std::make_unique<juce::TextButton> (label);
            button->setTooltip ("Replace the current chord with this suggestion.");
            button->onClick = [this, chord = replacement.chord] { applyChord (chord); };
            addAndMakeVisible (*button);
            candidateButtons.push_back ({ std::move (button), false });
            return;
        }

        label += "   " + utf8 (cs::romanNumeral (replacement.chord, prog.tonic, prog.scale));
        juce::String details, outsideNotes;
        if (scale.outsideChord != 0)
        {
            juce::StringArray notes;
            for (int pc = 0; pc < 12; ++pc)
                if ((scale.outsideChord & (1u << pc)) != 0)
                    notes.add (utf8 (cs::pitchClassName (pc, prog.tonic, prog.scale)));
            const auto pitches = notes.joinIntoString (", ");
            outsideNotes = "Outside: " + pitches;
            details = "\nChord notes outside scale: " + pitches;
        }
        if (cs::scaleMembership (replacement.chord, prog.tonic, prog.scale).outsideBass)
            details += "\nSeparate bass outside scale: "
                     + utf8 (cs::pitchClassName (replacement.chord.bassPc(), prog.tonic, prog.scale));

        auto row = std::make_unique<AuditionButton> (label,
            std::array<float, 3> { replacement.tension - current.tension, replacement.brightness - current.brightness, fitDelta },
            std::move (outsideNotes), details);
        const int slot = currentSlot;
        row->onAudition = [this, slot, replacement] (bool inContext)
        {
            auto preview = prog;
            useReplacement (preview, slot, replacement);
            processor.auditionAlternative (preview, slot, inContext);
        };
        row->onRelease = [this] { processor.stopAlternativeAudition(); };
        row->replace.onClick = [this, slot, replacement]
        {
            cancelGestures();
            processor.edit ([slot, replacement] (cs::Progression& p, cs::Rng&)
            {
                if (slot >= 0 && slot < p.numSlots)
                    useReplacement (p, slot, replacement);
            });
            processor.auditionSlot (slot);
            setSlot (slot);
        };
        alternativesContent.addAndMakeVisible (*row);
        candidateButtons.push_back ({ std::move (row), scale.outsideChord != 0 });
    };

    if (! alternativesMode)
    {
        for (const auto& chord : cs::alternatives (prog, currentSlot, 6))
        {
            auto test = prog;
            auto previewRng = rng;
            cs::setChord (test, currentSlot, chord, previewRng);
            addCandidate (test.slots[(size_t) currentSlot], cs::fitScore (test) - currentFit, {});
        }
        return;
    }

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
}

void ChordPickerOverlay::layoutAlternatives (juce::Rectangle<int> area)
{
    showOutside.setBounds (area.removeFromTop (27));
    candidateArea = area.removeFromTop (23);
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

void ChordPickerOverlay::paint (juce::Graphics& g)
{
    // Semi-transparent dark glass background
    g.setColour (ui::background.withAlpha (0.96f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    g.setColour (ui::outline.brighter (0.25f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    if (alternativesMode)
    {
        auto header = candidateArea.withTrimmedRight (alternativesViewport.getScrollBarThickness());
        const auto cells = metricCells (header);
        g.setFont (ui::font (12.0f, true));
        g.setColour (ui::textDim);
        g.drawText ("Chord / Degree", header.reduced (9, 0), juce::Justification::centredLeft);
        for (size_t i = 0; i < cells.size(); ++i)
            g.drawText (metricNames[i], cells[i], juce::Justification::centred);
        return;
    }

    // Sub-section headers
    g.setFont (ui::font (11.0f));
    g.setColour (ui::textDim);

    if (! rootArea.isEmpty())
        g.drawText ("ROOT PITCH", rootArea.getX(), rootArea.getY() - 15, 120, 14, juce::Justification::bottomLeft);

    if (! qualityArea.isEmpty())
        g.drawText ("QUALITY", qualityArea.getX(), qualityArea.getY() - 15, 120, 14, juce::Justification::bottomLeft);

    if (! voicingArea.isEmpty())
        g.drawText ("CHORD VOICING", voicingArea.getX(), voicingArea.getY() - 15, 140, 14, juce::Justification::bottomLeft);

    if (! bassArea.isEmpty())
        g.drawText ("CHANGE BASS", bassArea.getX(), bassArea.getY() - 15, 140, 14, juce::Justification::bottomLeft);

    if (! candidateArea.isEmpty())
        g.drawText ("RECOMMENDED ALTERNATIVES (FIT CHANGE)", candidateArea.getX(), candidateArea.getY() - 15, 240, 14, juce::Justification::bottomLeft);
}

void ChordPickerOverlay::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto topRow = area.removeFromTop (26);
    closeBtn.setBounds (topRow.removeFromRight (64));
    topRow.removeFromRight (6);
    switchViewBtn.setButtonText (alternativesMode ? "Edit chord" : "Alternatives");
    switchViewBtn.setBounds (topRow.removeFromRight (100));
    titleLabel.setBounds (topRow);

    for (auto& button : rootButtons)
        button->setVisible (! alternativesMode);
    for (auto& quality : qualityButtons)
        quality.btn->setVisible (! alternativesMode);
    for (auto& button : bassButtons)
        button->setVisible (! alternativesMode);
    invertUpBtn.setVisible (! alternativesMode);
    invertDownBtn.setVisible (! alternativesMode);
    showOutside.setVisible (alternativesMode);
    alternativesViewport.setVisible (alternativesMode);
    if (alternativesMode)
    {
        layoutAlternatives (area);
        return;
    }

    area.removeFromTop (16);

    // Root row
    rootArea = area.removeFromTop (28);
    const int rootBtnW = (rootArea.getWidth() - 11 * 4) / 12;
    for (int i = 0; i < 12; ++i)
    {
        rootButtons[(size_t) i]->setBounds (rootArea.getX() + i * (rootBtnW + 4),
                                            rootArea.getY(),
                                            i == 11 ? (rootArea.getRight() - (rootArea.getX() + 11 * (rootBtnW + 4))) : rootBtnW,
                                            26);
    }

    area.removeFromTop (16);

    // Quality area (4 rows of 8 for 32 chord types)
    const int qCount = (int) qualityButtons.size();
    const int cols = 8;
    const int rows = (qCount + cols - 1) / cols;
    const int qRowH = 22;
    const int qGap = 4;
    const int qTotalH = rows * qRowH + (rows - 1) * qGap;
    qualityArea = area.removeFromTop (qTotalH);
    if (qCount > 0)
    {
        const int qBtnW = (qualityArea.getWidth() - (cols - 1) * qGap) / cols;
        for (int i = 0; i < qCount; ++i)
        {
            const int r = i / cols;
            const int c = i % cols;
            const int x = qualityArea.getX() + c * (qBtnW + qGap);
            const int y = qualityArea.getY() + r * (qRowH + qGap);
            const int w = (c == cols - 1) ? (qualityArea.getRight() - x) : qBtnW;
            qualityButtons[(size_t) i].btn->setBounds (x, y, w, qRowH);
        }
    }

    area.removeFromTop (16);

    voicingArea = area.removeFromTop (28);
    invertUpBtn.setBounds (voicingArea.withSize (112, 26));
    invertDownBtn.setBounds (invertUpBtn.getBounds().translated (118, 0));

    area.removeFromTop (16);
    bassArea = area.removeFromTop (28);
    const int bCount = (int) bassButtons.size();
    if (bCount > 0)
    {
        const int bBtnW = (bassArea.getWidth() - (bCount - 1) * 6) / bCount;
        for (int i = 0; i < bCount; ++i)
            bassButtons[(size_t) i]->setBounds (bassArea.getX() + i * (bBtnW + 6), bassArea.getY(), bBtnW, 26);
    }

    area.removeFromTop (20);

    // Candidates row
    candidateArea = area.removeFromTop (28);
    const int cCount = (int) candidateButtons.size();
    if (cCount > 0)
    {
        const int cBtnW = (candidateArea.getWidth() - (cCount - 1) * 6) / cCount;
        for (int i = 0; i < cCount; ++i)
            candidateButtons[(size_t) i].btn->setBounds (candidateArea.getX() + i * (cBtnW + 6), candidateArea.getY(), cBtnW, 26);
    }
}

void ChordPickerOverlay::mouseDown (const juce::MouseEvent&)
{
    // Absorb clicks so they don't propagate underneath
}
