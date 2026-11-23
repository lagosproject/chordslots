// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiDragTile.h"

MidiDragTile::MidiDragTile (ChordSlotsProcessor& p,
                           ChordSlotsProcessor::MidiExportMode mode,
                           const juce::String& label)
    : processor (p), exportMode (mode), tileLabel (label)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void MidiDragTile::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (isMouseOver() ? ui::panelHi.brighter (0.1f) : ui::panelHi);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (ui::outline);
    const float dashes[] = { 4.0f, 3.0f };
    juce::Path outlinePath, dashed;
    outlinePath.addRoundedRectangle (r, 5.0f);
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outlinePath, dashes, 2);
    g.setColour (ui::textDim);
    g.fillPath (dashed);

    if (r.getHeight() >= 36.0f)
    {
        // Tiny piano-roll glyph
        const auto icon = r.withSizeKeepingCentre (26.0f, 16.0f).translated (0.0f, -8.0f);
        g.setColour (ui::accent.brighter (0.2f));
        g.fillRoundedRectangle (icon.getX(), icon.getY(), 12.0f, 3.5f, 1.5f);
        g.fillRoundedRectangle (icon.getX() + 7.0f, icon.getY() + 6.0f, 14.0f, 3.5f, 1.5f);
        g.fillRoundedRectangle (icon.getX() + 14.0f, icon.getY() + 12.0f, 12.0f, 3.5f, 1.5f);

        g.setColour (ui::text);
        g.setFont (ui::font (12.0f, true));
        g.drawText (tileLabel, r.withTrimmedTop (r.getHeight() * 0.52f).toNearestInt(), juce::Justification::centred, false);
    }
    else
    {
        // Compact height for dual stacked tiles (Chords / Bass)
        g.setColour (exportMode == ChordSlotsProcessor::MidiExportMode::BassOnly ? ui::accent.brighter (0.35f) : ui::accent);
        const float tagY = r.getCentreY() - 3.5f;
        g.fillRoundedRectangle (r.getX() + 6.0f, tagY, 7.0f, 7.0f, 1.5f);

        g.setColour (ui::text);
        g.setFont (ui::font (11.0f, true));
        g.drawFittedText (tileLabel, r.withTrimmedLeft (16.0f).withTrimmedRight (4.0f).toNearestInt(),
                          juce::Justification::centredLeft, 2, 0.75f);
    }
}

void MidiDragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;
    const juce::File file = processor.writeTempMidiFile (exportMode);
    if (! file.existsAsFile())
        return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                [this] { dragging = false; });
}
