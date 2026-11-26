// ChordSlots - custom look and feel.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
const juce::Colour background { 0xff15171b };
const juce::Colour panel      { 0xff1d2026 };
const juce::Colour panelHi    { 0xff2a2e37 };
const juce::Colour outline    { 0xff343944 };
const juce::Colour text       { 0xffe9ebf2 };
const juce::Colour textDim    { 0xffa0a7bc }; // higher contrast textDim (was 0xff8d93a6)
const juce::Colour textMuted  { 0xff70778c };
const juce::Colour accent     { 0xff7aa2ff };
const juce::Colour harmonyAccent { 0xff9bb5ff };
const juce::Colour feelAccent { 0xff80dcc3 };
const juce::Colour songAccent { 0xffefba7c };
const juce::Colour brightness { 0xffe6b562 };
const juce::Colour tension    { 0xffee9bb0 };
const juce::Colour energy     { 0xff7fd3c4 };

enum class ButtonIcon { None, Transport, Save, Undo, Redo, Settings };

juce::Font font (float height, bool bold = false);
/** Slot colour: deep blue for dark chords, warm gold for bright ones. */
juce::Colour colourForBrightness (float b);
void drawPadlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool closed);
} // namespace ui

//==============================================================================
class ChordSlotsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChordSlotsLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
};
