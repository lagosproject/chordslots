#include <clap/clap.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <clap/factory/plugin-factory.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
 #define NOMINMAX
 #include <windows.h>
#else
 #include <dlfcn.h>
#endif

namespace
{
struct DynamicLibrary
{
#if defined(_WIN32)
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif

    explicit DynamicLibrary(const char* path)
    {
#if defined(_WIN32)
        handle = LoadLibraryA(path);
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    }

    ~DynamicLibrary()
    {
#if defined(_WIN32)
        if (handle != nullptr)
            FreeLibrary(handle);
#else
        if (handle != nullptr)
            dlclose(handle);
#endif
    }

    void* symbol(const char* name) const
    {
#if defined(_WIN32)
        return handle != nullptr ? reinterpret_cast<void*>(GetProcAddress(handle, name)) : nullptr;
#else
        return handle != nullptr ? dlsym(handle, name) : nullptr;
#endif
    }
};

struct EventInput
{
    std::vector<const clap_event_header_t*> events;
};

struct EventOutput
{
    std::vector<std::array<uint8_t, 3>> midi;
};

uint32_t CLAP_ABI inputSize(const clap_input_events_t* list)
{
    return static_cast<uint32_t>(static_cast<EventInput*>(list->ctx)->events.size());
}

const clap_event_header_t* CLAP_ABI inputGet(const clap_input_events_t* list, uint32_t index)
{
    const auto& events = static_cast<EventInput*>(list->ctx)->events;
    return index < events.size() ? events[index] : nullptr;
}

bool CLAP_ABI outputPush(const clap_output_events_t* list, const clap_event_header_t* event)
{
    if (event == nullptr || event->space_id != CLAP_CORE_EVENT_SPACE_ID || event->type != CLAP_EVENT_MIDI)
        return true;

    const auto* midi = reinterpret_cast<const clap_event_midi_t*>(event);
    static_cast<EventOutput*>(list->ctx)->midi.push_back({ midi->data[0], midi->data[1], midi->data[2] });
    return true;
}

const void* CLAP_ABI hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
void CLAP_ABI hostRequestRestart(const clap_host_t*) {}
void CLAP_ABI hostRequestProcess(const clap_host_t*) {}
void CLAP_ABI hostRequestCallback(const clap_host_t*) {}

struct Host
{
    clap_host_t clap {
        CLAP_VERSION,
        nullptr,
        "ChordSlots MIDI smoke",
        "ChordSlots",
        "https://chordslots.org",
        "0.1",
        hostGetExtension,
        hostRequestRestart,
        hostRequestProcess,
        hostRequestCallback
    };
};

struct ProcessBuffer
{
    static constexpr uint32_t frames = 256;
    std::array<std::array<float, frames>, 2> samples {};
    std::array<float*, 2> channels { samples[0].data(), samples[1].data() };
    clap_audio_buffer_t output { channels.data(), nullptr, 2, 0, 0 };
};

clap_event_midi_t makeMidiNote(uint8_t status, int key)
{
    clap_event_midi_t event {};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_MIDI;
    event.header.flags = CLAP_EVENT_IS_LIVE;
    event.port_index = 0;
    event.data[0] = status;
    event.data[1] = static_cast<uint8_t>(key);
    event.data[2] = status == 0x90 ? 0x7f : 0;
    return event;
}

bool run(const char* pluginPath)
{
    DynamicLibrary library(pluginPath);
    if (library.handle == nullptr)
    {
        std::cerr << "Could not load CLAP library: " << pluginPath << '\n';
        return false;
    }

    auto* entry = static_cast<const clap_plugin_entry_t*>(library.symbol("clap_entry"));
    if (entry == nullptr || ! entry->init(pluginPath))
    {
        std::cerr << "CLAP entry init failed\n";
        return false;
    }

    bool entryInitialized = true;
    const auto cleanupEntry = [&]
    {
        if (entryInitialized)
        {
            entry->deinit();
            entryInitialized = false;
        }
    };

    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (factory == nullptr || factory->get_plugin_count(factory) == 0)
    {
        std::cerr << "CLAP plugin factory is missing or empty\n";
        cleanupEntry();
        return false;
    }

    const clap_plugin_descriptor_t* descriptor = nullptr;
    for (uint32_t i = 0; i < factory->get_plugin_count(factory); ++i)
    {
        const auto* candidate = factory->get_plugin_descriptor(factory, i);
        if (candidate != nullptr && std::string(candidate->id) == "org.chordslots.ChordSlots")
        {
            descriptor = candidate;
            break;
        }
    }
    if (descriptor == nullptr)
    {
        std::cerr << "Plugin ID org.chordslots.ChordSlots was not found\n";
        cleanupEntry();
        return false;
    }

    Host host;
    const clap_plugin_t* plugin = factory->create_plugin(factory, &host.clap, descriptor->id);
    if (plugin == nullptr)
    {
        std::cerr << "CLAP plugin instance initialization failed\n";
        cleanupEntry();
        return false;
    }
    if (! plugin->init(plugin))
    {
        std::cerr << "CLAP plugin instance initialization failed\n";
        plugin->destroy(plugin);
        cleanupEntry();
        return false;
    }

    bool active = false;
    bool processing = false;
    bool success = false;
    do
    {
        const auto* notePorts = static_cast<const clap_plugin_note_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
        if (notePorts == nullptr || notePorts->count(plugin, true) != 1 || notePorts->count(plugin, false) != 1)
        {
            std::cerr << "Expected exactly one CLAP MIDI note input and output port\n";
            break;
        }

        clap_note_port_info_t inputInfo {};
        clap_note_port_info_t outputInfo {};
        if (! notePorts->get(plugin, 0, true, &inputInfo) || ! notePorts->get(plugin, 0, false, &outputInfo)
            || (inputInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI) == 0
            || (outputInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI) == 0)
        {
            std::cerr << "CLAP note ports do not advertise the MIDI dialect\n";
            break;
        }

        const auto* audioPorts = static_cast<const clap_plugin_audio_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
        if (audioPorts == nullptr || audioPorts->count(plugin, false) != 1)
        {
            std::cerr << "Expected one output audio port for instrument activation\n";
            break;
        }
        clap_audio_port_info_t outputAudioInfo {};
        if (! audioPorts->get(plugin, 0, false, &outputAudioInfo) || outputAudioInfo.channel_count != 2)
        {
            std::cerr << "Expected stereo audio output\n";
            break;
        }

        if (! plugin->activate(plugin, 44100.0, ProcessBuffer::frames, ProcessBuffer::frames))
        {
            std::cerr << "CLAP activate failed\n";
            break;
        }
        active = true;
        if (! plugin->start_processing(plugin))
        {
            std::cerr << "CLAP start_processing failed\n";
            break;
        }
        processing = true;

        EventInput input;
        EventOutput output;
        clap_input_events_t inputList { &input, inputSize, inputGet };
        clap_output_events_t outputList { &output, outputPush };
        ProcessBuffer buffer;
        clap_process_t process {};
        process.steady_time = 0;
        process.frames_count = ProcessBuffer::frames;
        process.audio_outputs = &buffer.output;
        process.audio_outputs_count = 1;
        process.in_events = &inputList;
        process.out_events = &outputList;

        auto noteOn = makeMidiNote(0x90, 60); // default trigger key: C3 -> first chord
        input.events = { &noteOn.header };
        if (plugin->process(plugin, &process) == CLAP_PROCESS_ERROR)
        {
            std::cerr << "CLAP process rejected the note-on block\n";
            break;
        }
        process.steady_time += ProcessBuffer::frames;

        // Give any strummed notes time to leave the processor's bounded pending-note queue.
        input.events.clear();
        bool processOk = true;
        for (int block = 0; block < 64; ++block)
        {
            for (auto& channel : buffer.samples)
                channel.fill(0.0f);
            if (plugin->process(plugin, &process) == CLAP_PROCESS_ERROR)
            {
                std::cerr << "CLAP process rejected a continuation block\n";
                processOk = false;
                break;
            }
            process.steady_time += ProcessBuffer::frames;
        }
        if (! processOk)
            break;
        const auto noteOff = makeMidiNote(0x80, 60);
        input.events = { &noteOff.header };
        for (auto& channel : buffer.samples)
            channel.fill(0.0f);
        if (plugin->process(plugin, &process) == CLAP_PROCESS_ERROR)
        {
            std::cerr << "CLAP process rejected the note-off block\n";
            break;
        }

        int noteOns = 0;
        int noteOffs = 0;
        for (const auto& message : output.midi)
        {
            const auto status = message[0] & 0xf0;
            if (status == 0x90 && message[2] != 0)
                ++noteOns;
            else if (status == 0x80 || (status == 0x90 && message[2] == 0))
                ++noteOffs;
        }
        if (noteOns == 0 || noteOffs == 0)
        {
            std::cerr << "Expected output MIDI note-on and note-off; observed "
                      << noteOns << " on, " << noteOffs << " off\n";
            break;
        }

        std::cout << "CLAP MIDI smoke passed: MIDI note ports in/out, " << noteOns
                  << " output note-ons and " << noteOffs << " output note-offs\n";
        success = true;
    } while (false);

    if (processing)
        plugin->stop_processing(plugin);
    if (active)
        plugin->deactivate(plugin);
    plugin->destroy(plugin);
    cleanupEntry();
    return success;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: chordslots_clap_midi_smoke <plugin.clap>\n";
        return 2;
    }
    return run(argv[1]) ? 0 : 1;
}
