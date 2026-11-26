// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../PluginProcessor.h"
#include "Localization.h"
#include "LookAndFeel.h"
#include <optional>

namespace ui
{
class ProgressionLibrary final : public juce::Component, private juce::ListBoxModel
{
public:
    using ReadBank = std::function<juce::ValueTree()>;
    using WriteBank = std::function<bool (const juce::ValueTree&)>;

    ProgressionLibrary (ReadBank read, WriteBank write,
                        std::function<cs::Progression()> capture,
                        std::function<void (const cs::Progression&)> load,
                        std::function<void (const cs::Progression&)> preview,
                        std::function<void()> stop)
        : readBank (std::move (read)), writeBank (std::move (write)),
          captureProgression (std::move (capture)), loadProgression (std::move (load)),
          previewProgression (std::move (preview)), stopPreview (std::move (stop))
    {
        setLookAndFeel (&lookAndFeel);
        for (auto* child : std::initializer_list<juce::Component*> {
                 &search, &category, &list, &title, &details, &save, &edit, &remove, &audition, &apply })
            addAndMakeVisible (*child);
        search.setTextToShowWhenEmpty (tr ("Search presets"), textDim);
        search.setTitle (tr ("Search presets"));
        search.onTextChange = [this] { filter(); };
        category.setTitle (tr ("Category"));
        category.onChange = [this] { filter(); };
        list.setRowHeight (44);
        list.setModel (this);
        list.setColour (juce::ListBox::backgroundColourId, panel);
        list.setAccessible (true);
        list.setTitle (tr ("Progression presets"));
        title.setFont (font (20.0f, true));
        details.setMultiLine (true);
        details.setReadOnly (true);
        details.setTitle (tr ("Preset details"));
        details.setScrollbarsShown (true);
        details.setColour (juce::TextEditor::backgroundColourId, panel);
        apply.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.30f));
        apply.setTooltip (tr ("Replace the current variation with this progression."));
        save.onClick = [this]
        {
            juce::ValueTree entry ("Preset");
            entry.setProperty ("id", juce::Uuid().toString(), nullptr);
            entry.setProperty ("name", tr ("Untitled progression"), nullptr);
            entry.appendChild (ChordSlotsProcessor::serializeProgression (captureProgression()), nullptr);
            editMetadata (entry, true);
        };
        edit.onClick = [this] { editMetadata (selectedEntry(), false); };
        remove.onClick = [this] { deleteSelected(); };
        audition.onClick = [this]
        {
            if (const auto progression = decode (selectedEntry()))
            {
                previewStarted = true;
                previewProgression (*progression);
            }
        };
        apply.onClick = [this] { applySelected(); };
        reload();
        setSize (710, 430);
    }

    ~ProgressionLibrary() override
    {
        if (previewStarted)
            stopPreview();
        list.setModel (nullptr);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16);
        auto toolbar = area.removeFromTop (32);
        category.setBounds (toolbar.removeFromRight (182));
        toolbar.removeFromRight (8);
        search.setBounds (toolbar);
        area.removeFromTop (10);
        auto actions = area.removeFromBottom (32);
        apply.setBounds (actions.removeFromRight (102));
        actions.removeFromRight (8);
        audition.setBounds (actions.removeFromRight (102));
        save.setBounds (actions.removeFromLeft (134));
        actions.removeFromLeft (8);
        edit.setBounds (actions.removeFromLeft (92));
        actions.removeFromLeft (8);
        remove.setBounds (actions.removeFromLeft (88));
        area.removeFromBottom (10);
        list.setBounds (area.removeFromLeft (area.getWidth() * 46 / 100));
        area.removeFromLeft (12);
        title.setBounds (area.removeFromTop (34));
        details.setBounds (area);
    }

private:
    static juce::String categoryOf (const juce::ValueTree& entry)
    {
        const auto value = entry.getProperty ("category").toString();
        return value.isEmpty() ? tr ("Uncategorized") : value;
    }

    static juce::ValueTree factoryBank()
    {
        juce::ValueTree bank ("Factory");
        const auto add = [&] (const char* name, const char* style, const char* description,
                              int tonic, int scale, std::initializer_list<cs::Chord> chords)
        {
            cs::Progression p;
            p.tonic = tonic;
            p.scale = scale;
            p.numSlots = (int) chords.size();
            cs::layoutEven (p);
            cs::Rng rng (1);
            int slot = 0;
            for (auto chord : chords)
                cs::setChord (p, slot++, chord, rng);

            juce::ValueTree entry ("Preset");
            entry.setProperty ("id", juce::String ("factory/") + name, nullptr);
            entry.setProperty ("name", name, nullptr);
            entry.setProperty ("category", tr (style), nullptr);
            entry.setProperty ("description", tr (description), nullptr);
            entry.setProperty ("factory", true, nullptr);
            entry.appendChild (ChordSlotsProcessor::serializeProgression (p), nullptr);
            bank.appendChild (entry, nullptr);
        };
        constexpr int major = 0, minor = 1, sus4 = 6, maj7 = 7, min7 = 8, dom7 = 9, halfDim = 10;
        add ("Open Highway", "Pop", "A bright four-chord loop with a lift into the turnaround.",
             0, major, { { 0, major }, { 7, major }, { 9, minor }, { 5, major } });
        add ("Start Again", "Pop", "A minor opening gives way to a broad major chorus.",
             0, major, { { 9, minor }, { 5, major }, { 0, major }, { 7, major } });
        add ("Blue Hour", "Jazz", "A ii-V-I cadence settles into a warm major seventh.",
             0, major, { { 2, min7 }, { 7, dom7 }, { 0, maj7 }, { 0, maj7 } });
        add ("Last Table", "Jazz", "A half-diminished ii chord and dominant seventh lead home to minor.",
             9, minor, { { 11, halfDim }, { 4, dom7 }, { 9, min7 }, { 9, min7 } });
        add ("Late Train", "Soul", "Soft seventh chords circle through the relative major and minor.",
             0, major, { { 5, maj7 }, { 4, min7 }, { 9, min7 }, { 2, min7 } });
        add ("Warm Current", "Soul", "A rising bass line under a relaxed major-seventh turnaround.",
             5, major, { { 5, maj7 }, { 7, min7 }, { 9, min7 }, { 10, maj7 } });
        add ("Night Signal", "Electronic", "A steady minor loop with a darker second half.",
             9, minor, { { 9, minor }, { 7, major }, { 5, major }, { 2, minor } });
        add ("Glass Horizon", "Cinematic", "A suspended dominant holds the tension before the minor loop starts over.",
             9, minor, { { 9, minor }, { 2, minor }, { 5, major }, { 4, sus4 } });
        add ("Pocket Lantern", "Soul", "Two minor-key seventh chords leave room for a lead to trade phrases.",
             2, minor, { { 2, min7 }, { 7, min7 } });
        add ("Down the Stairwell", "Cinematic", "The bass descends A, G, F, E before the dominant pulls back to A minor.",
             9, minor, { { 9, minor }, { 7, major }, { 5, maj7 }, { 4, dom7 } });
        add ("Held Door", "Pop", "The suspended G resolves inside the turnaround before C returns on the next pass.",
             0, major, { { 0, major }, { 5, major }, { 7, sus4 }, { 7, dom7 } });
        add ("Porch Light", "Soul", "A final IV to I lands softly without a dominant chord.",
             5, major, { { 5, maj7 }, { 2, min7 }, { 10, maj7 }, { 5, major } });
        add ("False Landing", "Pop", "The dominant slips into the relative minor at the end for a deceptive cadence.",
             7, major, { { 7, major }, { 0, major }, { 2, dom7 }, { 4, minor } });
        add ("Cut to Black", "Electronic", "A minor verse ends on its dominant, leaving the next section to answer.",
             4, minor, { { 4, minor }, { 0, major }, { 9, minor }, { 11, dom7 } });
        return bank;
    }

    static std::optional<cs::Progression> decode (const juce::ValueTree& entry)
    {
        cs::Progression p;
        if (ChordSlotsProcessor::deserializeProgression (entry.getChildWithName ("Progression"), p))
            return p;
        return std::nullopt;
    }

    void reload (const juce::String& preferredId = {})
    {
        const auto previousCategory = category.getText();
        bank = factoryBank();
        for (const auto& entry : readBank())
            if (entry.hasType ("Preset") && entry.getChildWithName ("Progression").isValid())
                bank.appendChild (entry.createCopy(), nullptr);

        juce::StringArray categories;
        for (const auto& entry : bank)
            categories.addIfNotAlreadyThere (categoryOf (entry), true);
        categories.sort (true);
        category.clear (juce::dontSendNotification);
        category.addItem (tr ("All categories"), 1);
        category.addItemList (categories, 2);
        const int previousIndex = categories.indexOf (previousCategory, true);
        category.setSelectedId (previousIndex < 0 ? 1 : previousIndex + 2, juce::dontSendNotification);
        if (preferredId.isNotEmpty())
        {
            search.clear();
            category.setSelectedId (1, juce::dontSendNotification);
        }
        filter (preferredId);
    }

    void filter (const juce::String& preferredId = {})
    {
        const auto selectedId = preferredId.isEmpty() ? selectedEntry().getProperty ("id").toString() : preferredId;
        const auto query = search.getText().trim();
        rows.clear();
        for (const auto& entry : bank)
        {
            const auto style = categoryOf (entry);
            if (category.getSelectedId() > 1 && ! style.equalsIgnoreCase (category.getText()))
                continue;
            const auto searchable = entry.getProperty ("name").toString() + " " + style
                                  + " " + entry.getProperty ("description").toString();
            if (query.isEmpty() || searchable.containsIgnoreCase (query))
                rows.push_back (entry);
        }
        std::stable_sort (rows.begin(), rows.end(), [] (const auto& a, const auto& b)
        {
            return a.getProperty ("name").toString().compareNatural (b.getProperty ("name").toString()) < 0;
        });
        list.deselectAllRows();
        list.updateContent();
        const auto selected = std::find_if (rows.begin(), rows.end(), [&] (const auto& entry)
        {
            return entry.getProperty ("id").toString() == selectedId;
        });
        if (! rows.empty())
            list.selectRow (selected == rows.end() ? 0 : (int) (selected - rows.begin()));
        selectedRowsChanged (list.getSelectedRow());
    }

    int getNumRows() override { return (int) rows.size(); }

    juce::String getNameForRow (int row) override
    {
        return juce::isPositiveAndBelow (row, getNumRows())
            ? rows[(size_t) row].getProperty ("name").toString() : juce::String {};
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, getNumRows()))
            return;
        const auto& entry = rows[(size_t) row];
        g.fillAll (selected ? panelHi : panel);
        if (selected)
        {
            g.setColour (accent);
            g.fillRect (0, 0, 3, height);
        }
        g.setColour (text);
        g.setFont (font (14.0f, true));
        g.drawText (entry.getProperty ("name").toString(), 12, 4, width - 24, 20, juce::Justification::centredLeft);
        g.setColour (textDim);
        g.setFont (font (12.0f));
        const auto source = (bool) entry.getProperty ("factory", false) ? tr ("Factory") : tr ("User");
        g.drawText (categoryOf (entry) + "  /  " + source, 12, 23, width - 24, 17, juce::Justification::centredLeft);
    }

    juce::ValueTree selectedEntry() const
    {
        const int row = list.getSelectedRow();
        return juce::isPositiveAndBelow (row, (int) rows.size()) ? rows[(size_t) row] : juce::ValueTree {};
    }

    void selectedRowsChanged (int) override
    {
        const auto entry = selectedEntry();
        const auto p = decode (entry);
        const bool editable = entry.isValid() && ! (bool) entry.getProperty ("factory", false);
        edit.setEnabled (editable);
        remove.setEnabled (editable);
        audition.setEnabled (p.has_value());
        apply.setEnabled (p.has_value());
        title.setText (entry.isValid() ? entry.getProperty ("name").toString() : tr ("No matching presets"),
                       juce::dontSendNotification);
        juce::String info;
        if (p)
        {
            juce::StringArray chords;
            for (int i = 0; i < p->numSlots; ++i)
                chords.add (ui::chordName (p->slots[(size_t) i].chord, p->tonic, p->scale));
            info = categoryOf (entry) + "  /  " + juce::String (p->bars) + " " + tr ("bars")
                 + "  /  " + juce::String (p->timeSigNum) + "/" + juce::String (p->timeSigDen)
                 + "\n\n" + chords.joinIntoString ("  >  ");
            const auto description = entry.getProperty ("description").toString();
            if (description.isNotEmpty())
                info += "\n\n" + description;
        }
        details.setText (info, false);
        details.setCaretPosition (0);
    }

    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { applySelected(); }
    void returnKeyPressed (int) override { applySelected(); }

    void applySelected()
    {
        if (const auto progression = decode (selectedEntry()))
        {
            loadProgression (*progression);
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState (0);
        }
    }

    void editMetadata (juce::ValueTree entry, bool isNew)
    {
        auto* dialog = new juce::AlertWindow (isNew ? tr ("Save progression preset") : tr ("Edit progression preset"),
                                             {}, juce::MessageBoxIconType::NoIcon, this);
        dialog->addTextEditor ("name", entry.getProperty ("name").toString(), tr ("Name"));
        dialog->addTextEditor ("category", entry.getProperty ("category").toString(), tr ("Category"));
        dialog->addTextEditor ("description", entry.getProperty ("description").toString(), tr ("Description"));
        dialog->addButton (tr ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        dialog->addButton (tr ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        dialog->enterModalState (true, juce::ModalCallbackFunction::create (
            [safe = SafePointer<ProgressionLibrary> (this), dialog, entry, isNew] (int result)
            {
                if (safe == nullptr || result != 1)
                    return;
                auto userBank = safe->readBank();
                const auto id = entry.getProperty ("id");
                auto item = isNew ? entry : userBank.getChildWithProperty ("id", id);
                if (! item.isValid())
                    return;
                const auto name = dialog->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                    item.setProperty ("name", name, nullptr);
                for (const auto* field : { "category", "description" })
                    item.setProperty (field, dialog->getTextEditorContents (field).trim(), nullptr);
                if (isNew)
                    userBank.appendChild (item, nullptr);
                if (safe->writeBank (userBank))
                    safe->reload (id.toString());
            }), true);
    }

    void deleteSelected()
    {
        const auto entry = selectedEntry();
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, tr ("Delete progression preset"),
            tr ("Delete ") + entry.getProperty ("name").toString() + "?", tr ("Delete"), tr ("Cancel"), this,
            juce::ModalCallbackFunction::create (
                [safe = SafePointer<ProgressionLibrary> (this), id = entry.getProperty ("id")] (int confirmed)
                {
                    if (safe == nullptr || ! confirmed)
                        return;
                    auto userBank = safe->readBank();
                    const auto item = userBank.getChildWithProperty ("id", id);
                    if (! item.isValid())
                        return;
                    userBank.removeChild (item, nullptr);
                    if (safe->writeBank (userBank))
                        safe->reload();
                }));
    }

    ReadBank readBank;
    WriteBank writeBank;
    std::function<cs::Progression()> captureProgression;
    std::function<void (const cs::Progression&)> loadProgression, previewProgression;
    std::function<void()> stopPreview;
    bool previewStarted = false;
    ChordSlotsLookAndFeel lookAndFeel;
    juce::ValueTree bank;
    std::vector<juce::ValueTree> rows;
    juce::TextEditor search, details;
    juce::ComboBox category;
    juce::ListBox list;
    juce::Label title;
    juce::TextButton save { tr ("Save current") }, edit { tr ("Edit details") }, remove { tr ("Delete") },
                     audition { tr ("Preview") }, apply { tr ("Load") };
};
} // namespace ui
