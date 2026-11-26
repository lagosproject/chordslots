// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>
#include <chordslots/Theory.h>
#include <string_view>
#include <unordered_map>

namespace ui
{
enum class Language { english, spanish, french };

inline Language languageFromCode (const juce::String& code)
{
    if (code == "es") return Language::spanish;
    if (code == "fr") return Language::french;
    return Language::english;
}

// Editors share a language for their lifetime so opening another plug-in instance
// cannot mix a new set of dynamic labels with an existing editor's static labels.
class EditorLocale
{
public:
    explicit EditorLocale (Language preference)
    {
        if (openEditors++ == 0)
            active = preference;
    }
    ~EditorLocale() { --openEditors; }

    static Language language() { return active; }

private:
    inline static Language active = Language::english;
    inline static int openEditors = 0;

    JUCE_DECLARE_NON_COPYABLE (EditorLocale)
};

inline juce::String tr (const char* english)
{
    if (EditorLocale::language() != Language::english)
    {
        struct Translations { const char* spanish; const char* french; };
        static const std::unordered_map<std::string_view, Translations> catalog {
            #include "Translations.inc"
        };
        if (const auto match = catalog.find (english); match != catalog.end())
            return juce::String::fromUTF8 (EditorLocale::language() == Language::spanish
                                              ? match->second.spanish : match->second.french);
    }
    return juce::String::fromUTF8 (english);
}

inline juce::String noteSpelling (std::string_view name, Language language = EditorLocale::language())
{
    if (name.empty())
        return {};

    if (language == Language::english || name.front() < 'A' || name.front() > 'G')
        return juce::String::fromUTF8 (name.data(), (int) name.size());

    constexpr const char* spanish[] { "La", "Si", "Do", "Re", "Mi", "Fa", "Sol" };
    constexpr const char* french[] { "La", "Si", "Do", u8"R\u00e9", "Mi", "Fa", "Sol" };
    const auto& notes = language == Language::spanish ? spanish : french;
    auto result = juce::String::fromUTF8 (notes[name.front() - 'A']);
    for (const char accidental : name.substr (1))
        result += juce::String::charToString (accidental == '#' ? 0x266f : 0x266d);
    return result;
}

inline juce::String pitchClassName (int pitchClass, int tonic, int scale)
{
    return noteSpelling (cs::pitchClassName (pitchClass, tonic, scale));
}

inline juce::String chordName (cs::Chord chord, int tonic, int scale)
{
    auto name = pitchClassName (chord.root, tonic, scale)
              + juce::String::fromUTF8 (cs::chordTypes()[(size_t) chord.type].suffix);
    if (chord.isSlash())
        name << "/" << pitchClassName (chord.bassPc(), tonic, scale);
    return name;
}
} // namespace ui
