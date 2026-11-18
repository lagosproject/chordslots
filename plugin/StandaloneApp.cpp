// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EditorPreferences.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
class ChordSlotsWindow final : public juce::StandaloneFilterWindow
{
public:
    using StandaloneFilterWindow::StandaloneFilterWindow;

private:
    static constexpr int showTooltips = 5;

    // JUCE's title-bar button dispatches through this Button::Listener override.
    void buttonClicked (juce::Button* button) override
    {
        juce::PopupMenu menu;
        menu.addItem (1, TRANS ("Audio/MIDI Settings..."));
        menu.addSeparator();
        menu.addItem (2, TRANS ("Save current state..."));
        menu.addItem (3, TRANS ("Load a saved state..."));
        menu.addSeparator();
        menu.addItem (4, TRANS ("Reset to default state"));
        menu.addSeparator();
        menu.addItem (showTooltips, "Show Tooltips", true, editorPreferences::tooltipsEnabled());

        SafePointer<ChordSlotsWindow> window (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (button), [window] (int result)
        {
            if (window == nullptr)
                return;

            if (result == showTooltips)
                editorPreferences::setTooltipsEnabled (! editorPreferences::tooltipsEnabled());
            else
                window->handleMenuResult (result);
        });
    }
};

class ChordSlotsApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = getApplicationName();
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #endif
        appProperties.setStorageParameters (options);

        auto holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false);
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            headlessHolder = std::move (holder);
            return;
        }

        const auto background = juce::LookAndFeel::getDefaultLookAndFeel()
                                    .findColour (juce::ResizableWindow::backgroundColourId);
        window = std::make_unique<ChordSlotsWindow> (getApplicationName(), background, std::move (holder));
        window->setVisible (true);
    }

    void shutdown() override
    {
        headlessHolder.reset();
        window.reset();
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (auto* holder = window != nullptr ? window->getPluginHolder() : headlessHolder.get())
            holder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
            return;
        }

        quit();
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<ChordSlotsWindow> window;
    std::unique_ptr<juce::StandalonePluginHolder> headlessHolder;
};
}

JUCE_CREATE_APPLICATION_DEFINE (ChordSlotsApp)
