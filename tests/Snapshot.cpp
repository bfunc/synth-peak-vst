// Draws every editor page to a PNG, with the "Perc Up GEO" dump loaded, so the UI can be checked without a synth or
// a Windows machine:  peak_snapshot <output folder>
// Links against the plugin's shared code; nothing is sent anywhere (no MIDI ports are opened in a container).

#include <juce_audio_utils/juce_audio_utils.h>

#include "core/Sysex.h"
#include "plugin/PeakProcessor.h"
#include "ui/PeakEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    out.createDirectory();

    auto proc = std::make_unique<PeakProcessor>();

    // Load the test dump into the editor as if it came from a file (no port open, so nothing is sent).
    juce::MemoryBlock mb;
    if (juce::File (juce::String (PEAK_TEST_DATA) + "/2026-11-13-editbuffer-perc-up-geo.syx").loadFileAsData (mb))
        if (auto msg = peak::parsePatchMessage (static_cast<const uint8_t*> (mb.getData()), mb.getSize()))
            proc->session().loadFromFile (msg->data, "perc-up-geo.syx");
    proc->setPlayheads (true);

    std::unique_ptr<PeakEditor> ed (dynamic_cast<PeakEditor*> (proc->createEditor()));
    ed->setSize (1360, 800);
    const char* names[] = { "main", "fx", "matrix", "presets", "setup" };
    for (int i = 0; i < 5; ++i)
    {
        ed->showPage (i);
        ed->refreshNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream os (out.getChildFile (juce::String ("page-") + names[i] + ".png"));
        os.setPosition (0);
        os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote page-%s.png\n", names[i]);
    }
    ed.reset();
    proc.reset();
    return 0;
}
