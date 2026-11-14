// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace ui
{

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour colourForBrightness (float b)
{
    const juce::Colour dark (0xff232a55), mid (0xff6a5d8c), bright (0xfff0b54a);
    b = juce::jlimit (0.0f, 1.0f, b);
    return b < 0.5f ? dark.interpolatedWith (mid, b * 2.0f) : mid.interpolatedWith (bright, (b - 0.5f) * 2.0f);
}

} // namespace ui

namespace
{

juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

const juce::Colour laneColours[3] = { ui::brightness, ui::tension, ui::energy };
const char* const laneNames[3] = { "BRIGHTNESS", "TENSION", "ENERGY" };

float& targetRef (cs::Slot& s, int lane) { return lane == 0 ? s.targetB : (lane == 1 ? s.targetT : s.targetE); }
float targetOf (const cs::Slot& s, int lane) { return lane == 0 ? s.targetB : (lane == 1 ? s.targetT : s.targetE); }
float actualOf (const cs::Slot& s, int lane) { return lane == 0 ? s.brightness : (lane == 1 ? s.tension : s.energy); }

void drawPadlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool closed)
{
    g.setColour (c);
    const auto body = r.withTrimmedTop (r.getHeight() * 0.45f);
    g.fillRoundedRectangle (body, 2.0f);
    juce::Path shackle;
    const float w = r.getWidth() * 0.56f;
    const float x = r.getCentreX() - w * 0.5f + (closed ? 0.0f : w * 0.55f);
    shackle.addCentredArc (x + w * 0.5f, body.getY(), w * 0.5f, r.getHeight() * 0.36f, 0.0f,
                           -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.strokePath (shackle, juce::PathStrokeType (1.8f));
}

} // namespace

//==============================================================================
ChordSlotsLookAndFeel::ChordSlotsLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::background);
    setColour (juce::ComboBox::backgroundColourId, ui::panel);
    setColour (juce::ComboBox::outlineColourId, ui::outline);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::textDim);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::headerTextColourId, ui::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, ui::text);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void ChordSlotsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.75f);
    const bool primary = b.getProperties().getWithDefault ("primary", false);
    juce::Colour fill = primary ? ui::accent : (b.getToggleState() ? ui::accent.withAlpha (0.75f) : ui::panelHi);
    if (down)
        fill = fill.darker (0.25f);
    else if (over)
        fill = fill.brighter (0.12f);
    if (! b.isEnabled())
        fill = fill.withAlpha (0.35f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (b.getToggleState() || primary ? ui::accent.brighter (0.3f) : ui::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

juce::Font ChordSlotsLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const bool primary = b.getProperties().getWithDefault ("primary", false);
    return ui::font (primary ? juce::jmin (20.0f, (float) h * 0.45f) : 13.5f, primary);
}

void ChordSlotsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thickness = juce::jmax (3.0f, radius * 0.22f);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f, 0.0f, startAngle, angle, true);

    g.setColour (ui::panelHi);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - thickness - 3.0f;
    if (inner > 3.0f)
    {
        g.setColour (ui::panel.brighter (0.08f));
        g.fillEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (centre));
        g.setColour (ui::text);
        juce::Path tick;
        tick.addRoundedRectangle (-1.2f, -inner + 2.0f, 2.4f, inner * 0.5f, 1.2f);
        g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }
}

void ChordSlotsLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (w, h).toFloat().reduced (0.75f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::accent : ui::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (ui::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

juce::Font ChordSlotsLookAndFeel::getComboBoxFont (juce::ComboBox&) { return ui::font (14.0f); }
juce::Font ChordSlotsLookAndFeel::getPopupMenuFont() { return ui::font (14.0f); }
juce::Font ChordSlotsLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

//==============================================================================
TimelineView::TimelineView (ChordSlotsProcessor& p) : processor (p) {}

void TimelineView::setProgression (const cs::Progression& p)
{
    prog = p;
    if (selected >= prog.numSlots)
        selected = -1;
    repaint();
}

void TimelineView::setPlayhead (double step)
{
    if (std::abs (step - playhead) > 1.0e-6)
    {
        playhead = step;
        repaint();
    }
}

float TimelineView::xForStep (double step) const
{
    return (float) (step / (double) prog.lengthSteps()) * (float) getWidth();
}

int TimelineView::stepForX (float x) const
{
    return (int) std::lround ((double) x / juce::jmax (1, getWidth()) * prog.lengthSteps());
}

int TimelineView::cutAt (float x) const
{
    for (int i = 1; i < prog.numSlots; ++i)
        if (std::abs (x - xForStep (prog.slots[(size_t) i].start)) <= 5.0f)
            return i;
    return -1;
}

int TimelineView::slotAt (float x) const
{
    const double step = (double) x / juce::jmax (1, getWidth()) * prog.lengthSteps();
    for (int i = prog.numSlots - 1; i >= 0; --i)
        if (step >= prog.slots[(size_t) i].start)
            return i;
    return 0;
}

juce::Rectangle<float> TimelineView::slotBounds (int i) const
{
    const float x0 = xForStep (prog.slots[(size_t) i].start);
    const float x1 = xForStep (prog.slotEnd (i));
    return { x0 + 1.5f, 18.0f, juce::jmax (2.0f, x1 - x0 - 3.0f), (float) getHeight() - 18.0f };
}

juce::Rectangle<float> TimelineView::lockBounds (int i) const
{
    const auto r = slotBounds (i);
    return { r.getRight() - 21.0f, r.getY() + 7.0f, 13.0f, 14.0f };
}

void TimelineView::paint (juce::Graphics& g)
{
    // Ruler: bars and beats
    const int len = prog.lengthSteps();
    for (int step = 0; step <= len; step += 4)
    {
        const bool bar = step % cs::kStepsPerBar == 0;
        const float x = juce::jlimit (0.5f, (float) getWidth() - 0.5f, xForStep (step));
        g.setColour (bar ? ui::textDim : ui::outline);
        g.drawVerticalLine ((int) x, bar ? 2.0f : 9.0f, 15.0f);
        if (bar && step < len)
        {
            g.setFont (ui::font (10.5f));
            g.drawText (juce::String (step / cs::kStepsPerBar + 1), (int) x + 4, 0, 30, 13, juce::Justification::centredLeft);
        }
    }

    for (int i = 0; i < prog.numSlots; ++i)
    {
        const cs::Slot& s = prog.slots[(size_t) i];
        const auto r = slotBounds (i);
        const juce::Colour fill = ui::colourForBrightness (s.brightness);
        const bool lightText = fill.getPerceivedBrightness() < 0.6f;
        const juce::Colour ink = lightText ? juce::Colours::white : juce::Colour (0xff1a1630);

        g.setColour (i == hoverSlot && hoverCut < 0 ? fill.brighter (0.12f) : fill);
        g.fillRoundedRectangle (r, 7.0f);

        // Tension as a strip along the bottom edge.
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 7.0f);
            g.reduceClipRegion (clip);
            g.setColour (ui::tension.withAlpha (0.9f));
            g.fillRect (r.getX(), r.getBottom() - 4.0f, r.getWidth() * s.tension, 4.0f);
        }

        if (s.locked || i == selected)
        {
            g.setColour (s.locked ? ui::accent.brighter (0.25f) : juce::Colours::white.withAlpha (0.7f));
            g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, s.locked ? 2.0f : 1.2f);
        }

        if (r.getWidth() > 26.0f)
        {
            const juce::String name = utf8 (cs::chordName (s.chord, prog.tonic, prog.scale));
            const float size = juce::jlimit (12.0f, 26.0f, r.getWidth() / juce::jmax (2.0f, (float) name.length() * 0.62f));
            auto textArea = r.reduced (6.0f, 0.0f);
            g.setColour (ink);
            g.setFont (ui::font (size, true));
            g.drawText (name, textArea.withHeight (r.getHeight() * 0.62f).translated (0.0f, r.getHeight() * 0.10f),
                        juce::Justification::centred, false);

            g.setColour (ink.withAlpha (0.72f));
            g.setFont (ui::font (12.0f));
            g.drawText (utf8 (cs::romanNumeral (s.chord, prog.tonic)),
                        textArea.withTop (r.getBottom() - 30.0f).withHeight (18.0f), juce::Justification::centred, false);

            if (r.getWidth() > 44.0f)
            {
                const bool show = s.locked || i == hoverSlot;
                if (show)
                    drawPadlock (g, lockBounds (i), s.locked ? ink : ink.withAlpha (hoverLock && i == hoverSlot ? 0.9f : 0.4f), s.locked);
            }
        }
    }

    if (hoverCut > 0 || dragCut > 0)
    {
        const int c = dragCut > 0 ? dragCut : hoverCut;
        if (c < prog.numSlots)
        {
            const float x = xForStep (prog.slots[(size_t) c].start);
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (x - 1.5f, 22.0f, 3.0f, (float) getHeight() - 26.0f, 1.5f);
        }
    }

    if (playhead >= 0.0)
    {
        const float x = xForStep (playhead);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (x - 1.0f, 14.0f, 2.0f, (float) getHeight() - 14.0f);
    }
}

void TimelineView::mouseMove (const juce::MouseEvent& e)
{
    const int cut = cutAt ((float) e.x);
    const int slot = slotAt ((float) e.x);
    const bool lock = cut < 0 && lockBounds (slot).expanded (5.0f).contains (e.position);
    if (cut != hoverCut || slot != hoverSlot || lock != hoverLock)
    {
        hoverCut = cut;
        hoverSlot = slot;
        hoverLock = lock;
        setMouseCursor (cut > 0 ? juce::MouseCursor::LeftRightResizeCursor
                                : (lock ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
        repaint();
    }
}

void TimelineView::mouseExit (const juce::MouseEvent&)
{
    hoverCut = hoverSlot = -1;
    hoverLock = false;
    repaint();
}

void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    const int cut = cutAt ((float) e.x);
    const int slot = slotAt ((float) e.x);

    if (e.mods.isPopupMenu())
    {
        showSlotMenu (slot, stepForX ((float) e.x));
        return;
    }

    if (cut > 0)
    {
        dragCut = cut;
        processor.beginGesture();
        return;
    }

    if (lockBounds (slot).expanded (5.0f).contains (e.position))
    {
        processor.edit ([slot] (cs::Progression& p, cs::Rng&)
        {
            if (slot < p.numSlots)
                p.slots[(size_t) slot].locked = ! p.slots[(size_t) slot].locked;
        });
        return;
    }

    selected = slot;
    processor.auditionSlot (slot);
    repaint();
}

void TimelineView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCut <= 0)
        return;
    const int cut = dragCut;
    const int step = stepForX ((float) e.x);
    processor.edit ([cut, step] (cs::Progression& p, cs::Rng&) { cs::moveCut (p, cut, step); }, false);
}

void TimelineView::mouseUp (const juce::MouseEvent&)
{
    dragCut = -1;
    repaint();
}

void TimelineView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int cut = cutAt ((float) e.x);
    if (cut > 0)
    {
        processor.edit ([cut] (cs::Progression& p, cs::Rng&) { cs::removeSlot (p, cut); });
        return;
    }
    const int slot = slotAt ((float) e.x);
    const int step = stepForX ((float) e.x);
    processor.edit ([slot, step] (cs::Progression& p, cs::Rng&) { cs::splitSlot (p, slot, step); });
}

void TimelineView::showSlotMenu (int slot, int step)
{
    if (slot < 0 || slot >= prog.numSlots)
        return;

    const auto alts = cs::alternatives (prog, slot, 10);
    juce::PopupMenu menu;
    menu.addSectionHeader ("Replace with (best fit first)");
    for (int i = 0; i < (int) alts.size(); ++i)
    {
        const auto m = cs::chordMetrics (alts[(size_t) i], prog.tonic, prog.scale);
        const juce::String label = utf8 (cs::chordName (alts[(size_t) i], prog.tonic, prog.scale)) + "    "
                                   + utf8 (cs::romanNumeral (alts[(size_t) i], prog.tonic))
                                   + "    bright " + juce::String (juce::roundToInt (m.brightness * 100.0f))
                                   + "  tense " + juce::String (juce::roundToInt (m.tension * 100.0f));
        menu.addItem (100 + i, label);
    }
    menu.addSeparator();

    // Bass note submenu
    juce::PopupMenu bassMenu;
    const cs::Chord curChord = prog.slots[(size_t) slot].chord;
    const auto& t = cs::chordTypes()[(size_t) curChord.type];
    const int currentBassInt = curChord.bassInt;
    const juce::String rootNoteName = utf8 (cs::pitchClassName (curChord.root, prog.tonic, prog.scale));
    bassMenu.addItem (200, "Root: " + rootNoteName + " (root position)", true, currentBassInt == 0);

    std::vector<int> bassOptions;
    int optIdx = 0;
    for (int iv : t.iv)
    {
        const int ivNorm = cs::mod12 (iv);
        if (ivNorm == 0)
            continue;
        if (std::find (bassOptions.begin(), bassOptions.end(), ivNorm) != bassOptions.end())
            continue;
        bassOptions.push_back (ivNorm);

        const int notePc = cs::mod12 (curChord.root + ivNorm);
        const juce::String noteName = utf8 (cs::pitchClassName (notePc, prog.tonic, prog.scale));
        juce::String desc;
        if (ivNorm == 3 || ivNorm == 4)
            desc = "3rd (1st inv)";
        else if (ivNorm == 6 || ivNorm == 7 || ivNorm == 8)
            desc = "5th (2nd inv)";
        else if (ivNorm == 9 || ivNorm == 10 || ivNorm == 11)
            desc = "7th (3rd inv)";
        else if (ivNorm == 2)
            desc = "9th";
        else if (ivNorm == 5)
            desc = "4th/11th";
        else
            desc = juce::String (ivNorm) + " semitones";

        bassMenu.addItem (201 + optIdx, desc + ": " + noteName, true, currentBassInt == ivNorm);
        ++optIdx;
    }
    menu.addSubMenu ("Bass note", bassMenu);

    menu.addItem (1, prog.slots[(size_t) slot].locked ? "Unlock slot" : "Lock slot");
    menu.addItem (2, "Split here", prog.numSlots < cs::kMaxSlots);
    menu.addItem (3, "Merge with previous", slot > 0);

    juce::Component::SafePointer<TimelineView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withParentComponent (getTopLevelComponent()).withMousePosition(),
                        [safe, slot, step, alts, bassOptions] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto& proc = safe->processor;
        if (result >= 100 && result - 100 < (int) alts.size())
        {
            const cs::Chord c = alts[(size_t) (result - 100)];
            proc.edit ([slot, c] (cs::Progression& p, cs::Rng& rng) { cs::setChord (p, slot, c, rng); });
            proc.auditionSlot (slot);
        }
        else if (result == 200)
        {
            proc.edit ([slot] (cs::Progression& p, cs::Rng&) { cs::setBass (p, slot, 0); });
            proc.auditionSlot (slot);
        }
        else if (result >= 201 && result - 201 < (int) bassOptions.size())
        {
            const int bInt = bassOptions[(size_t) (result - 201)];
            proc.edit ([slot, bInt] (cs::Progression& p, cs::Rng&) { cs::setBass (p, slot, bInt); });
            proc.auditionSlot (slot);
        }
        else if (result == 1)
            proc.edit ([slot] (cs::Progression& p, cs::Rng&) { if (slot < p.numSlots) p.slots[(size_t) slot].locked = ! p.slots[(size_t) slot].locked; });
        else if (result == 2)
            proc.edit ([slot, step] (cs::Progression& p, cs::Rng&) { cs::splitSlot (p, slot, step); });
        else if (result == 3)
            proc.edit ([slot] (cs::Progression& p, cs::Rng&) { cs::removeSlot (p, slot); });
    });
}

//==============================================================================
LanesView::LanesView (ChordSlotsProcessor& p) : processor (p) {}

void LanesView::setProgression (const cs::Progression& p)
{
    prog = p;
    repaint();
}

juce::Rectangle<float> LanesView::laneBounds (int lane) const
{
    const float gap = 8.0f;
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

            g.setColour (ui::panel);
            g.fillRoundedRectangle (col, 5.0f);

            const float target = targetOf (s, lane);
            const float actual = actualOf (s, lane);
            const float alpha = s.locked ? 0.45f : 1.0f;
            const auto usable = col.reduced (0.0f, 3.0f);
            const float ty = usable.getBottom() - target * usable.getHeight();

            {
                juce::Graphics::ScopedSaveState state (g);
                juce::Path clip;
                clip.addRoundedRectangle (col, 5.0f);
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
        g.setFont (ui::font (10.5f, true));
        g.drawText (laneNames[lane], lb.reduced (9.0f, 5.0f).toNearestInt(), juce::Justification::topLeft, false);
    }
}

void LanesView::applyDrag (const juce::MouseEvent& e)
{
    if (dragLane < 0)
        return;
    const auto lb = laneBounds (dragLane).reduced (0.0f, 3.0f);
    const float value = juce::jlimit (0.0f, 1.0f, (lb.getBottom() - e.position.y) / lb.getHeight());
    const double step = (double) e.position.x / juce::jmax (1, getWidth()) * prog.lengthSteps();

    int slot = 0;
    for (int i = prog.numSlots - 1; i >= 0; --i)
        if (step >= prog.slots[(size_t) i].start)
        {
            slot = i;
            break;
        }

    editedMask |= 1u << slot;
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
    // Releasing the mouse makes the edited slots follow their new targets;
    // everything else stays exactly as it was.
    processor.edit ([mask, energy] (cs::Progression& p, cs::Rng& rng)
    {
        if (energy)
            cs::applyEnergy (p, rng);
        else
            cs::rerollSlots (p, mask, rng);
    }, false);
}

//==============================================================================
MidiDragTile::MidiDragTile (ChordSlotsProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void MidiDragTile::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (isMouseOver() ? ui::panelHi.brighter (0.1f) : ui::panelHi);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ui::outline);
    const float dashes[] = { 4.0f, 3.0f };
    juce::Path outlinePath, dashed;
    outlinePath.addRoundedRectangle (r, 6.0f);
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outlinePath, dashes, 2);
    g.setColour (ui::textDim);
    g.fillPath (dashed);

    // Tiny piano-roll glyph
    const auto icon = r.withSizeKeepingCentre (26.0f, 16.0f).translated (0.0f, -9.0f);
    g.setColour (ui::accent.brighter (0.2f));
    g.fillRoundedRectangle (icon.getX(), icon.getY(), 12.0f, 3.5f, 1.5f);
    g.fillRoundedRectangle (icon.getX() + 7.0f, icon.getY() + 6.0f, 14.0f, 3.5f, 1.5f);
    g.fillRoundedRectangle (icon.getX() + 14.0f, icon.getY() + 12.0f, 12.0f, 3.5f, 1.5f);

    g.setColour (ui::text);
    g.setFont (ui::font (12.5f, true));
    g.drawText ("Drag MIDI", r.withTrimmedTop (r.getHeight() * 0.55f).toNearestInt(), juce::Justification::centred, false);
}

void MidiDragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;
    const juce::File file = processor.writeTempMidiFile();
    if (file.existsAsFile())
    {
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }
}

//==============================================================================
ChordSlotsEditor::ChordSlotsEditor (ChordSlotsProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      timeline (p),
      lanes (p),
      dragTile (p),
      gateAttachment (p.apvts, "gate", gate),
      volumeAttachment (p.apvts, "synthVol", volume)
{
    setLookAndFeel (&lnf);

    setupCombo (keyBox, keyLabel, "KEY");
    setupCombo (scaleBox, scaleLabel, "SCALE");
    setupCombo (barsBox, barsLabel, "BARS");
    setupCombo (slotsBox, slotsLabel, "SLOTS");
    setupCombo (snapBox, snapLabel, "CUT GRID");
    setupCombo (moodBox, moodLabel, "MOOD (TARGET CURVES)");

    for (int i = 0; i < cs::numScales(); ++i)
        scaleBox.addItem (cs::scales()[(size_t) i].name, i + 1);
    for (int b : { 1, 2, 4, 8 })
        barsBox.addItem (juce::String (b), b);
    for (int i = 1; i <= cs::kMaxSlots; ++i)
        slotsBox.addItem (juce::String (i), i);
    snapBox.addItem ("1/4", 4);
    snapBox.addItem ("1/8", 2);
    snapBox.addItem ("1/16", 1);
    for (int i = 0; i < (int) cs::moods().size(); ++i)
        moodBox.addItem (cs::moods()[(size_t) i].name, i + 1);
    moodBox.setTextWhenNothingSelected ("Custom");

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
                cs::applyMood (pr, pr.mood, false); // re-spread the curves over the new slot count
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

    fitCaption.setText ("FIT", juce::dontSendNotification);
    fitCaption.setFont (ui::font (10.5f, true));
    fitCaption.setColour (juce::Label::textColourId, ui::textDim);
    fitCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fitCaption);
    fitLabel.setFont (ui::font (26.0f, true));
    fitLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fitLabel);

    addAndMakeVisible (timeline);
    addAndMakeVisible (lanes);

    lockCaption.setText ("LOCK  (survives Roll)", juce::dontSendNotification);
    lockCaption.setFont (ui::font (10.5f, true));
    lockCaption.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (lockCaption);

    struct LockDef
    {
        juce::TextButton* button;
        bool cs::Locks::* field;
    };
    for (const LockDef& d : { LockDef { &lockRhythm, &cs::Locks::rhythm }, LockDef { &lockRoots, &cs::Locks::roots },
                              LockDef { &lockColors, &cs::Locks::colors }, LockDef { &lockVoicing, &cs::Locks::voicing } })
    {
        d.button->setClickingTogglesState (true);
        auto* button = d.button;
        const auto field = d.field;
        button->onClick = [this, button, field]
        {
            const bool on = button->getToggleState();
            processor.edit ([field, on] (cs::Progression& pr, cs::Rng&) { pr.locks.*field = on; }, false);
        };
        addAndMakeVisible (*button);
    }

    setupKnob (adventure, adventureLabel, "ADVENTURE");
    setupKnob (syncopation, syncopationLabel, "SYNCOPATION");
    setupKnob (bassMotion, bassMotionLabel, "BASS MOTION");
    setupKnob (gate, gateLabel, "GATE");
    setupKnob (volume, volumeLabel, "SYNTH VOL");
    adventure.setRange (0.0, 1.0, 0.01);
    syncopation.setRange (0.0, 1.0, 0.01);
    bassMotion.setRange (0.0, 1.0, 0.01);
    adventure.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    syncopation.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    bassMotion.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    gate.setColour (juce::Slider::rotarySliderFillColourId, ui::textDim);
    volume.setColour (juce::Slider::rotarySliderFillColourId, ui::textDim);

    adventure.onValueChange = [this]
    {
        if (refreshing) return;
        const float v = (float) adventure.getValue();
        processor.edit ([v] (cs::Progression& pr, cs::Rng&) { pr.adventure = v; }, false);
    };
    syncopation.onValueChange = [this]
    {
        if (refreshing) return;
        const float v = (float) syncopation.getValue();
        processor.edit ([v] (cs::Progression& pr, cs::Rng&) { pr.syncopation = v; }, false);
    };
    bassMotion.onValueChange = [this]
    {
        if (refreshing) return;
        const float v = (float) bassMotion.getValue();
        processor.edit ([v] (cs::Progression& pr, cs::Rng&) { pr.bassMotion = v; }, false);
    };

    rollButton.getProperties().set ("primary", true);
    rollButton.onClick = [this] { processor.edit ([] (cs::Progression& pr, cs::Rng& rng) { cs::roll (pr, rng); }); };
    undoButton.onClick = [this] { processor.undo(); };
    playButton.onClick = [this] { processor.setPreviewPlaying (! processor.isPreviewPlaying()); };
    saveButton.onClick = [this] { saveMidi(); };
    for (auto* b : { &undoButton, &rollButton, &playButton, &saveButton })
        addAndMakeVisible (*b);
    addAndMakeVisible (dragTile);

    hint.setText (juce::String::fromUTF8 ("Click a slot to hear it  \xc2\xb7  right-click for alternatives  \xc2\xb7  drag the edges to move cuts  \xc2\xb7  "
                                          "double-click to split or merge  \xc2\xb7  drag the bars to set targets (white marks = what the chord measures)"),
                  juce::dontSendNotification);
    hint.setFont (ui::font (11.5f));
    hint.setColour (juce::Label::textColourId, ui::textDim);
    hint.setJustificationType (juce::Justification::centredLeft);
    hint.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (hint);

    processor.addChangeListener (this);
    refresh();

    setResizable (true, true);
    setResizeLimits (860, 540, 1800, 1100);
    setSize (980, 620);
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
    label.setFont (ui::font (10.5f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void ChordSlotsEditor::setupKnob (juce::Slider& s, juce::Label& label, const juce::String& caption)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (s);
    label.setText (caption, juce::dontSendNotification);
    label.setFont (ui::font (10.0f, true));
    label.setColour (juce::Label::textColourId, ui::textDim);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
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
    slotsBox.setSelectedId (p.numSlots, juce::dontSendNotification);
    snapBox.setSelectedId (p.snap, juce::dontSendNotification);
    moodBox.setSelectedId (p.mood + 1, juce::dontSendNotification);

    lockRhythm.setToggleState (p.locks.rhythm, juce::dontSendNotification);
    lockRoots.setToggleState (p.locks.roots, juce::dontSendNotification);
    lockColors.setToggleState (p.locks.colors, juce::dontSendNotification);
    lockVoicing.setToggleState (p.locks.voicing, juce::dontSendNotification);
    adventure.setValue (p.adventure, juce::dontSendNotification);
    syncopation.setValue (p.syncopation, juce::dontSendNotification);
    bassMotion.setValue (p.bassMotion, juce::dontSendNotification);

    const int fit = juce::roundToInt (100.0f * cs::fitScore (p));
    fitLabel.setText (juce::String (fit) + "%", juce::dontSendNotification);
    fitLabel.setColour (juce::Label::textColourId, fit >= 85 ? ui::energy : (fit >= 70 ? ui::brightness : ui::tension));

    undoButton.setEnabled (processor.canUndo());
    timeline.setProgression (p);
    lanes.setProgression (p);
}

void ChordSlotsEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void ChordSlotsEditor::timerCallback()
{
    const double pos = processor.getPlayPosition();
    timeline.setPlayhead (pos);
    const juce::String text = processor.isPreviewPlaying() ? "Stop" : "Play";
    if (playButton.getButtonText() != text)
        playButton.setButtonText (text);
    playButton.setToggleState (processor.isPreviewPlaying(), juce::dontSendNotification);
}

void ChordSlotsEditor::saveMidi()
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile (juce::File::createLegalFileName ("ChordSlots " + processor.describeProgression()) + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Save progression as MIDI", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
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

void ChordSlotsEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);
    g.setColour (ui::text);
    g.setFont (ui::font (22.0f, true));
    g.drawText ("ChordSlots", 18, 16, 150, 30, juce::Justification::centredLeft);
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.5f));
    g.drawText ("target-driven harmony", 19, 42, 150, 14, juce::Justification::centredLeft);
}

void ChordSlotsEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    // Header
    auto header = area.removeFromTop (52);
    header.removeFromLeft (160);
    auto fitArea = header.removeFromRight (86);
    fitCaption.setBounds (fitArea.removeFromTop (14));
    fitLabel.setBounds (fitArea);

    auto place = [&header] (juce::ComboBox& box, juce::Label& label, int width)
    {
        auto cell = header.removeFromLeft (width);
        header.removeFromLeft (10);
        label.setBounds (cell.removeFromTop (16));
        box.setBounds (cell.removeFromTop (28));
    };
    const int flexible = juce::jmax (0, header.getWidth() - 6 * 10 - (66 + 78 + 60 + 60 + 78 + 130));
    place (keyBox, keyLabel, 66);
    place (scaleBox, scaleLabel, 150 + flexible / 3 - 72);
    place (barsBox, barsLabel, 60);
    place (slotsBox, slotsLabel, 60);
    place (snapBox, snapLabel, 78);
    place (moodBox, moodLabel, juce::jmax (130, header.getWidth() - 10));

    area.removeFromTop (12);

    // Footer
    hint.setBounds (area.removeFromBottom (18));
    area.removeFromBottom (8);
    auto footer = area.removeFromBottom (78);
    area.removeFromBottom (14);

    auto locks = footer.removeFromLeft (300);
    lockCaption.setBounds (locks.removeFromTop (16));
    locks.removeFromTop (4);
    locks = locks.removeFromTop (34);
    const int lw = (locks.getWidth() - 18) / 4;
    for (auto* b : { &lockRhythm, &lockRoots, &lockColors, &lockVoicing })
    {
        b->setBounds (locks.removeFromLeft (lw));
        locks.removeFromLeft (6);
    }
    footer.removeFromLeft (14);

    auto right = footer.removeFromRight (juce::jmin (430, footer.getWidth() - 4 * 66));
    auto knobs = footer;
    const int kw = knobs.getWidth() / 5;
    struct Knob { juce::Slider* s; juce::Label* l; };
    for (const Knob& k : { Knob { &adventure, &adventureLabel }, Knob { &syncopation, &syncopationLabel },
                           Knob { &bassMotion, &bassMotionLabel },
                           Knob { &gate, &gateLabel }, Knob { &volume, &volumeLabel } })
    {
        auto cell = knobs.removeFromLeft (kw);
        k.l->setBounds (cell.removeFromBottom (14));
        k.s->setBounds (cell.reduced (2, 0));
    }

    right.removeFromLeft (10);
    dragTile.setBounds (right.removeFromRight (92).reduced (0, 4));
    right.removeFromRight (8);
    auto small = right.removeFromRight (84).reduced (0, 4);
    saveButton.setBounds (small.removeFromBottom (small.getHeight() / 2).withTrimmedTop (3));
    playButton.setBounds (small.withTrimmedBottom (3));
    right.removeFromRight (8);
    undoButton.setBounds (right.removeFromLeft (62).reduced (0, 18));
    right.removeFromLeft (8);
    rollButton.setBounds (right.reduced (0, 4));

    // Main: slots on top, target lanes below, sharing the same time axis
    timeline.setBounds (area.removeFromTop (juce::jlimit (120, 190, area.getHeight() * 2 / 5)));
    area.removeFromTop (12);
    lanes.setBounds (area);
}
