// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LookAndFeel.h"

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

} // namespace ui

//==============================================================================
ChordSlotsLookAndFeel::ChordSlotsLookAndFeel()
{
    setWorkspaceAccent (ui::harmonyAccent);
    setColour (juce::ResizableWindow::backgroundColourId, ui::background);
    setColour (juce::ComboBox::backgroundColourId, ui::panel);
    setColour (juce::ComboBox::outlineColourId, ui::outline);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::textDim);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::headerTextColourId, ui::textDim);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::text);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::TextEditor::backgroundColourId, ui::panelHi);
    setColour (juce::TextEditor::textColourId, ui::text);
}

void ChordSlotsLookAndFeel::setWorkspaceAccent (juce::Colour colour)
{
    setColour (ui::workspaceAccentColourId, colour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::panel.interpolatedWith (colour, 0.35f));
    setColour (juce::Slider::textBoxHighlightColourId, colour.withAlpha (0.3f));
    setColour (juce::TextEditor::outlineColourId, ui::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, colour);
}

void ChordSlotsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    if (b.getProperties().contains ("workspaceTabAccent"))
    {
        const auto accent = juce::Colour::fromString (b.getProperties()["workspaceTabAccent"].toString());
        const auto bounds = b.getLocalBounds().toFloat();
        const bool active = b.getToggleState();
        if (active || over || down)
        {
            const auto surface = active ? bounds : bounds.withTrimmedBottom (1.0f);
            juce::Path shape;
            shape.addRoundedRectangle (surface.getX(), surface.getY(), surface.getWidth(), surface.getHeight(),
                                       6.0f, 6.0f, true, true, false, false);
            g.setGradientFill (juce::ColourGradient::vertical (
                ui::panel.interpolatedWith (accent, active ? 0.26f : 0.13f), surface.getY(),
                active ? ui::background : ui::panel, surface.getBottom()));
            g.fillPath (shape);
        }

        const auto indicator = bounds.withTrimmedTop (3.0f).withHeight (active ? 2.0f : 1.0f)
                                     .withSizeKeepingCentre (active ? 34.0f : 18.0f, active ? 2.0f : 1.0f);
        g.setColour (accent.withAlpha (active || over ? 1.0f : 0.60f));
        g.fillRoundedRectangle (indicator, 1.0f);

        if (b.hasKeyboardFocus (false))
        {
            g.setColour (accent);
            g.drawRoundedRectangle (bounds.reduced (2.0f), 4.0f, 1.0f);
        }
        return;
    }

    const auto r = b.getLocalBounds().toFloat().reduced (0.75f);
    const auto accent = b.findColour (ui::workspaceAccentColourId);
    const bool primary = b.getProperties().getWithDefault ("primary", false);
    const bool quiet = b.getProperties().getWithDefault ("quiet", false);
    const bool selected = b.getToggleState();
    auto fill = primary ? accent : selected ? ui::panel.interpolatedWith (accent, 0.17f) : ui::panelHi;
    if (down)
        fill = fill.darker (0.17f);
    else if (over)
        fill = fill.brighter (0.09f);
    else if (! primary && ! selected && (bool) b.getProperties().getWithDefault ("safeNote", false))
        fill = fill.interpolatedWith (ui::energy, 0.16f);

    const float alpha = b.isEnabled() ? 1.0f : 0.38f;
    if (! quiet || selected || over || down)
    {
        g.setColour (fill.withAlpha (alpha));
        g.fillRoundedRectangle (r, 6.0f);
        if (! primary)
        {
            g.setColour ((selected || over ? accent : ui::outline).withAlpha (alpha));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
    }
    if (b.hasKeyboardFocus (false))
    {
        g.setColour (ui::text);
        g.drawRoundedRectangle (r.reduced (1.5f), 4.0f, 1.0f);
    }
}

void ChordSlotsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool over, bool down)
{
    if (button.getProperties().contains ("workspaceTabAccent"))
    {
        const auto accent = juce::Colour::fromString (button.getProperties()["workspaceTabAccent"].toString());
        const bool active = button.getToggleState();
        g.setColour (active ? accent.interpolatedWith (ui::text, 0.32f) : over || down ? ui::text : ui::textDim);
        g.setFont (ui::font (14.0f, active));
        g.drawText (button.getButtonText(), button.getLocalBounds().withTrimmedTop (4),
                    juce::Justification::centred);
        return;
    }

    const bool primary = button.getProperties().getWithDefault ("primary", false);
    const auto ink = (primary ? ui::background : button.findColour (button.getToggleState()
                                      ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId))
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);
    const auto icon = static_cast<ui::ButtonIcon> ((int) button.getProperties().getWithDefault ("icon", 0));
    const auto font = getTextButtonFont (button, button.getHeight());
    if (icon == ui::ButtonIcon::None)
    {
        if (! primary)
        {
            juce::LookAndFeel_V4::drawButtonText (g, button, over, down);
            return;
        }
        g.setColour (ink);
        g.setFont (font);
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0),
                          juce::Justification::centred, 1, 0.7f);
        return;
    }

    constexpr int iconWidth = 12;
    constexpr int gap = 5;
    const bool iconOnly = icon == ui::ButtonIcon::Undo || icon == ui::ButtonIcon::Redo
                       || icon == ui::ButtonIcon::Settings || icon == ui::ButtonIcon::Close
                       || icon == ui::ButtonIcon::Audition;
    const int textWidth = juce::GlyphArrangement::getStringWidthInt (font, button.getButtonText());
    const int contentWidth = iconOnly ? iconWidth : juce::jmin (button.getWidth() - 6, iconWidth + gap + textWidth);

    auto content = button.getLocalBounds().withSizeKeepingCentre (contentWidth, button.getHeight());
    const auto box = content.removeFromLeft (iconWidth).withSizeKeepingCentre (iconWidth, iconWidth);
    const auto position = juce::AffineTransform::translation ((float) box.getX(), (float) box.getY());
    g.setColour (ink);

    juce::Path shape;
    switch (icon)
    {
        case ui::ButtonIcon::Audition:
        case ui::ButtonIcon::Transport:
            if (icon == ui::ButtonIcon::Transport
                && (button.getToggleState() || (bool) button.getProperties().getWithDefault ("auditionPlaying", false)))
                shape.addRoundedRectangle (2.0f, 2.0f, 8.0f, 8.0f, 1.0f);
            else
                shape.addTriangle (2.5f, 1.0f, 10.5f, 6.0f, 2.5f, 11.0f);
            break;
        case ui::ButtonIcon::Save:
            shape.startNewSubPath (1.0f, 8.0f);
            shape.lineTo (1.0f, 11.0f);
            shape.lineTo (11.0f, 11.0f);
            shape.lineTo (11.0f, 8.0f);
            shape.startNewSubPath (6.0f, 1.0f);
            shape.lineTo (6.0f, 7.5f);
            shape.startNewSubPath (3.5f, 5.0f);
            shape.lineTo (6.0f, 7.5f);
            shape.lineTo (8.5f, 5.0f);
            break;
        case ui::ButtonIcon::Undo:
            shape.startNewSubPath (4.5f, 1.0f);
            shape.lineTo (1.0f, 4.5f);
            shape.lineTo (4.5f, 8.0f);
            shape.startNewSubPath (1.0f, 4.5f);
            shape.lineTo (7.0f, 4.5f);
            shape.cubicTo (12.0f, 4.5f, 12.0f, 11.0f, 7.0f, 11.0f);
            break;
        case ui::ButtonIcon::Redo: // Undo mirrored horizontally
            shape.startNewSubPath (8.5f, 1.0f);
            shape.lineTo (12.0f, 4.5f);
            shape.lineTo (8.5f, 8.0f);
            shape.startNewSubPath (12.0f, 4.5f);
            shape.lineTo (6.0f, 4.5f);
            shape.cubicTo (1.0f, 4.5f, 1.0f, 11.0f, 6.0f, 11.0f);
            break;
        case ui::ButtonIcon::Settings:
            for (int i = 0; i < 3; ++i)
            {
                const float y = 2.0f + 4.0f * (float) i;
                const float x = i == 1 ? 8.5f : 3.5f;
                shape.startNewSubPath (0.5f, y);
                shape.lineTo (11.5f, y);
                shape.startNewSubPath (x, y - 1.6f);
                shape.lineTo (x, y + 1.6f);
            }
            break;
        case ui::ButtonIcon::Close:
            shape.startNewSubPath (2.5f, 2.5f);
            shape.lineTo (9.5f, 9.5f);
            shape.startNewSubPath (9.5f, 2.5f);
            shape.lineTo (2.5f, 9.5f);
            break;
        case ui::ButtonIcon::Import:
            shape.startNewSubPath (1.0f, 8.0f);
            shape.lineTo (1.0f, 11.0f);
            shape.lineTo (11.0f, 11.0f);
            shape.lineTo (11.0f, 8.0f);
            shape.startNewSubPath (6.0f, 8.0f);
            shape.lineTo (6.0f, 1.0f);
            shape.startNewSubPath (3.5f, 3.5f);
            shape.lineTo (6.0f, 1.0f);
            shape.lineTo (8.5f, 3.5f);
            break;
        case ui::ButtonIcon::Lock:
            ui::drawPadlock (g, box.toFloat().reduced (1.0f, 0.0f), ink, button.getToggleState());
            break;
        case ui::ButtonIcon::Dice:
            shape.addRoundedRectangle (0.85f, 0.85f, 10.3f, 10.3f, 2.3f);
            for (float pip : { 3.6f, 6.0f, 8.4f })
                g.fillEllipse ((float) box.getX() + pip - 0.8f, (float) box.getY() + pip - 0.8f, 1.6f, 1.6f);
            break;
        case ui::ButtonIcon::None:
            break;
    }

    if (icon == ui::ButtonIcon::Transport || icon == ui::ButtonIcon::Audition)
        g.fillPath (shape, position);
    else
        g.strokePath (shape, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      position);

    if (! iconOnly)
    {
        g.setFont (font);
        g.drawFittedText (button.getButtonText(), content.withTrimmedLeft (gap), juce::Justification::centred, 1, 0.68f);
    }
}

juce::Font ChordSlotsLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const bool primary = b.getProperties().getWithDefault ("primary", false);
    return ui::font (primary ? juce::jmin (16.0f, (float) h * 0.45f) : 13.0f, primary);
}

void ChordSlotsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thickness = juce::jmax (2.5f, radius * 0.20f);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f, 0.0f, startAngle, angle, true);

    g.setColour (ui::outline.brighter (0.22f));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto accent = s.findColour (ui::workspaceAccentColourId);
    g.setColour (s.isMouseOverOrDragging() || s.hasKeyboardFocus (true)
                   ? accent : ui::outline.interpolatedWith (accent, 0.67f));
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - thickness - 3.0f;
    if (inner > 3.0f)
    {
        g.setColour (ui::panel.brighter (0.08f));
        g.fillEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (centre));
        g.setColour (ui::text);
        juce::Path tick;
        tick.addRoundedRectangle (-1.2f, -inner + 1.0f, 2.4f, inner * 0.68f, 1.2f);
        g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }
}

void ChordSlotsLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                             float minPos, float maxPos, juce::Slider::SliderStyle style,
                                             juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal || w <= 0 || h <= 0)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, slider);
        return;
    }

    const auto start = static_cast<float> (x);
    const auto width = static_cast<float> (w);
    const float centreY = static_cast<float> (y) + static_cast<float> (h) * 0.5f;
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float origin = bipolar ? start + width * static_cast<float> (slider.valueToProportionOfLength (0.0)) : start;
    const bool enabled = slider.isEnabled();
    const auto accent = slider.findColour (ui::workspaceAccentColourId);

    g.setColour (ui::outline.brighter (0.22f).withAlpha (enabled ? 1.0f : 0.5f));
    g.fillRoundedRectangle (start, centreY - 2.0f, width, 4.0f, 2.0f);
    g.setColour (accent.withAlpha (enabled ? 0.85f : 0.3f));
    g.fillRoundedRectangle (juce::jmin (origin, pos), centreY - 2.0f, std::abs (pos - origin), 4.0f, 2.0f);

    if (bipolar)
    {
        g.setColour (ui::textDim.withAlpha (enabled ? 0.9f : 0.4f));
        g.fillRect (origin - 0.5f, centreY - 7.0f, 1.0f, 14.0f);
    }

    const juce::Rectangle<float> thumb (pos - 4.5f, centreY - 4.5f, 9.0f, 9.0f);
    g.setColour (enabled ? accent : ui::textMuted);
    g.fillEllipse (thumb);
    if (enabled && (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (true)))
    {
        g.setColour (ui::text);
        g.drawEllipse (thumb.expanded (1.0f), 1.0f);
    }
}

void ChordSlotsLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (w, h).toFloat().reduced (0.75f);
    g.setColour (ui::background);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) || box.isMouseOver (true)
                   ? box.findColour (ui::workspaceAccentColourId) : ui::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 3.5f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 3.5f, cy - 2.0f);
    g.setColour (ui::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

juce::Label* ChordSlotsLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    if (slider.getTextBoxPosition() == juce::Slider::TextBoxRight)
    {
        label->setJustificationType (juce::Justification::centredRight);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    }
    return label;
}

juce::Font ChordSlotsLookAndFeel::getComboBoxFont (juce::ComboBox&) { return ui::font (13.5f); }
juce::Font ChordSlotsLookAndFeel::getPopupMenuFont() { return ui::font (13.5f); }
juce::Font ChordSlotsLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void ChordSlotsLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font (getLabelFont (label));
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);

        auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());

        g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
    }
    else if (label.isEnabled())
    {
        g.setColour (label.findColour (juce::Label::outlineColourId));
    }

    g.drawRoundedRectangle (label.getLocalBounds().toFloat().reduced (0.75f), 5.0f, 1.0f);
}

void ChordSlotsLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (ui::panelHi);
    g.fillRoundedRectangle (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f, 3.0f);
}

void ChordSlotsLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    if (ed.hasKeyboardFocus (true))
    {
        g.setColour (ui::accent);
        g.drawRoundedRectangle (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f, 3.0f, 1.0f);
    }
    else
    {
        g.setColour (ui::outline);
        g.drawRoundedRectangle (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f, 3.0f, 1.0f);
    }
}
