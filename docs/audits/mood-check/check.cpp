#include "../../../plugin/PluginProcessor.h"
#include "../../../plugin/ui/LanesView.h"
#include <iostream>
#include <stdexcept>

void check (bool ok, const char* message)
{
    if (! ok) throw std::runtime_error (message);
    std::cout << message << '\n';
}

juce::ComboBox& findCombo (juce::Component& editor, const juce::String& firstItem)
{
    for (auto* child : editor.getChildren())
        if (auto* combo = dynamic_cast<juce::ComboBox*> (child))
            if (combo->getItemText (0) == firstItem)
                return *combo;
    throw std::runtime_error ("Combo unavailable");
}

juce::String details (const juce::ComboBox& box, const juce::String& mood)
{
    for (juce::PopupMenu::MenuItemIterator it (*box.getRootMenu()); it.next();)
        if (it.getItem().text == mood)
            return it.getItem().shortcutKeyDescription;
    throw std::runtime_error ("Mood unavailable");
}

void writeImage (const juce::Image& image, const char* name)
{
    juce::FileOutputStream stream (juce::File::getCurrentWorkingDirectory().getChildFile (name));
    stream.setPosition (0);
    stream.truncate();
    if (! juce::PNGImageFormat().writeImageToStream (image, stream))
        throw std::runtime_error ("Image write failed");
}

void renderMenu (juce::ComboBox& combo, const char* name)
{
    auto* label = dynamic_cast<juce::Label*> (combo.getChildComponent (0));
    if (label == nullptr) throw std::runtime_error ("Combo label unavailable");
    auto& look = combo.getLookAndFeel();
    const auto opts = look.getOptionsForComboBoxPopupMenu (combo, *label);
    struct Row { juce::PopupMenu::Item item; int height; };
    std::vector<Row> rows;
    const int border = look.getPopupMenuBorderSizeWithOptions (opts);
    int width = combo.getWidth(), height = border * 2;
    for (juce::PopupMenu::MenuItemIterator it (*combo.getRootMenu()); it.next();)
    {
        const auto& item = it.getItem();
        int w = 0, h = 0;
        if (item.isSectionHeader)
            look.getIdealPopupMenuSectionHeaderSizeWithOptions (item.text, opts.getStandardItemHeight(), w, h, opts);
        else
            look.getIdealPopupMenuItemSizeWithOptions (item.text + "   " + item.shortcutKeyDescription,
                false, opts.getStandardItemHeight(), w, h, opts);
        width = std::max (width, w + 2 * border);
        height += h;
        rows.push_back ({ item, h });
    }
    check (width <= 1040 && height <= 650 && rows.size() == 9, "Popup rows fit within default editor dimensions");
    juce::Image image (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (image);
        look.drawPopupMenuBackgroundWithOptions (g, width, height, opts);
        auto remaining = image.getBounds().reduced (border);
        for (auto& row : rows)
        {
            const auto area = remaining.removeFromTop (row.height);
            if (row.item.isSectionHeader)
                look.drawPopupMenuSectionHeaderWithOptions (g, area, row.item.text, opts);
            else
            {
                row.item.isTicked = row.item.itemID == combo.getSelectedId();
                look.drawPopupMenuItemWithOptions (g, area, false, row.item, opts);
            }
        }
    }
    writeImage (image, name);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto processor = std::make_unique<ChordSlotsProcessor>();
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    auto& mood = findCombo (*editor, "Euphoric");
    auto& transitions = findCombo (*editor, "Auto");
    const auto flush = [&] { processor->dispatchPendingMessages(); };
    processor->edit ([] (cs::Progression& p, cs::Rng&) { p.scale = 1; p.style = -1; p.mood = -1; });
    flush();
    check (editor->getWidth() == 1040 && editor->getHeight() == 650 && mood.getWidth() == 100
        && editor->getLocalBounds().contains (mood.getBounds()), "Default editor and Mood bounds");
    check (mood.getSelectedId() == 0, "Custom has no preset ID");
    check (details (mood, "Dreamy") == "Lydian  |  Adventure 60%  |  Syncopation 15%  |  Auto uses Extended harmony",
        "Dreamy preview before selection");
    check (details (mood, "Euphoric").endsWith ("Auto uses Major"), "Major scale fallback in Auto");
    check (details (mood, "Uplifting").endsWith ("Auto uses Minor"), "Minor scale fallback in Auto");
    check (processor->getProgression().scale == 1, "Reading previews leaves current scale alone");
    renderMenu (mood, "menu-auto.png");
    mood.setSelectedId (7, juce::sendNotificationSync);
    flush();
    auto applied = processor->getProgression();
    check (applied.scale == cs::moods()[6].scale && applied.mood == 6
        && cs::styleForProgression (applied) == cs::CorpusStyle::SoulRnb, "Dreamy applies new scale and Auto override");
    check (applied.adventure == cs::moods()[6].adventure && applied.syncopation == cs::moods()[6].syncopation,
        "Dreamy applies Adventure and Syncopation");
    cs::Progression expected = applied;
    cs::applyMood (expected, 6);
    for (int i = 0; i < applied.numSlots; ++i)
        check (applied.slots[i].targetB == expected.slots[i].targetB
            && applied.slots[i].targetT == expected.slots[i].targetT
            && applied.slots[i].targetE == expected.slots[i].targetE, "Dreamy applied all three slot targets");
    check (mood.getText() == "Dreamy", "Closed selector shows the short name");
    mood.setSelectedId (1, juce::sendNotificationSync);
    flush();
    check (cs::styleForProgression (processor->getProgression()) == cs::CorpusStyle::PopRock, "Euphoric follows its scale in Auto");
    transitions.setSelectedId (5, juce::sendNotificationSync);
    flush();
    check (details (mood, "Dreamy").endsWith ("Keeps Free"), "Manual style is shown before applying Dreamy");
    renderMenu (mood, "menu-manual.png");
    mood.setSelectedId (7, juce::sendNotificationSync);
    flush();
    check (cs::styleForProgression (processor->getProgression()) == cs::CorpusStyle::Free, "Manual style survives Dreamy");
    LanesView* lanes = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* view = dynamic_cast<LanesView*> (child))
            lanes = view;
    if (lanes == nullptr) throw std::runtime_error ("Target lanes unavailable");
    const juce::Point<float> pos (20.f, (float) lanes->getHeight() - 12.f);
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent click (juce::Desktop::getInstance().getMainMouseSource(), pos,
        juce::ModifierKeys::leftButtonModifier, 0.f, 0.f, 0.f, 0.f, 0.f, lanes, lanes, now, pos, now, 1, false);
    lanes->mouseDown (click);
    lanes->mouseUp (click);
    flush();
    check (mood.getSelectedId() == 0 && processor->getProgression().mood == -1, "Energy lane gesture clears preset selection");
    for (auto* child : mood.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child))
        {
            const auto font = label->getLookAndFeel().getLabelFont (*label);
            for (juce::PopupMenu::MenuItemIterator it (*mood.getRootMenu()); it.next();)
                if (it.getItem().itemID != 0 && juce::GlyphArrangement::getStringWidthInt (font, it.getItem().text) > label->getWidth())
                    throw std::runtime_error ("Preset name exceeds closed selector");
        }
    writeImage (editor->createComponentSnapshot (editor->getLocalBounds()), "default-editor.png");
}
