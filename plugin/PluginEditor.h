// ChordSlots - editor UI.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"

namespace ui
{
const juce::Colour background { 0xff12141a };
const juce::Colour panel { 0xff1b1e27 };
const juce::Colour panelHi { 0xff262a36 };
const juce::Colour outline { 0xff343949 };
const juce::Colour text { 0xffe9ebf2 };
const juce::Colour textDim { 0xff8d93a6 };
const juce::Colour accent { 0xff8b7bff };
const juce::Colour brightness { 0xfff3b743 };
const juce::Colour tension { 0xffef5d6e };
const juce::Colour energy { 0xff3ec9bf };

juce::Font font (float height, bool bold = false);
/** Slot colour: deep blue for dark chords, warm gold for bright ones. */
juce::Colour colourForBrightness (float b);
} // namespace ui

//==============================================================================
class ChordSlotsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ChordSlotsLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
/** The slots: one block per chord, cut handles between them. */
class TimelineView final : public juce::Component
{
public:
    explicit TimelineView (ChordSlotsProcessor&);

    void setProgression (const cs::Progression&);
    void setPlayhead (double step);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    float xForStep (double step) const;
    int stepForX (float x) const;
    int cutAt (float x) const;
    int slotAt (float x) const;
    juce::Rectangle<float> slotBounds (int i) const;
    juce::Rectangle<float> lockBounds (int i) const;
    void showSlotMenu (int slot, int step);

    ChordSlotsProcessor& processor;
    cs::Progression prog;
    double playhead = -1.0;
    int hoverSlot = -1, hoverCut = -1, dragCut = -1, selected = -1;
    bool hoverLock = false;
};

//==============================================================================
/** Three lanes of per-slot targets (bars) with the measured value on top. */
class LanesView final : public juce::Component
{
public:
    explicit LanesView (ChordSlotsProcessor&);

    void setProgression (const cs::Progression&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> laneBounds (int lane) const;
    void applyDrag (const juce::MouseEvent&);

    ChordSlotsProcessor& processor;
    cs::Progression prog;
    int dragLane = -1;
    unsigned editedMask = 0;
};

//==============================================================================
/** Drag this onto a DAW track to drop the progression as a MIDI clip. */
class MidiDragTile final : public juce::Component
{
public:
    explicit MidiDragTile (ChordSlotsProcessor&);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    ChordSlotsProcessor& processor;
    bool dragging = false;
};

//==============================================================================
class ChordSlotsEditor final : public juce::AudioProcessorEditor,
                               private juce::ChangeListener,
                               private juce::Timer
{
public:
    explicit ChordSlotsEditor (ChordSlotsProcessor&);
    ~ChordSlotsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void rebuildKeyNames (int scale);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& caption);
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& caption);
    void saveMidi();

    ChordSlotsProcessor& processor;
    ChordSlotsLookAndFeel lnf;

    juce::ComboBox keyBox, scaleBox, barsBox, slotsBox, snapBox, moodBox;
    juce::Label keyLabel, scaleLabel, barsLabel, slotsLabel, snapLabel, moodLabel;
    juce::Label fitLabel, fitCaption;

    TimelineView timeline;
    LanesView lanes;

    juce::TextButton lockRhythm { "Rhythm" }, lockRoots { "Roots" }, lockColors { "Colors" }, lockVoicing { "Voicing" };
    juce::Label lockCaption;
    juce::Slider adventure, syncopation, bassMotion, gate, volume;
    juce::Label adventureLabel, syncopationLabel, bassMotionLabel, gateLabel, volumeLabel;
    juce::TextButton undoButton { "Undo" }, rollButton { "ROLL" }, playButton { "Play" }, saveButton { "Save .mid" };
    MidiDragTile dragTile;
    juce::Label hint;

    juce::AudioProcessorValueTreeState::SliderAttachment gateAttachment, volumeAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    int keyNamesScale = -1;
    bool refreshing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSlotsEditor)
};
