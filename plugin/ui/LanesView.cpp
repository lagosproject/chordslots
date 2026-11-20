// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LanesView.h"

namespace
{
const juce::Colour laneColours[3] = { ui::brightness, ui::tension, ui::energy };
const char* const laneNames[3] = { "BRIGHTNESS", "TENSION", "ENERGY" };

float& targetRef (cs::Slot& s, int lane) { return lane == 0 ? s.targetB : (lane == 1 ? s.targetT : s.targetE); }
float targetOf (const cs::Slot& s, int lane) { return lane == 0 ? s.targetB : (lane == 1 ? s.targetT : s.targetE); }
float actualOf (const cs::Slot& s, int lane) { return lane == 0 ? s.brightness : (lane == 1 ? s.tension : s.energy); }
}

LanesView::LanesView (ChordSlotsProcessor& p) : processor (p) {}

void LanesView::setProgression (const cs::Progression& p)
{
    prog = p;
    repaint();
}

juce::Rectangle<float> LanesView::laneBounds (int lane) const
{
    const float gap = 6.0f;
    const float h = ((float) getHeight() - 2.0f * gap) / 3.0f;
    return { 0.0f, (float) lane * (h + gap), (float) getWidth(), h };
}

void LanesView::paint (juce::Graphics& g)
{
    const double len = (double) prog.lengthSteps();
    for (int lane = 0; lane < 3; ++lane)
    {
        const auto lb = laneBounds (lane);
        const juce::Colour c = laneColours[lane];

        for (int i = 0; i < prog.numSlots; ++i)
        {
            const cs::Slot& s = prog.slots[(size_t) i];
            const float x0 = (float) (s.start / len) * lb.getWidth();
            const float x1 = (float) (prog.slotEnd (i) / len) * lb.getWidth();
            const juce::Rectangle<float> col (x0 + 1.5f, lb.getY(), juce::jmax (2.0f, x1 - x0 - 3.0f), lb.getHeight());

            g.setColour (ui::background);
            g.fillRoundedRectangle (col, 4.0f);

            const float target = targetOf (s, lane);
            const float actual = actualOf (s, lane);
            const float alpha = s.locked ? 0.45f : 1.0f;
            const auto usable = col.reduced (0.0f, 3.0f);
            const float ty = usable.getBottom() - target * usable.getHeight();

            {
                juce::Graphics::ScopedSaveState state (g);
                juce::Path clip;
                clip.addRoundedRectangle (col, 4.0f);
                g.reduceClipRegion (clip);
                g.setColour (c.withAlpha (0.28f * alpha));
                g.fillRect (col.withTop (ty));
                g.setColour (c.withAlpha (alpha));
                g.fillRect (col.getX(), ty - 1.0f, col.getWidth(), 2.0f);
            }

            // Measured value of the chord that is actually there.
            const float ay = usable.getBottom() - actual * usable.getHeight();
            const float cx = col.getCentreX();
            const float half = juce::jmin (9.0f, col.getWidth() * 0.5f - 1.0f);
            if (half > 1.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.92f * alpha));
                g.fillRoundedRectangle (cx - half, ay - 1.5f, half * 2.0f, 3.0f, 1.5f);
            }
        }

        g.setColour (c.withAlpha (0.9f));
        g.setFont (ui::font (10.0f, true));
        const bool showingReadout = (readoutLane == lane && readoutSlot >= 0 && readoutSlot < prog.numSlots)
            || (readoutLane < 0 && selectedSlot >= 0 && selectedSlot < prog.numSlots);
        if (! showingReadout)
            g.drawText (laneNames[lane], lb.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::topLeft, false);
    }

    // Value readout for the slot under the pointer: target and measured chord value, in percent.
    for (int lane = 0; lane < 3; ++lane)
    if ((readoutLane == lane && readoutSlot >= 0 && readoutSlot < prog.numSlots) || (readoutLane < 0 && selectedSlot >= 0 && selectedSlot < prog.numSlots))
    {
        const int slot = readoutLane >= 0 ? readoutSlot : selectedSlot;
        const cs::Slot& s = prog.slots[(size_t) slot];
        const auto lb = laneBounds (lane);
        const float x0 = (float) (s.start / len) * lb.getWidth();
        const float x1 = (float) (prog.slotEnd (slot) / len) * lb.getWidth();
        const juce::String text = juce::String (laneNames[lane]) + "  Slot " + juce::String (slot + 1) + "  Target " + juce::String (juce::roundToInt (100.0f * targetOf (s, lane))) + "%"
                                + "  Result " + juce::String (juce::roundToInt (100.0f * actualOf (s, lane))) + "%";
        g.setFont (ui::font (11.0f, true));
        const float w = juce::jmin ((float) getWidth(), juce::jmax (46.0f, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 10.0f));
        const float cx = (x0 + x1) * 0.5f;
        const float bx = juce::jlimit (0.0f, (float) getWidth() - w, cx - w * 0.5f);
        const juce::Rectangle<float> pill (bx, lb.getY() + 3.0f, w, 16.0f);
        g.setColour (ui::background.withAlpha (0.92f));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (laneColours[lane]);
        g.drawRoundedRectangle (pill, 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.drawText (text, pill.toNearestInt(), juce::Justification::centred, false);
    }

    if (isDragOver)
    {
        g.setColour (ui::accent.withAlpha (0.25f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (ui::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
        g.setFont (ui::font (14.0f, true));
        g.setColour (juce::Colours::white);
        g.drawText ("Drop MIDI file here to extract target curves", getLocalBounds(), juce::Justification::centred);
    }
}

juce::String LanesView::getTooltip()
{
    static const char* const help[] = {
        "Guide chord choice and voicing register when regenerating a slot.",
        "Guide slot regeneration toward a target level of harmonic tension in context.",
        "Adjust MIDI velocity and voicing density while keeping the chords. Doublings depend on the voicing style and notes."
    };
    const auto pos = getMouseXYRelative().toFloat();
    for (int lane = 0; lane < 3; ++lane)
        if (laneBounds (lane).contains (pos))
            return juce::String (help[lane]) + " The colored line is Target; the white dash is Result.";

    return "Drop a MIDI file here to extract target curves from it.";
}

int LanesView::slotAtX (float x) const
{
    const double step = (double) x / juce::jmax (1, getWidth()) * prog.lengthSteps();
    for (int i = prog.numSlots - 1; i >= 0; --i)
        if (step >= prog.slots[(size_t) i].start)
            return i;
    return 0;
}

int LanesView::laneAt (juce::Point<float> pos) const
{
    for (int lane = 0; lane < 3; ++lane)
        if (laneBounds (lane).contains (pos))
            return lane;
    return -1;
}

void LanesView::setReadout (int lane, int slot)
{
    if (lane == readoutLane && slot == readoutSlot)
        return;
    readoutLane = lane;
    readoutSlot = slot;
    repaint();
}

void LanesView::mouseMove (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position);
    setReadout (lane, lane < 0 ? -1 : slotAtX (e.position.x));
}

void LanesView::mouseExit (const juce::MouseEvent&)
{
    if (dragLane < 0)
        setReadout (-1, -1);
}

void LanesView::applyDrag (const juce::MouseEvent& e)
{
    if (dragLane < 0)
        return;
    const auto lb = laneBounds (dragLane).reduced (0.0f, 3.0f);
    const float value = juce::jlimit (0.0f, 1.0f, (lb.getBottom() - e.position.y) / lb.getHeight());
    const int slot = slotAtX (e.position.x);

    editedMask |= 1u << slot;
    setReadout (dragLane, slot);
    const int lane = dragLane;
    processor.edit ([slot, lane, value] (cs::Progression& p, cs::Rng&)
    {
        if (slot < p.numSlots)
        {
            targetRef (p.slots[(size_t) slot], lane) = value;
            p.mood = -1;
        }
    }, false);
}

void LanesView::mouseDown (const juce::MouseEvent& e)
{
    dragLane = -1;
    for (int lane = 0; lane < 3; ++lane)
        if (laneBounds (lane).expanded (0.0f, 4.0f).contains (e.position))
            dragLane = lane;
    if (dragLane < 0)
        return;
    editedMask = 0;
    processor.beginGesture();
    applyDrag (e);
}

void LanesView::mouseDrag (const juce::MouseEvent& e) { applyDrag (e); }

void LanesView::mouseUp (const juce::MouseEvent&)
{
    if (dragLane < 0 || editedMask == 0)
        return;
    const unsigned mask = editedMask;
    const bool energy = dragLane == 2;
    dragLane = -1;
    setReadout (-1, -1);
    // Brightness and Tension regenerate by the edited-slot mask.
    // Energy reapplies voicing and velocity targets to the edited, unlocked slots only.
    processor.edit ([mask, energy] (cs::Progression& p, cs::Rng& rng)
    {
        if (energy)
            cs::applyEnergy (p, rng, mask);
        else
            cs::rerollSlots (p, mask, rng);
    }, false);
}

bool LanesView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void LanesView::fileDragEnter (const juce::StringArray&, int, int)
{
    isDragOver = true;
    repaint();
}

void LanesView::fileDragExit (const juce::StringArray&)
{
    isDragOver = false;
    repaint();
}

void LanesView::filesDropped (const juce::StringArray& files, int, int)
{
    isDragOver = false;
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
        {
            processor.loadMidiToCurves (juce::File (f));
            break;
        }
    }
    repaint();
}
