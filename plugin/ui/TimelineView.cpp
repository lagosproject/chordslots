// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TimelineView.h"
#include <numeric>

namespace
{
constexpr int separatorTop = 17;
constexpr int contentTop = 35;
constexpr int pitchColumnWidth = 44;
inline juce::String utf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
}

TimelineView::TimelineView (ChordSlotsProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
}

void TimelineView::setProgression (const cs::Progression& p)
{
    prog = p;
    updateMidiNotes();
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

int TimelineView::timelineLeft() const
{
    return midiVisible ? juce::jmin (pitchColumnWidth, juce::jmax (0, getWidth() - 1)) : 0;
}

float TimelineView::xForStep (double step) const
{
    const int left = timelineLeft();
    return left + (float) (step / (double) prog.lengthSteps()) * (getWidth() - left);
}

double TimelineView::positionForX (float x) const
{
    const int left = timelineLeft();
    return (double) (x - left) / juce::jmax (1, getWidth() - left) * prog.lengthSteps();
}

int TimelineView::stepForX (float x) const
{
    return (int) std::lround (positionForX (x));
}

int TimelineView::cutAt (float x) const
{
    int closest = -1;
    float distance = 5.0f;
    for (int i = 1; i < prog.numSlots; ++i)
    {
        const float candidate = std::abs (x - xForStep (prog.slots[(size_t) i].start));
        if (candidate <= distance)
        {
            closest = i;
            distance = candidate;
        }
    }
    return closest;
}

int TimelineView::slotAt (float x) const
{
    const double step = positionForX (x);
    for (int i = prog.numSlots - 1; i >= 0; --i)
        if (step >= prog.slots[(size_t) i].start)
            return i;
    return 0;
}

juce::Rectangle<float> TimelineView::slotBounds (int i) const
{
    const float x0 = xForStep (prog.slots[(size_t) i].start);
    const float x1 = xForStep (prog.slotEnd (i));
    return { x0 + 1.5f, (float) contentTop, juce::jmax (2.0f, x1 - x0 - 3.0f), (float) getHeight() - contentTop };
}

juce::Rectangle<float> TimelineView::lockBounds (int i) const
{
    const auto r = slotBounds (i);
    return { r.getRight() - 20.0f, r.getY() + 6.0f, 14.0f, 15.0f };
}

juce::Rectangle<float> TimelineView::optionsBounds (int i) const
{
    const auto r = slotBounds (i);
    return { r.getRight() - 20.0f, r.getBottom() - 20.0f, 15.0f, 14.0f };
}

void TimelineView::setMidiVisible (bool show)
{
    midiVisible = show;
    midiHoverY = isMouseOver() ? (float) getMouseXYRelative().y : -1.0f;
    updateMidiNotes();
    repaint();
}

void TimelineView::updateMidiNotes()
{
    midiNotes.clear();
    if (! midiVisible)
        return;

    const auto file = processor.createMidiFile();
    const double scale = (double) cs::kStepsPerQuarter / file.getTimeFormat();
    for (int track = 0; track < file.getNumTracks(); ++track)
        for (const auto* event : *file.getTrack (track))
            if (event->message.isNoteOn() && event->noteOffObject != nullptr)
                midiNotes.push_back ({ event->message.getTimeStamp() * scale,
                                       event->noteOffObject->message.getTimeStamp() * scale,
                                       event->message.getNoteNumber(), event->message.getVelocity(), track == 1 });
}

void TimelineView::paintMidi (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().withTrimmedTop ((float) contentTop + 26.0f);
    if (area.isEmpty())
        return;

    const int left = timelineLeft();
    g.setColour (ui::panel);
    g.fillRect (area);
    g.setColour (ui::background);
    g.fillRect (area.withWidth ((float) left));

    int low = midiNotes.empty() ? 60 : 127;
    int high = midiNotes.empty() ? 71 : 0;
    const bool hasSelection = selected >= 0 && selected < prog.numSlots;
    const int selectionStart = hasSelection ? prog.slots[(size_t) selected].start : 0;
    const int selectionEnd = hasSelection ? prog.slotEnd (selected) : 0;
    std::array<int, 128> labelPriority {};
    for (const auto& note : midiNotes)
    {
        low = std::min (low, note.pitch);
        high = std::max (high, note.pitch);
        const int priority = hasSelection && note.start < selectionEnd && note.end > selectionStart ? 2 : 1;
        auto& rowPriority = labelPriority[(size_t) note.pitch];
        rowPriority = std::max (rowPriority, priority);
    }
    const float rowH = area.getHeight() / (high - low + 1);
    auto yForNote = [&] (int pitch) { return area.getY() + (high - pitch) * rowH; };
    const int hovered = midiHoverY >= area.getY() && midiHoverY < area.getBottom()
        ? juce::jmax (low, high - (int) ((midiHoverY - area.getY()) / rowH)) : -1;
    const auto scale = cs::scaleMask (prog.scale);
    auto noteName = [&] (int pitch)
    {
        return utf8 (cs::pitchClassName (pitch % 12, prog.tonic, prog.scale))
            .replaceCharacter ('b', 0x266d).replaceCharacter ('#', 0x266f)
            + juce::String (pitch / 12 - 1);
    };
    for (int pitch = low; pitch <= high; ++pitch)
    {
        const int pc = pitch % 12;
        const int degree = cs::mod12 (pc - prog.tonic);
        const float y = yForNote (pitch);
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (ui::background.withAlpha (0.6f));
            g.fillRect ((float) left, y, area.getWidth() - left, rowH);
        }
        if ((scale & (1u << degree)) != 0)
        {
            g.setColour (ui::accent.withAlpha (degree == 0 ? 0.28f : 0.12f));
            g.fillRect (0.0f, y, area.getWidth(), rowH);
        }
        if (degree == 0)
        {
            g.setColour (ui::accent.brighter (0.4f).withAlpha (0.85f));
            g.fillRect (0.0f, y, 3.0f, rowH);
        }
    }
    const auto meter = prog.meter();
    const int beat = meter.num == 6 && meter.den == 8 ? 36 : cs::kStepsPerQuarter;
    const int grid = std::gcd (beat, prog.snap);
    for (int step = 0; step < prog.lengthSteps(); step += grid)
    {
        const float alpha = step % meter.stepsPerBar == 0 ? 0.95f : (step % beat == 0 ? 0.6f : 0.22f);
        g.setColour (ui::outline.brighter (0.4f).withAlpha (alpha));
        g.drawVerticalLine ((int) xForStep (step), area.getY(), area.getBottom());
    }
    if (hasSelection)
    {
        g.setColour (ui::accent.withAlpha (0.15f));
        g.fillRect (slotBounds (selected).withTrimmedTop (26.0f));
    }
    if (hovered >= 0)
    {
        g.setColour (ui::text.withAlpha (0.20f));
        g.fillRect (0.0f, yForNote (hovered), area.getWidth(), rowH);
    }

    const auto blockFont = ui::font (9.0f, true);
    g.setFont (blockFont);
    for (const auto& note : midiNotes)
    {
        const float x = xForStep (note.start);
        const auto colour = (note.bass ? ui::energy : ui::brightness)
            .withAlpha (0.4f + 0.6f * note.velocity / 127.0f);
        const float height = rowH * 0.86f;
        const juce::Rectangle<float> block (x, yForNote (note.pitch) + (rowH - height) * 0.5f,
                                            std::max (1.0f, xForStep (note.end) - x), height);
        g.setColour (colour);
        g.fillRect (block);
        if (height >= 10.0f && block.getWidth() >= 12.0f)
        {
            const auto label = noteName (note.pitch);
            if (juce::GlyphArrangement::getStringWidth (blockFont, label) + 8.0f <= block.getWidth())
            {
                const auto background = (note.pitch == hovered ? ui::panel.overlaidWith (ui::text.withAlpha (0.20f))
                                                               : ui::panel).overlaidWith (colour);
                g.setColour (background.contrasting());
                g.drawText (label, block.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
            }
        }
    }

    g.setColour (ui::outline);
    g.drawVerticalLine (left - 1, area.getY(), area.getBottom());
    constexpr float labelHeight = 11.0f;
    if (area.getHeight() < labelHeight)
        return;

    auto labelBounds = [&] (int pitch)
    {
        const float y = juce::jlimit (area.getY(), area.getBottom() - labelHeight,
                                     yForNote (pitch) + (rowH - labelHeight) * 0.5f);
        return juce::Rectangle<float> (0.0f, y, (float) left - 1.0f, labelHeight);
    };
    std::vector<juce::Rectangle<float>> placed;
    auto drawLabel = [&] (int pitch, bool isHovered)
    {
        const auto bounds = labelBounds (pitch);
        if (std::any_of (placed.begin(), placed.end(), [&] (auto other) { return bounds.intersects (other); }))
            return;

        if (isHovered)
        {
            g.setColour (ui::panel.brighter (0.35f));
            g.fillRect (bounds);
        }
        g.setFont (ui::font (10.0f, isHovered || pitch % 12 == prog.tonic));
        g.setColour (ui::text.withAlpha (isHovered || labelPriority[(size_t) pitch] > 0 ? 1.0f : 0.78f));
        g.drawText (noteName (pitch), bounds.withTrimmedLeft (6.0f).withTrimmedRight (6.0f),
                    juce::Justification::centredLeft, false);
        g.fillRect ((float) left - 5.0f, yForNote (pitch) + rowH * 0.5f - 0.5f, 4.0f, 1.0f);
        placed.push_back (bounds);
    };
    if (hovered >= 0)
        drawLabel (hovered, true);
    // When rows are shorter than a label, favor the selected slot, then other sounded pitches.
    for (int priority = 2; priority >= 0; --priority)
        for (int pitch = high; pitch >= low; --pitch)
            if (labelPriority[(size_t) pitch] == priority)
                drawLabel (pitch, false);
}

void TimelineView::paint (juce::Graphics& g)
{
    const int len = prog.lengthSteps();
    const float left = (float) timelineLeft();
    const auto m = prog.meter();
    const int barSteps = m.stepsPerBar;

    // Ruler
    if (m.num == 6 && m.den == 8)
    {
        for (int step = 0; step <= len; step += 12)
        {
            const bool bar = step % barSteps == 0;
            const bool compound = step % 36 == 0;
            const float x = juce::jmin ((float) getWidth() - 0.5f, juce::jmax (left + 0.5f, xForStep (step)));
            g.setColour (bar ? ui::textDim : (compound ? ui::outline.brighter (0.2f) : ui::outline.withAlpha (0.4f)));
            g.drawVerticalLine ((int) x, bar ? 2.0f : (compound ? 7.0f : 11.0f), 15.0f);
            if (bar && step < len)
            {
                g.setFont (ui::font (10.5f));
                g.drawText (juce::String (step / barSteps + 1), (int) x + 4, 0, 30, 13, juce::Justification::centredLeft);
            }
        }
    }
    else
    {
        for (int step = 0; step <= len; step += cs::kStepsPerQuarter)
        {
            const bool bar = step % barSteps == 0;
            const float x = juce::jmin ((float) getWidth() - 0.5f, juce::jmax (left + 0.5f, xForStep (step)));
            g.setColour (bar ? ui::textDim : ui::outline);
            g.drawVerticalLine ((int) x, bar ? 2.0f : 8.0f, 15.0f);
            if (bar && step < len)
            {
                g.setFont (ui::font (10.5f));
                g.drawText (juce::String (step / barSteps + 1), (int) x + 4, 0, 30, 13, juce::Justification::centredLeft);
            }
        }
    }

    if (midiVisible)
    {
        paintMidi (g);
        // Same step transform as the notes, including the pitch-label gutter.
        for (int i = 0; i < prog.numSlots; ++i)
        {
            const auto r = slotBounds (i).withHeight (24.0f);
            g.setColour (i == selected ? ui::accent.withAlpha (0.35f) : ui::panel);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (i == selected ? ui::accent.brighter (0.5f) : ui::outline);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, i == selected ? 2.0f : 1.0f);
            g.setColour (ui::text);
            g.setFont (ui::font (12.0f, true));
            // Readable type with ellipsis; full names remain in tooltip and context bar.
            g.drawText (utf8 (cs::chordName (prog.slots[(size_t) i].chord, prog.tonic, prog.scale)),
                        r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    }

    for (int i = 0; ! midiVisible && i < prog.numSlots; ++i)
    {
        const cs::Slot& s = prog.slots[(size_t) i];
        const auto r = slotBounds (i);
        const juce::Colour fill = ui::colourForBrightness (s.brightness);
        const bool lightText = fill.getPerceivedBrightness() < 0.6f;
        const juce::Colour ink = lightText ? juce::Colours::white : juce::Colour (0xff1a1630);

        g.setColour (i == hoverSlot && hoverCut < 0 ? fill.brighter (0.12f) : fill);
        g.fillRoundedRectangle (r, 6.0f);

        // Tension as a strip along the bottom edge
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 6.0f);
            g.reduceClipRegion (clip);
            g.setColour (ui::tension.withAlpha (0.95f));
            g.fillRect (r.getX(), r.getBottom() - 4.0f, r.getWidth() * s.tension, 4.0f);
        }

        const int triggered = processor.getTriggeredSlot();
        if (i == triggered)
        {
            g.setColour (ui::energy.withAlpha (0.45f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (ui::energy.brighter (0.4f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 2.5f);
        }
        else if (s.locked || i == selected)
        {
            g.setColour (s.locked ? ui::accent.brighter (0.35f) : juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, s.locked ? 2.2f : 1.5f);
        }

        const float rw = r.getWidth();
        const float rh = r.getHeight();

        if (rw > 28.0f)
        {
            // Top row: Roman numeral + Lock icon
            const juce::String roman = utf8 (cs::romanNumeral (s.chord, prog.tonic));
            g.setColour (ink.withAlpha (0.85f));
            g.setFont (ui::font (juce::jlimit (10.0f, 13.0f, rw * 0.15f), true));
            g.drawText (roman, r.getX() + 6.0f, r.getY() + 5.0f, rw - 26.0f, 16.0f, juce::Justification::topLeft, true);

            // Keep the chord lock discoverable without hovering.
            if (rw > 38.0f)
            {
                const float lockAlpha = s.locked ? 1.0f : (hoverLock && i == hoverSlot ? 0.9f : 0.45f);
                ui::drawPadlock (g, lockBounds (i), ink.withAlpha (lockAlpha), s.locked);
            }

            // Middle: Chord Name
            const juce::String name = utf8 (cs::chordName (s.chord, prog.tonic, prog.scale));
            const float nameSize = juce::jlimit (14.0f, 24.0f, rw / juce::jmax (2.0f, (float) name.length() * 0.55f));
            g.setColour (ink);
            g.setFont (ui::font (nameSize, true));
            const float nameY = r.getY() + (rh > 90.0f ? 24.0f : (rh * 0.28f));
            g.drawText (name, r.getX() + 4.0f, nameY, rw - 8.0f, 28.0f, juce::Justification::centred, false);

            // Voiced chord tones (e.g. "C3 G3 B3 E4")
            if (rw > 55.0f && rh > 95.0f && s.voicing.n > 0)
            {
                juce::String noteStr;
                for (int k = 0; k < s.voicing.n; ++k)
                {
                    const int midiNote = s.voicing.notes[(size_t) k];
                    const int pc = cs::mod12 (midiNote);
                    const int oct = midiNote / 12 - 1;
                    if (k > 0) noteStr << " ";
                    noteStr << utf8 (cs::pitchClassName (pc, prog.tonic, prog.scale)) << oct;
                }
                g.setColour (ink.withAlpha (0.75f));
                g.setFont (ui::font (10.5f));
                g.drawText (noteStr, r.getX() + 4.0f, nameY + 26.0f, rw - 8.0f, 15.0f, juce::Justification::centred, true);
            }

            // Bottom: Bass note & Options button
            if (rh > 80.0f)
            {
                juce::String bassStr = "Bass: " + utf8 (cs::pitchClassName (s.chord.bassPc(), prog.tonic, prog.scale));
                if (s.chord.isSlash())
                {
                    const int ivNorm = cs::mod12 (s.chord.bassInt);
                    if (ivNorm == 3 || ivNorm == 4) bassStr << " (1st)";
                    else if (ivNorm == 6 || ivNorm == 7 || ivNorm == 8) bassStr << " (2nd)";
                    else if (ivNorm >= 9 && ivNorm <= 11) bassStr << " (3rd)";
                }

                g.setColour (ink.withAlpha (0.75f));
                g.setFont (ui::font (11.0f));
                g.drawText (bassStr, r.getX() + 6.0f, r.getBottom() - 22.0f, rw - 26.0f, 16.0f, juce::Justification::bottomLeft, true);

                // Options "⋯" button
                if (rw > 45.0f)
                {
                    const auto ob = optionsBounds (i);
                    const bool showOpts = (i == hoverSlot || i == selected);
                    if (showOpts)
                    {
                        g.setColour (ink.withAlpha (hoverOptions && i == hoverSlot ? 0.9f : 0.45f));
                        const float cy = ob.getCentreY();
                        const float cx = ob.getCentreX();
                        g.fillEllipse (cx - 4.5f, cy - 1.5f, 3.0f, 3.0f);
                        g.fillEllipse (cx,         cy - 1.5f, 3.0f, 3.0f);
                        g.fillEllipse (cx + 4.5f, cy - 1.5f, 3.0f, 3.0f);
                    }
                }
            }
        }
    }

    g.setColour (ui::panel);
    g.fillRect (0, separatorTop, getWidth(), contentTop - separatorTop);
    for (int i = 1; i < prog.numSlots; ++i)
    {
        const auto& s = prog.slots[(size_t) i];
        const float x = xForStep (i == dragCut && dragStep >= 0 ? dragStep : s.start);
        const bool highlighted = i == hoverCut || i == dragCut;
        const auto colour = s.cutLocked ? ui::accent.brighter (0.55f) : (highlighted ? ui::text : ui::textDim);
        g.setColour (colour.withAlpha (highlighted ? 0.9f : 0.4f));
        g.drawVerticalLine ((int) x, (float) contentTop, (float) getHeight());
        if (s.cutLocked)
            ui::drawPadlock (g, { x - 5.0f, 20.0f, 10.0f, 12.0f }, colour, true);
        else
        {
            g.setColour (colour);
            g.fillRoundedRectangle (x - 3.0f, 21.0f, 6.0f, 10.0f, 2.0f);
        }
    }

    // Playhead
    if (playhead >= 0.0)
    {
        const float x = xForStep (playhead);
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (getLocalBounds().withTrimmedLeft ((int) left));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (x - 1.0f, 14.0f, 2.0f, (float) getHeight() - 14.0f);
    }
}

juce::String TimelineView::getTooltip()
{
    const auto pos = getMouseXYRelative().toFloat();
    if (pos.x < timelineLeft())
        return {};
    if (pos.y < separatorTop)
        return "Bar and beat positions for the current progression.";

    if (pos.y < contentTop)
    {
        const int cut = cutAt (pos.x);
        if (cut <= 0)
            return "Click between separators to split a chord slot at the nearest grid point.";

        return prog.slots[(size_t) cut].cutLocked
            ? "Separator lock protects this timing boundary. Double-click to unlock it."
            : "Drag to move this timing boundary. Double-click for a separator lock (timing only), or right-click for options.";
    }

    if (midiVisible)
        return utf8 (cs::chordName (prog.slots[(size_t) slotAt (pos.x)].chord, prog.tonic, prog.scale))
            + " — Click to select and hear this slot. Right-click for slot actions.";

    const int slot = slotAt (pos.x);
    const float width = slotBounds (slot).getWidth();
    if (width > 45.0f && optionsBounds (slot).expanded (4.0f).contains (pos))
        return "Open slot actions such as auditioning, replacing or locking the chord.";
    if (width > 38.0f && lockBounds (slot).expanded (4.0f).contains (pos))
        return "Chord lock keeps this slot's chord when rolling. Separator locks protect timing; generation locks apply across the progression.";

    return utf8 (cs::chordName (prog.slots[(size_t) slot].chord, prog.tonic, prog.scale))
        + " — Card color: measured Brightness. Bottom bar: measured Tension. Click for alternatives. Shift-click to hear it; right-click for actions.";
}

void TimelineView::mouseMove (const juce::MouseEvent& e)
{
    midiHoverY = e.position.y;
    const bool overTimeline = e.position.x >= timelineLeft();
    const bool inStrip = overTimeline && e.y >= separatorTop && e.y < contentTop;
    hoverCut = inStrip ? cutAt ((float) e.x) : -1;
    hoverSlot = overTimeline && e.y >= contentTop ? slotAt ((float) e.x) : -1;
    hoverLock = ! midiVisible && hoverSlot >= 0 && lockBounds (hoverSlot).expanded (4.0f).contains (e.position);
    hoverOptions = ! midiVisible && hoverSlot >= 0 && optionsBounds (hoverSlot).expanded (4.0f).contains (e.position);
    setMouseCursor (hoverCut > 0 && ! prog.slots[(size_t) hoverCut].cutLocked
                        ? juce::MouseCursor::LeftRightResizeCursor
                        : (inStrip || hoverSlot >= 0 ? juce::MouseCursor::PointingHandCursor
                                                    : juce::MouseCursor::NormalCursor));
    repaint();
}

void TimelineView::mouseExit (const juce::MouseEvent&)
{
    midiHoverY = -1.0f;
    hoverCut = hoverSlot = -1;
    hoverLock = hoverOptions = false;
    repaint();
}

void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < separatorTop || e.position.x < timelineLeft())
        return;

    const int step = stepForX ((float) e.x);
    const int slot = slotAt ((float) e.x);
    if (e.y < contentTop)
    {
        const int cut = cutAt ((float) e.x);
        if (cut > 0)
        {
            if (e.mods.isPopupMenu())
                showCutMenu (cut);
            else if (! prog.slots[(size_t) cut].cutLocked)
                dragCut = cut;
        }
        else if (e.mods.isLeftButtonDown() && e.getNumberOfClicks() == 1)
        {
            auto trial = prog;
            if (cs::splitSlot (trial, slot, step))
            {
                if (selected > slot)
                    ++selected;
                processor.edit ([slot, step] (cs::Progression& p, cs::Rng&) { cs::splitSlot (p, slot, step); });
                setProgression (processor.getProgression());
            }
        }
        return;
    }

    const float width = slotBounds (slot).getWidth();
    if (e.mods.isPopupMenu()
        || (! midiVisible && width > 45.0f && optionsBounds (slot).expanded (4.0f).contains (e.position)))
    {
        showSlotMenu (slot, step);
        return;
    }

    if (! midiVisible && width > 38.0f && lockBounds (slot).expanded (4.0f).contains (e.position))
    {
        processor.edit ([slot] (cs::Progression& p, cs::Rng&)
        {
            if (slot < p.numSlots)
                p.slots[(size_t) slot].locked = ! p.slots[(size_t) slot].locked;
        });
        return;
    }

    selected = slot;
    if (onSlotSelected)
        onSlotSelected (selected);
    if (midiVisible || e.mods.isShiftDown())
        processor.auditionSlot (slot);
    else if (onOpenAlternatives)
        onOpenAlternatives (slot);
    repaint();
}

void TimelineView::mouseDrag (const juce::MouseEvent& e)
{
    if (midiVisible)
    {
        midiHoverY = getLocalBounds().toFloat().contains (e.position) ? e.position.y : -1.0f;
        repaint();
    }
    if (dragCut <= 0 || dragCut >= prog.numSlots)
        return;

    auto trial = prog;
    cs::moveCut (trial, dragCut, stepForX ((float) e.x));
    dragStep = trial.slots[(size_t) dragCut].start;
    repaint();
}

void TimelineView::mouseUp (const juce::MouseEvent&)
{
    if (dragCut > 0 && dragCut < prog.numSlots && dragStep >= 0
        && dragStep != prog.slots[(size_t) dragCut].start)
        processor.edit ([cut = dragCut, step = dragStep] (cs::Progression& p, cs::Rng&) { cs::moveCut (p, cut, step); });
    dragCut = dragStep = -1;
    repaint();
}

void TimelineView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mouseWasDraggedSinceMouseDown() || e.y < separatorTop || e.y >= contentTop
        || e.position.x < timelineLeft())
        return;

    const int cut = cutAt ((float) e.x);
    if (cut > 0)
        processor.edit ([cut] (cs::Progression& p, cs::Rng&)
        {
            if (cut < p.numSlots)
                p.slots[(size_t) cut].cutLocked = ! p.slots[(size_t) cut].cutLocked;
        });
}

void TimelineView::showCutMenu (int cut)
{
    const bool locked = prog.slots[(size_t) cut].cutLocked;
    juce::PopupMenu menu;
    menu.addItem (1, locked ? "Unlock separator" : "Lock separator");
    menu.addItem (2, "Remove separator", ! locked);
    juce::Component::SafePointer<TimelineView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, cut] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto trial = safe->processor.getProgression();
        if (result == 2 && cs::removeSlot (trial, cut) && safe->selected >= cut)
            --safe->selected;
        safe->processor.edit ([cut, result] (cs::Progression& p, cs::Rng&)
        {
            if (result == 2)
                cs::removeSlot (p, cut);
            else if (cut < p.numSlots)
                p.slots[(size_t) cut].cutLocked = ! p.slots[(size_t) cut].cutLocked;
        });
    });
}

bool TimelineView::keyPressed (const juce::KeyPress& key)
{
    if ((key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L') && selected >= 0)
    {
        const int s = selected;
        processor.edit ([s] (cs::Progression& p, cs::Rng&)
        {
            if (s < p.numSlots)
                p.slots[(size_t) s].locked = ! p.slots[(size_t) s].locked;
        });
        repaint();
        return true;
    }
    return false;
}

void TimelineView::showSlotMenu (int slot, int step)
{
    if (slot < 0 || slot >= prog.numSlots)
        return;

    juce::PopupMenu menu;
    menu.addItem (4, "Chord alternatives");
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
        if (ivNorm == 4 || (ivNorm == 3 && ! cs::hasMajorThird (curChord.type)))
            desc = "3rd (1st inv)";
        else if (ivNorm == 7 || (ivNorm == 6 && ! cs::hasPerfectFifth (curChord.type)) || ivNorm == 8)
            desc = "5th (2nd inv)";
        else if (ivNorm == 10 || ivNorm == 11 || (ivNorm == 9 && cs::seventhKind (curChord.type) == cs::SeventhKind::Diminished7))
            desc = "7th (3rd inv)";
        else if (ivNorm == 2)
            desc = "9th";
        else if (ivNorm == 1)
            desc = "b9th";
        else if (ivNorm == 3)
            desc = "#9th";
        else if (ivNorm == 5)
            desc = "4th/11th";
        else if (ivNorm == 6)
            desc = "#11th";
        else if (ivNorm == 9)
            desc = "6th/13th";
        else
            desc = juce::String (ivNorm) + " semitones";

        bassMenu.addItem (201 + optIdx, desc + ": " + noteName, true, currentBassInt == ivNorm);
        ++optIdx;
    }
    menu.addSubMenu ("Bass note", bassMenu);

    menu.addItem (1, prog.slots[(size_t) slot].locked ? "Unlock chord (L)" : "Lock chord on Roll (L)");
    menu.addItem (2, "Split here", prog.numSlots < cs::kMaxSlots);
    menu.addItem (3, "Merge with previous", slot > 0 && ! prog.slots[(size_t) slot].cutLocked);

    juce::Component::SafePointer<TimelineView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withParentComponent (getTopLevelComponent()).withMousePosition(),
                        [safe, slot, step, bassOptions] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto& proc = safe->processor;
        if (result == 4)
        {
            safe->setSelectedSlot (slot);
            if (safe->onSlotSelected)
                safe->onSlotSelected (slot);
            if (safe->onOpenAlternatives)
                safe->onOpenAlternatives (slot);
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
