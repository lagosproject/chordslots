// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Localization.h"
#include "../PluginProcessor.h"
#include "LookAndFeel.h"

class SongPanel final : public juce::Component
{
public:
    explicit SongPanel (ChordSlotsProcessor& p) : processor (p)
    {
        for (auto* control : std::initializer_list<juce::Component*> {
                 &title, &empty, &chain, &enable, &add, &clear,
                 &includeMelody })
            addAndMakeVisible (*control);

        setComponentID ("song-panel");
        includeMelody.setComponentID ("song-include-melody");
        includeMelody.setTooltip (ui::tr ("Include the imported melodies from the Song sections in Save MIDI and Drag Song."));
        includeMelody.onClick = [this] { if (onChange) onChange(); };
        title.setColour (juce::Label::textColourId, ui::songAccent);
        title.setFont (ui::font (17.0f, true));
        empty.setText (ui::tr ("Add a section to arrange your variations."), juce::dontSendNotification);
        empty.setJustificationType (juce::Justification::centred);
        chain.setViewedComponent (&blocks, false);
        chain.setScrollBarsShown (true, false);
        for (size_t i = 0; i < sections.size(); ++i)
        {
            auto& row = sections[i];
            const auto index = juce::String ((int) i);
            for (auto* component : std::initializer_list<juce::Component*> { &row.select, &row.variation,
                       &row.summary, &row.transpose, &row.remove })
                blocks.addChildComponent (component);
            row.select.setComponentID ("song-section-" + index);
            row.select.setTitle (ui::tr ("Section ") + juce::String ((int) i + 1));
            row.select.onClick = [this, i] { selected = (int) i; refresh(); };
            row.variation.setComponentID ("song-variation-" + index);
            row.variation.setTitle (ui::tr ("Variation"));
            row.transpose.setComponentID ("song-transpose-" + index);
            row.transpose.setTitle (ui::tr ("Transpose"));
            row.transpose.setSliderStyle (juce::Slider::IncDecButtons);
            row.transpose.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 25);
            row.transpose.setColour (juce::Slider::textBoxBackgroundColourId, ui::panelHi);
            row.transpose.setColour (juce::Slider::textBoxOutlineColourId, ui::outline);
            row.transpose.setRange (cs::kMinSongTranspose, cs::kMaxSongTranspose, 1);
            row.transpose.setTextValueSuffix (" st");
            row.transpose.setTooltip (ui::tr ("Shift the selected section in semitones."));
            row.summary.setColour (juce::Label::textColourId, ui::textDim);
            row.summary.setFont (ui::font (13.0f));
            row.remove.getProperties().set ("quiet", true);
            row.remove.setTitle (ui::tr ("Remove section") + " " + juce::String ((int) i + 1));
            row.remove.onClick = [this, i] { removeSection ((int) i); };
            row.variation.onChange = [this, i]
            {
                const auto value = sections[i].variation.getSelectedId() - 1;
                if (value >= 0)
                    editSection ((int) i, [value] (auto& entry) { entry.variation = value; });
            };
            row.transpose.onValueChange = [this, i]
            {
                const auto value = (int) sections[i].transpose.getValue();
                editSection ((int) i, [value] (auto& entry) { entry.transpose = value; });
            };
        }
        enable.setTooltip (ui::tr ("Use the Song chain for host playback."));
        enable.onClick = [this] { processor.setSongEnabled (! processor.isSongEnabled()); refresh(); };
        add.onClick = [this]
        {
            auto song = processor.getSong();
            if (song.numEntries >= cs::kMaxSongEntries) return;
            selected = song.numEntries;
            song.entries[song.numEntries++] = { processor.getEditVariation(), 0 };
            processor.setSong (song);
            refresh();
            chain.setViewPosition (0, selected * rowHeight);
        };
        clear.onClick = [this] { selected = -1; processor.setSong ({}); refresh(); };
        refresh();
    }

    static juce::Result saveMidiTo (const juce::File& file, const juce::MidiFile& midi, int format = 1)
    {
        juce::TemporaryFile temporary (file);
        {
            juce::FileOutputStream out (temporary.getFile());
            if (! out.openedOk()) return juce::Result::fail (ui::tr ("Cannot create temporary MIDI file: ") + out.getStatus().getErrorMessage());
            const bool written = midi.writeTo (out, format);
            out.flush();
            if (! written || out.getStatus().failed())
                return juce::Result::fail (ui::tr ("Cannot write MIDI: ") + out.getStatus().getErrorMessage());
        }
        if (! temporary.overwriteTargetFileWithTemporary())
            return juce::Result::fail (ui::tr ("Cannot replace ") + file.getFullPathName() + ui::tr (". The destination was not changed."));
        return juce::Result::ok();
    }

    std::function<void()> onChange;
    bool includesMelody() const noexcept { return includeMelody.isEnabled() && includeMelody.getToggleState(); }

    void refresh()
    {
        const auto song = processor.getSong();
        const int variationCount = processor.getVariationCount();
        selected = juce::jmin (selected, song.numEntries - 1);
        enable.setToggleState (processor.isSongEnabled(), juce::dontSendNotification);
        add.setEnabled (song.numEntries < cs::kMaxSongEntries);
        add.getProperties().set ("primary", song.numEntries == 0);
        add.setColour (juce::TextButton::textColourOffId, song.numEntries == 0 ? ui::background : ui::text);
        clear.getProperties().set ("quiet", true);
        clear.setEnabled (song.numEntries > 0);
        empty.setVisible (song.numEntries == 0);
        std::array<int, cs::kMaxVariations> bars {};
        std::array<juce::String, cs::kMaxVariations> chordSummaries;
        std::array<bool, cs::kMaxVariations> hasMelody {};
        for (int v = 0; v < variationCount; ++v)
        {
            const auto progression = processor.getProgression (v);
            bars[(size_t) v] = progression.bars;
            juce::StringArray names;
            for (int i = 0; i < juce::jmin (progression.numSlots, 4); ++i)
                names.add (ui::chordName (progression.slots[(size_t) i].chord, progression.tonic, progression.scale));
            chordSummaries[(size_t) v] = names.joinIntoString ("  ·  ");
            if (progression.numSlots > 4)
                chordSummaries[(size_t) v] += " …";
            hasMelody[(size_t) v] = ! progression.melody.empty();
        }
        bool songHasMelody = false;
        rowCount = song.numEntries;
        int totalBars = 0;
        for (size_t i = 0; i < sections.size(); ++i)
        {
            auto& row = sections[i];
            const bool visible = (int) i < rowCount;
            for (auto* component : std::initializer_list<juce::Component*> { &row.select, &row.variation,
                       &row.summary, &row.transpose, &row.remove })
                component->setVisible (visible);
            if (! visible) continue;

            if (row.variation.getNumItems() != variationCount)
            {
                row.variation.clear (juce::dontSendNotification);
                for (int v = 0; v < variationCount; ++v)
                    row.variation.addItem (ui::tr ("Variation ") + juce::String::charToString ('A' + v), v + 1);
            }
            const auto entry = song.entries[i];
            songHasMelody |= hasMelody[(size_t) entry.variation];
            const int length = bars[(size_t) entry.variation];
            totalBars += length;
            const auto description = juce::String (length) + (length == 1 ? ui::tr (" bar") : ui::tr (" bars"));
            row.select.setButtonText (juce::String ((int) i + 1).paddedLeft ('0', 2));
            row.select.setTooltip (ui::tr ("Section ") + juce::String ((int) i + 1) + " · " + description
                                   + " · " + juce::String (entry.transpose) + " st");
            row.select.setToggleState ((int) i == selected, juce::dontSendNotification);
            row.variation.setSelectedId (entry.variation + 1, juce::dontSendNotification);
            row.transpose.setValue (entry.transpose, juce::dontSendNotification);
            const auto& chords = chordSummaries[(size_t) entry.variation];
            row.summary.setText (chords.isEmpty() ? description : chords, juce::dontSendNotification);
            row.summary.setTooltip (description + (chords.isEmpty() ? juce::String() : " · " + chords));
        }
        title.setText (ui::tr ("Song arrangement") + "   ·   " + juce::String (rowCount) + "/"
                       + juce::String (cs::kMaxSongEntries) + "   ·   " + juce::String (totalBars)
                       + (totalBars == 1 ? ui::tr (" bar") : ui::tr (" bars")),
                       juce::dontSendNotification);
        blocks.count = rowCount;
        blocks.selected = selected;
        layoutRows();
        blocks.repaint();
        includeMelody.setEnabled (songHasMelody);
        if (onChange) onChange();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (ui::background);
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (ui::outline.interpolatedWith (ui::songAccent, 0.24f));
        g.drawRoundedRectangle (bounds, 10.0f, 1.0f);
    }

    void resized() override
    {
        auto stage = getLocalBounds().reduced (12);
        title.setBounds (stage.removeFromTop (32));
        stage.removeFromTop (4);
        auto actions = stage.removeFromBottom (32);
        add.setBounds (actions.removeFromLeft (112));
        actions.removeFromLeft (6);
        clear.setBounds (actions.removeFromLeft (94));
        actions.removeFromLeft (12);
        enable.setBounds (actions.removeFromLeft (132));
        actions.removeFromLeft (10);
        includeMelody.setBounds (actions);
        stage.removeFromBottom (8);
        chain.setBounds (stage);
        empty.setBounds (stage.withHeight (juce::jmin (88, stage.getHeight())));
        layoutRows();
    }

private:
    static constexpr int rowHeight = 44;

    void layoutRows()
    {
        const auto width = juce::jmax (0, chain.getWidth() - chain.getScrollBarThickness());
        blocks.setSize (width, rowCount * rowHeight);
        for (int i = 0; i < rowCount; ++i)
        {
            auto& row = sections[(size_t) i];
            auto area = juce::Rectangle<int> (0, i * rowHeight, width, rowHeight).reduced (2, 4);
            row.select.setBounds (area.removeFromLeft (43));
            area.removeFromLeft (7);
            row.remove.setBounds (area.removeFromRight (115));
            area.removeFromRight (7);
            row.transpose.setBounds (area.removeFromRight (150).reduced (0, 3));
            area.removeFromRight (9);
            row.variation.setBounds (area.removeFromLeft (area.getWidth() * 2 / 5));
            row.summary.setBounds (area.reduced (8, 0));
        }
    }

    void removeSection (int index)
    {
        auto song = processor.getSong();
        if (! juce::isPositiveAndBelow (index, song.numEntries)) return;
        for (int i = index; i + 1 < song.numEntries; ++i)
            song.entries[i] = song.entries[i + 1];
        song.entries[--song.numEntries] = {};
        selected = juce::jmin (index, song.numEntries - 1);
        processor.setSong (song);
        refresh();
    }

    template <typename Edit>
    void editSection (int index, Edit edit)
    {
        auto song = processor.getSong();
        if (! juce::isPositiveAndBelow (index, song.numEntries)) return;
        selected = index;
        edit (song.entries[(size_t) index]);
        processor.setSong (song);
        refresh();
    }

    struct SectionRow
    {
        juce::TextButton select;
        juce::ComboBox variation;
        juce::Label summary;
        juce::Slider transpose;
        juce::TextButton remove { ui::tr ("Remove section") };
    };

    ChordSlotsProcessor& processor;
    int selected = -1, rowCount = 0;
    juce::Label title, empty;
    juce::TextButton enable { ui::tr ("Song playback") }, add { ui::tr ("+ Add section") }, clear { ui::tr ("Clear chain") };
    juce::ToggleButton includeMelody { ui::tr ("Include melody in MIDI export") };
    std::array<SectionRow, cs::kMaxSongEntries> sections;
    struct ArrangementRows final : juce::Component
    {
        int count = 0, selected = -1;

        void paint (juce::Graphics& g) override
        {
            for (int i = 0; i < count; ++i)
            {
                const auto card = juce::Rectangle<float> (0.5f, (float) (i * rowHeight + 1),
                                                         (float) getWidth() - 1.0f, (float) rowHeight - 2.0f);
                g.setColour (i == selected ? ui::panel.interpolatedWith (ui::songAccent, 0.09f) : ui::panel);
                g.fillRoundedRectangle (card, 6.0f);
                g.setColour (i == selected ? ui::songAccent.withAlpha (0.64f) : ui::outline);
                g.drawRoundedRectangle (card, 6.0f, 1.0f);
            }
        }
    } blocks;
    juce::Viewport chain;
};
