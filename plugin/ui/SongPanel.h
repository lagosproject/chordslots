// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "MidiDragTile.h"

// Child panel: all section controls stay inside the editor, including long chains.
class SongPanel final : public juce::Component, private juce::Timer
{
public:
    explicit SongPanel (ChordSlotsProcessor& p) : processor (p), drag (p, ChordSlotsProcessor::MidiExportMode::Song, "Drag Song")
    {
        setComponentID ("song-panel");
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &title, &help, &empty, &close, &enable, &add, &clear, &play, &save, &drag, &viewport }) addAndMakeVisible (*c);
        title.setText ("SONG CHAIN", juce::dontSendNotification);
        help.setText ("Plays top to bottom. Each section uses a variation's full length. Transpose is in semitones.", juce::dontSendNotification);
        empty.setText ("Your Song chain is empty.\nAdd a section to arrange your variations.", juce::dontSendNotification);
        empty.setJustificationType (juce::Justification::centred);
        viewport.setViewedComponent (&rows, false);
        viewport.setScrollBarsShown (true, false);
        close.onClick = [this] { if (onClose) onClose(); };
        enable.onClick = [this] { processor.setSongEnabled (! processor.isSongEnabled()); refresh(); };
        add.onClick = [this] {
            auto song = processor.getSong();
            if (song.numEntries < cs::kMaxSongEntries) song.entries[song.numEntries++] = { processor.getEditVariation(), 0 };
            processor.setSong (song); refresh();
            viewport.setViewPosition (0, rows.getHeight());
        };
        clear.onClick = [this] { processor.setSong ({}); refresh(); };
        play.onClick = [this] {
            const bool starting = ! processor.isPreviewPlaying();
            if (starting) processor.setSongEnabled (true);
            processor.setPreviewPlaying (starting); refresh();
        };
        save.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser> ("Save Song MIDI...", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ChordSlots Song.mid"), "*.mid");
            juce::Component::SafePointer<SongPanel> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::saveMode,
                [safe] (const juce::FileChooser& fc) {
                    if (! safe || fc.getResult() == juce::File()) return;
                    const auto file = fc.getResult().withFileExtension ("mid");
                    auto write = [safe, file] {
                        if (! safe) return;
                        const auto result = saveMidiTo (file, safe->processor.createSongMidiFile());
                        if (result.failed())
                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                "Song MIDI export failed", result.getErrorMessage());
                    };
                    // Confirm the actual destination after normalizing the extension.
                    if (file.exists())
                        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                            "Replace Song MIDI?", "Replace " + file.getFullPathName() + "?",
                            "Replace", "Cancel", safe.getComponent(),
                            juce::ModalCallbackFunction::create ([write] (int result) { if (result == 1) write(); }));
                    else
                        write();
                });
        };
        for (int i = 0; i < cs::kMaxSongEntries; ++i)
        {
            auto& r = sections[(size_t) i];
            for (auto* c : std::initializer_list<juce::Component*> { &r.number, &r.variation, &r.transpose, &r.remove }) rows.addChildComponent (*c);
            r.number.setText (juce::String (i + 1).paddedLeft ('0', 2), juce::dontSendNotification);
            for (int v = 0; v < cs::kNumVariations; ++v) r.variation.addItem ("Variation " + juce::String::charToString ('A' + v), v + 1);
            r.variation.setComponentID ("song-variation-" + juce::String (i));
            r.transpose.setComponentID ("song-transpose-" + juce::String (i));
            r.transpose.setSliderStyle (juce::Slider::IncDecButtons);
            r.transpose.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 65, 28);
            r.transpose.setRange (cs::kMinSongTranspose, cs::kMaxSongTranspose, 1);
            r.transpose.setTextValueSuffix (" st");
            r.transpose.setTooltip ("Transpose this section in semitones");
            r.variation.onChange = [this, i] { auto s = processor.getSong(); if (i < s.numEntries) { s.entries[i].variation = sections[(size_t) i].variation.getSelectedId() - 1; processor.setSong (s); } };
            r.transpose.onValueChange = [this, i] { auto s = processor.getSong(); if (i < s.numEntries) { s.entries[i].transpose = (int) sections[(size_t) i].transpose.getValue(); processor.setSong (s); } };
            r.remove.onClick = [this, i] { auto s = processor.getSong(); if (i < s.numEntries) { for (int n = i; n + 1 < s.numEntries; ++n) s.entries[n] = s.entries[n + 1]; s.entries[--s.numEntries] = {}; processor.setSong (s); refresh(); } };
        }
        refresh(); startTimerHz (10);
    }
    // Stage the complete MIDI beside the destination before replacing it.
    static juce::Result saveMidiTo (const juce::File& file, const juce::MidiFile& midi)
    {
        juce::TemporaryFile temporary (file);
        {
            juce::FileOutputStream out (temporary.getFile());
            if (! out.openedOk()) return juce::Result::fail ("Cannot create temporary MIDI file: " + out.getStatus().getErrorMessage());
            const bool written = midi.writeTo (out, 1);
            out.flush();
            if (! written || out.getStatus().failed())
                return juce::Result::fail ("Cannot write Song MIDI: " + out.getStatus().getErrorMessage());
        }
        if (! temporary.overwriteTargetFileWithTemporary())
            return juce::Result::fail ("Cannot replace " + file.getFullPathName() + ". The destination was not changed.");
        return juce::Result::ok();
    }
    std::function<void()> onClose;
    void refresh()
    {
        const auto song = processor.getSong();
        title.setText ("SONG CHAIN   " + juce::String (song.numEntries) + "/16", juce::dontSendNotification);
        enable.setToggleState (processor.isSongEnabled(), juce::dontSendNotification);
        play.setButtonText (processor.isPreviewPlaying() ? "Stop" : "Play Song");
        add.setEnabled (song.numEntries < cs::kMaxSongEntries);
        play.setEnabled (song.numEntries > 0 || processor.isPreviewPlaying());
        for (auto* c : std::initializer_list<juce::Component*> { &clear, &save, &drag }) c->setEnabled (song.numEntries > 0);
        empty.setVisible (song.numEntries == 0);
        for (int i = 0; i < cs::kMaxSongEntries; ++i) {
            auto& r = sections[(size_t) i];
            for (auto* c : std::initializer_list<juce::Component*> { &r.number, &r.variation, &r.transpose, &r.remove }) c->setVisible (i < song.numEntries);
            if (i < song.numEntries) {
                r.variation.setSelectedId (song.entries[i].variation + 1, juce::dontSendNotification);
                r.transpose.setValue (song.entries[i].transpose, juce::dontSendNotification);
            }
        }
        resized();
    }
    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colour (0xff1c1e28));
        g.setColour (juce::Colour (0xff464253)); g.drawRect (getLocalBounds());
    }
    void resized() override {
        auto a = getLocalBounds().reduced (20);
        auto h = a.removeFromTop (32); close.setBounds (h.removeFromRight (70)); enable.setBounds (h.removeFromRight (150)); title.setBounds (h);
        help.setBounds (a.removeFromTop (30)); a.removeFromTop (8);
        auto f = a.removeFromBottom (38); add.setBounds (f.removeFromLeft (130)); f.removeFromLeft (8); clear.setBounds (f.removeFromLeft (100));
        drag.setBounds (f.removeFromRight (110)); f.removeFromRight (8); save.setBounds (f.removeFromRight (130)); f.removeFromRight (8); play.setBounds (f.removeFromRight (100));
        a.removeFromBottom (12); viewport.setBounds (a); empty.setBounds (a);
        rows.setSize (juce::jmax (1, a.getWidth() - 16), processor.getSong().numEntries * 44);
        for (int i = 0; i < cs::kMaxSongEntries; ++i) { auto r = juce::Rectangle<int> (0, i * 44, rows.getWidth(), 36); auto& s = sections[(size_t) i]; s.number.setBounds (r.removeFromLeft (50)); s.remove.setBounds (r.removeFromRight (90)); r.removeFromRight (16); s.transpose.setBounds (r.removeFromRight (150)); r.removeFromRight (16); s.variation.setBounds (r); }
    }
private:
    void timerCallback() override { if (isVisible()) refresh(); }
    ChordSlotsProcessor& processor;
    struct Section { juce::Label number; juce::ComboBox variation; juce::Slider transpose; juce::TextButton remove { "Remove" }; };
    std::array<Section, cs::kMaxSongEntries> sections;
    juce::Label title, help, empty;
    juce::TextButton close { "Close" }, enable { "Song playback" }, add { "Add section" }, clear { "Clear chain" }, play { "Play Song" }, save { "Save Song MIDI" };
    MidiDragTile drag;
    juce::Component rows;
    juce::Viewport viewport;
    std::unique_ptr<juce::FileChooser> chooser;
};
