#include "ParameterIds.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <memory>

// Renders the editor offscreen into PNG files: phu-screenshot <output-dir>. Needs a display on Linux (xvfb-run).
namespace {

using phu::plugin::parameter_ids::LaneParameter;

void setLaneParameter(PluginProcessor& processor, int lane, LaneParameter parameter, float plainValue)
{
    auto* target = processor.parameters().getParameter(phu::plugin::parameter_ids::laneParameterId(lane, parameter));
    target->setValueNotifyingHost(target->convertTo0to1(plainValue));
}

void populateDemoState(PluginProcessor& processor)
{
    using phu::sequencing::VelocityMode;
    auto& store = processor.patternStore();
    setLaneParameter(processor, 1, LaneParameter::latch, 1);
    setLaneParameter(processor, 1, LaneParameter::inputMode, 1);
    setLaneParameter(processor, 1, LaneParameter::velocityMode, 1);
    setLaneParameter(processor, 2, LaneParameter::timingSource, 1);
    setLaneParameter(processor, 2, LaneParameter::velocityMode, 2);
    setLaneParameter(processor, 3, LaneParameter::mute, 1);
    setLaneParameter(processor, 3, LaneParameter::velocityMode, 3);
    setLaneParameter(processor, 4, LaneParameter::stepCount, 7);
    setLaneParameter(processor, 6, LaneParameter::stepCount, 12);

    store.setTextGridText(2, "1/8t, 1/8., 1/4, 1/16, 1/16, 1/8");
    store.setTextGridText(3, "1/8, 1/7, 1/16");  // 1/7 is invalid and shown in red
    store.setStepEnabled(0, 1, false);
    store.setStepValue(1, 0, VelocityMode::byStep, 100);
    store.setStepValue(1, 2, VelocityMode::byStep, 40);
    store.setStepValue(2, 0, VelocityMode::deltaByStep, 40);
    store.setStepValue(2, 1, VelocityMode::deltaByStep, -25);
}

bool writeSnapshot(juce::AudioProcessorEditor& editor, int width, int height, const juce::File& file)
{
    editor.setSize(width, height);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    file.deleteFile();
    juce::FileOutputStream stream(file);
    return stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: phu-screenshot <output-dir>\n";
        return 2;
    }
    const juce::File outputDir = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    if (!outputDir.createDirectory()) {
        std::cerr << "cannot create " << outputDir.getFullPathName() << "\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI initialiser;
    PluginProcessor processor;
    populateDemoState(processor);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

    const bool ok = writeSnapshot(*editor, 1280, 820, outputDir.getChildFile("editor-default.png"))
                 && writeSnapshot(*editor, 1000, 420, outputDir.getChildFile("editor-min-size.png"));
    editor.reset();
    return ok ? 0 : 1;
}
