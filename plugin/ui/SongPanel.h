// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "../PluginProcessor.h"
#include "LookAndFeel.h"

class SongPanel final : public juce::Component
{
public:
    explicit SongPanel (ChordSlotsProcessor& p) : processor (p)
    {
        for (auto* control : std::initializer_list<juce::Component*> {
                 &title, &empty, &chain, &sectionTitle, &variation, &transpose, &remove, &enable, &add, &clear })
            addAndMakeVisible (*control);

        setComponentID ("song-panel");
        variation.setComponentID ("song-variation");
        transpose.setComponentID ("song-transpose");
        title.setColour (juce::Label::textColourId, ui::songAccent);
        sectionTitle.setColour (juce::Label::textColourId, ui::songAccent);
        title.setFont (ui::font (17.0f, true));
        sectionTitle.setFont (ui::font (16.0f, true));
        empty.setText ("Add a section to arrange your variations.", juce::dontSendNotification);
        empty.setJustificationType (juce::Justification::centred);
        chain.setViewedComponent (&blocks, false);
        chain.setScrollBarsShown (false, true);
        for (size_t i = 0; i < sections.size(); ++i)
        {
            auto& button = sections[i];
            blocks.addChildComponent (button);
            button.setComponentID ("song-section-" + juce::String ((int) i));
            button.onClick = [this, i] { selected = (int) i; refresh(); };
        }
        for (int v = 0; v < cs::kNumVariations; ++v)
            variation.addItem ("Variation " + juce::String::charToString ('A' + v), v + 1);
        transpose.setSliderStyle (juce::Slider::IncDecButtons);
        transpose.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 74, 27);
        transpose.setColour (juce::Slider::textBoxBackgroundColourId, ui::panelHi);
        transpose.setColour (juce::Slider::textBoxOutlineColourId, ui::outline);
        transpose.setRange (cs::kMinSongTranspose, cs::kMaxSongTranspose, 1);
        transpose.setTextValueSuffix (" st");
        transpose.setTooltip ("Shift the selected section in semitones.");
        variation.onChange = [this]
        {
            editSection ([this] (auto& entry) { entry.variation = variation.getSelectedId() - 1; });
        };
        transpose.onValueChange = [this]
        {
            editSection ([this] (auto& entry) { entry.transpose = (int) transpose.getValue(); });
        };
        enable.setTooltip ("Use the Song chain for host playback.");
        enable.onClick = [this] { processor.setSongEnabled (! processor.isSongEnabled()); refresh(); };
        add.onClick = [this]
        {
            auto song = processor.getSong();
            if (song.numEntries >= cs::kMaxSongEntries) return;
            selected = song.numEntries;
            song.entries[song.numEntries++] = { processor.getEditVariation(), 0 };
            processor.setSong (song);
            refresh();
            chain.setViewPosition (sections[(size_t) selected].getX(), 0);
        };
        clear.onClick = [this] { selected = -1; processor.setSong ({}); refresh(); };
        remove.onClick = [this]
        {
            auto song = processor.getSong();
            if (! hasSelection (song.numEntries)) return;
            for (int i = selected; i + 1 < song.numEntries; ++i)
                song.entries[i] = song.entries[i + 1];
            song.entries[--song.numEntries] = {};
            selected = juce::jmin (selected, song.numEntries - 1);
            processor.setSong (song);
            refresh();
        };
        refresh();
    }

    static juce::Result saveMidiTo (const juce::File& file, const juce::MidiFile& midi, int format = 1)
    {
        juce::TemporaryFile temporary (file);
        {
            juce::FileOutputStream out (temporary.getFile());
            if (! out.openedOk()) return juce::Result::fail ("Cannot create temporary MIDI file: " + out.getStatus().getErrorMessage());
            const bool written = midi.writeTo (out, format);
            out.flush();
            if (! written || out.getStatus().failed())
                return juce::Result::fail ("Cannot write MIDI: " + out.getStatus().getErrorMessage());
        }
        if (! temporary.overwriteTargetFileWithTemporary())
            return juce::Result::fail ("Cannot replace " + file.getFullPathName() + ". The destination was not changed.");
        return juce::Result::ok();
    }

    std::function<void()> onChange;
    void refresh()
    {
        const auto song = processor.getSong();
        selected = juce::jmin (selected, song.numEntries - 1);
        title.setText ("Song chain  " + juce::String (song.numEntries) + "/16", juce::dontSendNotification);
        enable.setToggleState (processor.isSongEnabled(), juce::dontSendNotification);
        add.setEnabled (song.numEntries < cs::kMaxSongEntries);
        add.getProperties().set ("primary", song.numEntries == 0);
        add.setColour (juce::TextButton::textColourOffId, song.numEntries == 0 ? ui::background : ui::text);
        clear.setEnabled (song.numEntries > 0);
        empty.setVisible (song.numEntries == 0);
        const bool editing = hasSelection (song.numEntries);
        sectionTitle.setText (editing ? "Section " + juce::String (selected + 1) : "Select a section",
                              juce::dontSendNotification);
        for (auto* control : std::initializer_list<juce::Component*> { &variation, &transpose, &remove })
            control->setVisible (editing);
        if (editing)
        {
            const auto entry = song.entries[(size_t) selected];
            variation.setSelectedId (entry.variation + 1, juce::dontSendNotification);
            transpose.setValue (entry.transpose, juce::dontSendNotification);
        }

        std::array<int, cs::kNumVariations> bars {};
        for (int v = 0; v < cs::kNumVariations; ++v)
            bars[(size_t) v] = processor.getProgression (v).bars;
        int x = 0;
        for (size_t i = 0; i < sections.size(); ++i)
        {
            auto& button = sections[i];
            const bool visible = (int) i < song.numEntries;
            button.setVisible (visible);
            if (! visible) continue;
            const auto entry = song.entries[i];
            const int length = bars[(size_t) entry.variation];
            const auto description = juce::String::charToString ('A' + entry.variation)
                + "  /  " + juce::String (length) + (length == 1 ? " bar" : " bars");
            button.setButtonText (description);
            button.setTooltip ("Section " + juce::String ((int) i + 1) + " · " + description
                               + " · " + juce::String (entry.transpose) + " st");
            button.setToggleState ((int) i == selected, juce::dontSendNotification);
            const int width = 68 + 19 * length;
            button.setBounds (x, 0, width, 60);
            x += width + 5;
        }
        blocks.setSize (juce::jmax (0, x - 5), 60);
        if (onChange) onChange();
    }

    void paint (juce::Graphics& g) override
    {
        for (auto bounds : { stageBounds, railBounds })
        {
            g.setColour (ui::panel.interpolatedWith (ui::songAccent, 0.045f));
            g.fillRoundedRectangle (bounds.toFloat(), 7.0f);
            g.setColour (ui::outline.interpolatedWith (ui::songAccent, 0.24f));
            g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 7.0f, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        railBounds = area.removeFromRight (328);
        area.removeFromRight (10);
        stageBounds = area;
        auto stage = stageBounds.reduced (12);
        auto header = stage.removeFromTop (32);
        title.setBounds (header.removeFromLeft (170));
        add.setBounds (header.removeFromRight (105));
        header.removeFromRight (5);
        clear.setBounds (header.removeFromRight (90));
        stage.removeFromTop (12);
        chain.setBounds (stage.removeFromTop (83));
        enable.setBounds (stage.removeFromBottom (28).withWidth (150));
        empty.setBounds (stageBounds.reduced (12).withTrimmedTop (50).withTrimmedBottom (30));

        auto rail = railBounds.reduced (12);
        sectionTitle.setBounds (rail.removeFromTop (32));
        variation.setBounds (rail.removeFromTop (30));
        rail.removeFromTop (8);
        transpose.setBounds (rail.removeFromTop (30));
        rail.removeFromTop (12);
        remove.setBounds (rail.removeFromTop (30).withWidth (104));
    }

private:
    bool hasSelection (int count) const noexcept { return selected >= 0 && selected < count; }
    template <typename Edit>
    void editSection (Edit edit)
    {
        auto song = processor.getSong();
        if (! hasSelection (song.numEntries)) return;
        edit (song.entries[(size_t) selected]);
        processor.setSong (song);
        refresh();
    }

    ChordSlotsProcessor& processor;
    int selected = -1;
    juce::Rectangle<int> stageBounds, railBounds;
    juce::Label title, empty, sectionTitle;
    juce::TextButton enable { "Song playback" }, add { "+ Add section" }, clear { "Clear chain" }, remove { "Remove section" };
    juce::ComboBox variation;
    juce::Slider transpose;
    std::array<juce::TextButton, cs::kMaxSongEntries> sections;
    juce::Component blocks;
    juce::Viewport chain;
};
