// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ChordPickerOverlay.h"

namespace
{
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
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
        auto b = std::make_unique<juce::TextButton> (sfx[0] == '\0' ? "Maj" : sfx);
        b->onClick = [this, t] { selectQuality (t); };
        addAndMakeVisible (b.get());
        qualityButtons.emplace_back (std::move (b), t);
    }
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
        titleLabel.setText ("CHORD INSPECTOR — Slot " + juce::String (currentSlot + 1) + " (" + chordName + " · " + roman + ")",
                            juce::dontSendNotification);

        // Update root button highlighting
        for (int i = 0; i < 12; ++i)
            rootButtons[(size_t) i]->setColour (juce::TextButton::buttonColourId,
                                                i == activeChord.root ? ui::accent.withAlpha (0.4f) : ui::panel);

        // Update quality button highlighting
        for (auto& qb : qualityButtons)
            qb.btn->setColour (juce::TextButton::buttonColourId,
                               qb.typeIndex == activeChord.type ? ui::accent.withAlpha (0.4f) : ui::panel);

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
            b->setColour (juce::TextButton::buttonColourId,
                          ivNorm == activeChord.bassInt ? ui::accent.withAlpha (0.4f) : ui::panel);
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
    candidateButtons.clear();
    if (currentSlot < 0 || currentSlot >= prog.numSlots)
        return;

    const float curFit = cs::fitScore (prog);
    const auto alts = cs::alternatives (prog, currentSlot, 6);

    for (const auto& altChord : alts)
    {
        cs::Progression testProg = prog;
        cs::Rng rng (123);
        cs::setChord (testProg, currentSlot, altChord, rng);
        const float newFit = cs::fitScore (testProg);
        const float delta = newFit - curFit;

        const juce::String cName = utf8 (cs::chordName (altChord, prog.tonic, prog.scale));
        const juce::String roman = utf8 (cs::romanNumeral (altChord, prog.tonic, prog.scale));
        juce::String deltaStr = (delta >= 0.0f ? "+" : "") + juce::String (delta, 2);

        auto b = std::make_unique<juce::TextButton> (cName + " (" + roman + ")  " + deltaStr);
        b->setColour (juce::TextButton::buttonColourId, delta >= 0.0f ? ui::accent.withAlpha (0.35f) : ui::panel);
        b->onClick = [this, altChord] { applyChord (altChord); };
        addAndMakeVisible (b.get());
        candidateButtons.emplace_back (std::move (b), altChord, delta);
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
        g.drawText ("RECOMMENDED ALTERNATIVES (\u0394FIT)", candidateArea.getX(), candidateArea.getY() - 15, 240, 14, juce::Justification::bottomLeft);
}

void ChordPickerOverlay::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto topRow = area.removeFromTop (26);
    closeBtn.setBounds (topRow.removeFromRight (26));
    titleLabel.setBounds (topRow);

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
