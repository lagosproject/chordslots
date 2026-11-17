// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ChordPickerOverlay.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
}

ChordPickerOverlay::AuditionButton::AuditionButton() : juce::TextButton ("Listen")
{
    onClick = [this]
    {
        if (! fired && onAudition)
            onAudition (false);
    };
}

void ChordPickerOverlay::AuditionButton::mouseDown (const juce::MouseEvent& e)
{
    fired = false;
    juce::TextButton::mouseDown (e);
    if (e.mods.isLeftButtonDown())
        startTimer (450);
}

void ChordPickerOverlay::AuditionButton::mouseUp (const juce::MouseEvent& e)
{
    stopTimer();
    juce::TextButton::mouseUp (e);
    fired = false;
}

void ChordPickerOverlay::AuditionButton::timerCallback()
{
    stopTimer();
    if (isDown() && onAudition)
    {
        fired = true;
        onAudition (true);
    }
}

ChordPickerOverlay::ChordPickerOverlay (ChordSlotsProcessor& proc)
    : processor (proc)
{
    setAlwaysOnTop (true);

    closeBtn.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (closeBtn);
    addChildComponent (inspectorBtn);
    addChildComponent (swapBtn);
    addChildComponent (auditionBtn);
    addChildComponent (selectionLabel);
    selectionLabel.setFont (ui::font (13.0f, true));
    auditionBtn.onAudition = [this] (bool inContext)
    {
        if (selectedCandidate >= 0)
            processor.auditionAlternative (currentSlot, candidateButtons[(size_t) selectedCandidate].chord, inContext);
    };
    swapBtn.onClick = [this]
    {
        if (selectedCandidate >= 0)
            applyChord (candidateButtons[(size_t) selectedCandidate].chord);
    };
    inspectorBtn.onClick = [this] { open (currentSlot, false); };

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
        b->onClick = [this, i] { selectRoot (i); };
        addAndMakeVisible (b);
    }

    // Common Chord Qualities
    const auto& types = cs::chordTypes();
    for (int t = 0; t < (int) types.size(); ++t)
    {
        const char* sfx = types[(size_t) t].suffix;
        auto b = std::make_unique<juce::TextButton> (sfx[0] == '\0' ? juce::String ("Maj") : juce::String::fromUTF8 (sfx));
        b->onClick = [this, t] { selectQuality (t); };
        addAndMakeVisible (b.get());
        qualityButtons.emplace_back (std::move (b), t);
    }
}

void ChordPickerOverlay::open (int slotIndex, bool showPlot)
{
    if (showPlot != plotMode || slotIndex != currentSlot)
        selectedCandidate = -1;
    plotMode = showPlot;
    setSlot (slotIndex);
}

void ChordPickerOverlay::setSlot (int slotIndex)
{
    currentSlot = slotIndex;
    prog = processor.getProgression();

    if (currentSlot >= 0 && currentSlot < prog.numSlots)
    {
        activeChord = prog.slots[(size_t) currentSlot].chord;
        const juce::String chordName = utf8 (cs::chordName (activeChord, prog.tonic, prog.scale));
        const juce::String roman = utf8 (cs::romanNumeral (activeChord, prog.tonic, prog.scale));
        titleLabel.setText ((plotMode ? "Alternatives for slot " : "Chord inspector - slot ") + juce::String (currentSlot + 1)
                               + "    Current: " + chordName + " (" + roman + ")",
                            juce::dontSendNotification);

        // Update root button highlighting
        for (int i = 0; i < 12; ++i)
            rootButtons[(size_t) i]->setToggleState (i == activeChord.root, juce::dontSendNotification);

        // Update quality button highlighting
        for (auto& qb : qualityButtons)
            qb.btn->setToggleState (qb.typeIndex == activeChord.type, juce::dontSendNotification);

        // Rebuild Inversion / Bass buttons
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
            juce::String name;
            if (ivNorm == 0)
                name = "Root";
            else if (ivNorm == 4 || (ivNorm == 3 && ! cs::hasMajorThird (activeChord.type)))
                name = "1st Inv (3rd)";
            else if (ivNorm == 7 || (ivNorm == 6 && ! cs::hasPerfectFifth (activeChord.type)) || ivNorm == 8)
                name = "2nd Inv (5th)";
            else if (ivNorm == 10 || ivNorm == 11 || (ivNorm == 9 && cs::seventhKind (activeChord.type) == cs::SeventhKind::Diminished7))
                name = "3rd Inv (7th)";
            else
                name = "/" + utf8 (cs::pitchClassName (cs::mod12 (activeChord.root + ivNorm), prog.tonic, prog.scale));

            auto b = std::make_unique<juce::TextButton> (name);
            b->setToggleState (ivNorm == activeChord.bassInt, juce::dontSendNotification);
            b->onClick = [this, ivNorm] { selectBass (ivNorm); };
            addAndMakeVisible (b.get());
            bassButtons.push_back (std::move (b));
        }

        rebuildCandidates();
    }

    resized();
    repaint();
}

void ChordPickerOverlay::rebuildCandidates()
{
    cs::Chord previous = activeChord;
    if (selectedCandidate >= 0 && selectedCandidate < (int) candidateButtons.size())
        previous = candidateButtons[(size_t) selectedCandidate].chord;
    candidateButtons.clear();
    selectedCandidate = -1;
    if (currentSlot < 0 || currentSlot >= prog.numSlots)
        return;

    const float currentFit = cs::fitScore (prog);
    const auto alternatives = cs::alternatives (prog, currentSlot, plotMode ? 10 : 6);
    for (const auto& chord : alternatives)
    {
        auto test = prog;
        cs::Rng rng (123);
        cs::setChord (test, currentSlot, chord, rng);
        const auto& slot = test.slots[(size_t) currentSlot];
        const float delta = cs::fitScore (test) - currentFit;
        auto label = utf8 (cs::chordName (chord, prog.tonic, prog.scale));
        if (! plotMode)
            label += "  " + juce::String (delta >= 0.0f ? "+" : "") + juce::String (delta, 2);

        auto button = std::make_unique<juce::TextButton> (label);
        const int index = (int) candidateButtons.size();
        button->onClick = [this, index, chord]
        {
            if (plotMode)
                selectCandidate (index);
            else
                applyChord (chord);
        };
        if (chord == previous)
            selectedCandidate = index;
        addAndMakeVisible (button.get());
        candidateButtons.emplace_back (std::move (button), chord, juce::Point<float> { slot.brightness, slot.tension });
    }
    if (plotMode)
        selectCandidate (selectedCandidate >= 0 ? selectedCandidate : (candidateButtons.empty() ? -1 : 0));
}

void ChordPickerOverlay::selectCandidate (int index)
{
    selectedCandidate = index;
    auditionBtn.setEnabled (index >= 0);
    swapBtn.setEnabled (index >= 0);
    for (int i = 0; i < (int) candidateButtons.size(); ++i)
        candidateButtons[(size_t) i].btn->setToggleState (i == index, juce::dontSendNotification);

    if (index < 0)
        selectionLabel.setText ("No alternatives", juce::dontSendNotification);
    else
    {
        const auto& candidate = candidateButtons[(size_t) index];
        const auto& current = prog.slots[(size_t) currentSlot];
        auto delta = [] (float diff)
        {
            const int n = juce::roundToInt (diff * 100.0f);
            return juce::String (n > 0 ? "+" : "") + juce::String (n);
        };
        selectionLabel.setText (utf8 (cs::chordName (candidate.chord, prog.tonic, prog.scale))
                                    + "    Brightness " + delta (candidate.metrics.x - current.brightness)
                                    + "    Tension " + delta (candidate.metrics.y - current.tension),
                                juce::dontSendNotification);
    }
    repaint();
}

void ChordPickerOverlay::layoutPlot (juce::Rectangle<int> area)
{
    auto controls = area.removeFromBottom (27);
    swapBtn.setBounds (controls.removeFromRight (70));
    controls.removeFromRight (6);
    auditionBtn.setBounds (controls.removeFromRight (175));
    auditionBtn.setButtonText ("Listen (hold for loop)");
    controls.removeFromRight (8);
    selectionLabel.setBounds (controls);
    plotArea = area.withTrimmedLeft (60).withTrimmedRight (12).withTrimmedTop (12).withTrimmedBottom (29);

    if (currentSlot < 0 || currentSlot >= prog.numSlots || plotArea.isEmpty())
        return;

    auto position = [this] (float brightness, float tension)
    {
        return juce::Point<float> { plotArea.getX() + juce::jlimit (0.0f, 1.0f, brightness) * plotArea.getWidth(),
                                    plotArea.getBottom() - juce::jlimit (0.0f, 1.0f, tension) * plotArea.getHeight() };
    };
    const auto& current = prog.slots[(size_t) currentSlot];
    currentPoint = position (current.brightness, current.tension);

    // Keep the metric coordinates exact and move only labels that would overlap.
    const auto bounds = plotArea.expanded (14, 8).getIntersection (area);
    std::vector<juce::Rectangle<int>> occupied { juce::Rectangle<int> (20, 20).withCentre (currentPoint.roundToInt()) };
    for (auto& candidate : candidateButtons)
    {
        candidate.point = position (candidate.metrics.x, candidate.metrics.y);
        juce::Rectangle<int> best;
        float distance = std::numeric_limits<float>::max();
        for (int y = bounds.getY(); y + 22 <= bounds.getBottom(); y += 12)
            for (int x = bounds.getX(); x + 86 <= bounds.getRight(); x += 12)
            {
                const juce::Rectangle<int> label (x, y, 86, 22);
                const float d = candidate.point.getDistanceSquaredFrom (label.getCentre().toFloat());
                if (d < distance && std::none_of (occupied.begin(), occupied.end(),
                                                  [&] (auto other) { return other.expanded (3).intersects (label); }))
                {
                    best = label;
                    distance = d;
                }
            }
        occupied.push_back (best);
        candidate.btn->setBounds (best);
    }
}

void ChordPickerOverlay::paintPlot (juce::Graphics& g)
{
    if (currentSlot < 0 || currentSlot >= prog.numSlots || plotArea.isEmpty())
        return;
    g.setColour (ui::panel);
    g.fillRect (plotArea);
    g.setColour (ui::outline);
    g.drawRect (plotArea);
    g.drawVerticalLine ((int) currentPoint.x, (float) plotArea.getY(), (float) plotArea.getBottom());
    g.drawHorizontalLine ((int) currentPoint.y, (float) plotArea.getX(), (float) plotArea.getRight());
    g.setFont (ui::font (11.0f));
    g.setColour (ui::textDim);
    g.drawText ("Tension", plotArea.getX() - 60, plotArea.getY(), 52, 15, juce::Justification::centredRight);
    g.drawText ("100", plotArea.getX() - 31, plotArea.getY() + 12, 22, 14, juce::Justification::centredRight);
    g.drawText ("0", plotArea.getX() - 31, plotArea.getBottom() - 13, 22, 14, juce::Justification::centredRight);
    const auto xAxis = plotArea.withY (plotArea.getBottom() + 4).withHeight (15);
    g.drawText ("0", xAxis, juce::Justification::centredLeft);
    g.drawText ("Brightness", xAxis, juce::Justification::centred);
    g.drawText ("100", xAxis, juce::Justification::centredRight);

    for (int i = 0; i < (int) candidateButtons.size(); ++i)
    {
        const auto& candidate = candidateButtons[(size_t) i];
        const auto colour = i == selectedCandidate ? ui::accent.brighter (0.5f) : ui::textDim;
        g.setColour (colour);
        g.drawLine ({ candidate.point, candidate.btn->getBounds().getCentre().toFloat() }, 1.0f);
        g.fillEllipse (candidate.point.x - 3.0f, candidate.point.y - 3.0f, 6.0f, 6.0f);
    }
    g.setColour (juce::Colours::white);
    g.drawEllipse (currentPoint.x - 5.0f, currentPoint.y - 5.0f, 10.0f, 10.0f, 2.0f);
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
    activeChord.bassInt = bassInt;
    applyChord (activeChord);
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

    if (plotMode)
    {
        paintPlot (g);
        return;
    }

    // Sub-section headers
    g.setFont (ui::font (11.0f));
    g.setColour (ui::textDim);

    if (! rootArea.isEmpty())
        g.drawText ("ROOT PITCH", rootArea.getX(), rootArea.getY() - 15, 120, 14, juce::Justification::bottomLeft);

    if (! qualityArea.isEmpty())
        g.drawText ("QUALITY", qualityArea.getX(), qualityArea.getY() - 15, 120, 14, juce::Justification::bottomLeft);

    if (! bassArea.isEmpty())
        g.drawText ("INVERSION / BASS", bassArea.getX(), bassArea.getY() - 15, 140, 14, juce::Justification::bottomLeft);

    if (! candidateArea.isEmpty())
        g.drawText ("RECOMMENDED ALTERNATIVES (FIT CHANGE)", candidateArea.getX(), candidateArea.getY() - 15, 240, 14, juce::Justification::bottomLeft);
}

void ChordPickerOverlay::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto topRow = area.removeFromTop (26);
    closeBtn.setBounds (topRow.removeFromRight (64));
    inspectorBtn.setVisible (plotMode);
    if (plotMode)
    {
        topRow.removeFromRight (6);
        inspectorBtn.setBounds (topRow.removeFromRight (74));
    }
    titleLabel.setBounds (topRow);

    for (auto& button : rootButtons)
        button->setVisible (! plotMode);
    for (auto& quality : qualityButtons)
        quality.btn->setVisible (! plotMode);
    for (auto& button : bassButtons)
        button->setVisible (! plotMode);
    auditionBtn.setVisible (plotMode);
    swapBtn.setVisible (plotMode);
    selectionLabel.setVisible (plotMode);
    if (plotMode)
    {
        layoutPlot (area);
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

    // Bass / Inversion row
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
