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
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.3f));
    setColour (juce::TextEditor::backgroundColourId, ui::panelHi);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::outlineColourId, ui::accent);
    setColour (juce::TextEditor::focusedOutlineColourId, ui::accent.brighter (0.2f));
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
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (b.getToggleState() || primary ? ui::accent.brighter (0.3f) : ui::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void ChordSlotsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool over, bool down)
{
    const auto icon = static_cast<ui::ButtonIcon> ((int) button.getProperties().getWithDefault ("icon", 0));
    if (icon == ui::ButtonIcon::None)
    {
        juce::LookAndFeel_V4::drawButtonText (g, button, over, down);
        return;
    }

    const auto font = getTextButtonFont (button, button.getHeight());
    const int textWidth = juce::GlyphArrangement::getStringWidthInt (font, button.getButtonText());
    constexpr int iconWidth = 12;
    constexpr int gap = 5;
    const int contentWidth = iconWidth + gap + textWidth;

    if (contentWidth + 8 > button.getWidth())
    {
        juce::LookAndFeel_V4::drawButtonText (g, button, over, down);
        return;
    }

    auto content = button.getLocalBounds().withSizeKeepingCentre (contentWidth, button.getHeight());
    const auto box = content.removeFromLeft (iconWidth).withSizeKeepingCentre (iconWidth, iconWidth);
    const auto position = juce::AffineTransform::translation ((float) box.getX(), (float) box.getY());
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                           : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

    juce::Path shape;
    switch (icon)
    {
        case ui::ButtonIcon::Transport:
            if (button.getToggleState())
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
        case ui::ButtonIcon::None:
            break;
    }

    if (icon == ui::ButtonIcon::Transport)
        g.fillPath (shape, position);
    else
        g.strokePath (shape, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      position);

    g.setFont (font);
    g.drawText (button.getButtonText(), content.withTrimmedLeft (gap), juce::Justification::centred, false);
}

juce::Font ChordSlotsLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const bool primary = b.getProperties().getWithDefault ("primary", false);
    return ui::font (primary ? juce::jmin (20.0f, (float) h * 0.45f) : 13.0f, primary);
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
    const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 3.5f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 3.5f, cy - 2.0f);
    g.setColour (ui::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
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

    g.drawRect (label.getLocalBounds());
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
