#include "editor/LaneControlStrip.h"

#include "ParameterIds.h"
#include "sequencing/Constants.h"
#include "sequencing/sources/SourceSelection.h"

#include <string>

namespace phu::plugin::editor {

namespace {

using parameter_ids::LaneParameter;

enum LaneCommandItem {
    reverseSteps = 1,
    rotateLeft,
    rotateRight,
    invertSteps,
    clearLane,
    fillLane,
    fillRandom,
    arithmeticSequence,
    euclideanPattern
};

juce::RangedAudioParameter& parameterOf(juce::AudioProcessorValueTreeState& apvts, int lane, LaneParameter parameter)
{
    return *apvts.getParameter(parameter_ids::laneParameterId(lane, parameter));
}

void styleToggle(juce::TextButton& button)
{
    button.setClickingTogglesState(true);
}

}  // namespace

LaneColorPicker::LaneColorPicker(LaneEditHistory& edits, int laneIndex) : edits_(edits), laneIndex_(laneIndex) {}

void LaneColorPicker::setColour(juce::Colour colour)
{
    if (colour != colour_) {
        colour_ = colour;
        repaint();
    }
}

void LaneColorPicker::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(colour_);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    g.setFont(12.0f);
    g.drawText(juce::String::fromUTF8("Color \xe2\x96\xbe"), getLocalBounds(), juce::Justification::centred, false);
}

void LaneColorPicker::mouseUp(const juce::MouseEvent& event)
{
    if (!getLocalBounds().contains(event.getPosition())) {
        return;
    }

    class Picker final : public juce::Component, private juce::ChangeListener {
    public:
        Picker(LaneEditHistory& edits, int lane, juce::Colour initial)
            : selector_(juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders, 4, 4),
              edits_(edits), lane_(lane)
        {
            selector_.setCurrentColour(initial, juce::dontSendNotification);
            selector_.addChangeListener(this);
            addAndMakeVisible(selector_);
            setSize(240, 260);
        }
        ~Picker() override
        {
            selector_.removeChangeListener(this);
            edits_.endGesture();
        }
        void resized() override { selector_.setBounds(getLocalBounds()); }

    private:
        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            const auto colour = selector_.getCurrentColour();
            const arpeggiator::LaneColor color{colour.getRed(), colour.getGreen(), colour.getBlue()};
            const int lane = lane_;
            edits_.executeCoalesced("laneColor" + std::to_string(lane), [lane, color](arpeggiator::LaneStateAccess& access) {
                return std::make_unique<arpeggiator::SetLaneColorCommand>(access, lane, color);
            });
        }
        juce::ColourSelector selector_;
        LaneEditHistory& edits_;
        int lane_;
    };

    juce::CallOutBox::launchAsynchronously(std::make_unique<Picker>(edits_, laneIndex_, colour_),
                                           getScreenBounds(), nullptr);
}

LaneControlStrip::LaneControlStrip(juce::AudioProcessorValueTreeState& apvts, LaneEditHistory& edits, int laneIndex,
                                   juce::Component& parent)
    : apvts_(apvts), laneIndex_(laneIndex), colorPicker_(edits, laneIndex)
{
    for (auto* component : std::initializer_list<juce::Component*>{
             &mute_, &latch_, &inputMode_, &followIndex_, &followChannel_, &outputChannel_, &transpose_, &gate_,
             &timingBase_, &timingGrid_, &laneCommands_, &baseTiming_, &subdivision_, &swing_, &stepCount_, &velocityMode_,
             &colorPicker_}) {
        parent.addAndMakeVisible(*component);
    }
    laneCommands_.setName("Lane commands");
    laneCommands_.setTextWhenNothingSelected("Lane commands");
    laneCommands_.addItem("Reverse", reverseSteps);
    laneCommands_.addItem("Rotate left", rotateLeft);
    laneCommands_.addItem("Rotate right", rotateRight);
    laneCommands_.addItem("Invert", invertSteps);
    laneCommands_.addItem("Clear lane", clearLane);
    laneCommands_.addItem("Fill lane", fillLane);
    laneCommands_.addSeparator();
    laneCommands_.addItem("Fill random...", fillRandom);
    laneCommands_.addItem("Arithmetic sequence...", arithmeticSequence);
    laneCommands_.addItem("Euclidean pattern...", euclideanPattern);
    for (const int item : {fillRandom, arithmeticSequence, euclideanPattern}) {
        laneCommands_.setItemEnabled(item, false);
    }
    laneCommands_.onChange = [this, &edits] {
        const int selected = laneCommands_.getSelectedId();
        const bool enabled = laneCommands_.isItemEnabled(selected);
        laneCommands_.setSelectedId(0, juce::dontSendNotification);
        if (!enabled) {
            return;
        }
        const int lane = laneIndex_;
        switch (selected) {
        case reverseSteps:
            edits.execute([lane](arpeggiator::LaneStateAccess& access) {
                return std::make_unique<arpeggiator::ReverseStepsCommand>(access, lane);
            });
            break;
        case rotateLeft:
        case rotateRight:
            edits.execute([lane, selected](arpeggiator::LaneStateAccess& access) {
                return std::make_unique<arpeggiator::RotateStepsCommand>(access, lane, selected == rotateLeft ? -1 : 1);
            });
            break;
        case invertSteps:
            edits.execute([lane](arpeggiator::LaneStateAccess& access) {
                return std::make_unique<arpeggiator::InvertStepsCommand>(access, lane);
            });
            break;
        case clearLane:
            edits.execute([lane](arpeggiator::LaneStateAccess& access) {
                return std::make_unique<arpeggiator::ClearLaneCommand>(access, lane);
            });
            break;
        case fillLane:
            edits.execute([lane](arpeggiator::LaneStateAccess& access) {
                return std::make_unique<arpeggiator::FillLaneCommand>(access, lane);
            });
            break;
        default:
            jassertfalse;
            break;
        }
    };
    styleToggle(mute_);
    styleToggle(latch_);
    mute_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc0392b));
    timingBase_.setRadioGroupId(1 + laneIndex);
    timingGrid_.setRadioGroupId(1 + laneIndex);
    timingBase_.setClickingTogglesState(true);
    timingGrid_.setClickingTogglesState(true);

    auto& inputModeParameter = parameterOf(apvts, laneIndex, LaneParameter::inputMode);
    populateChoices(inputMode_, inputModeParameter);
    auto& baseTimingParameter = parameterOf(apvts, laneIndex, LaneParameter::baseTiming);
    populateChoices(baseTiming_, baseTimingParameter);
    auto& subdivisionParameter = parameterOf(apvts, laneIndex, LaneParameter::subdivision);
    populateChoices(subdivision_, subdivisionParameter);
    auto& velocityParameter = parameterOf(apvts, laneIndex, LaneParameter::velocityMode);
    populateChoices(velocityMode_, velocityParameter);

    muteAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
        parameterOf(apvts, laneIndex, LaneParameter::mute), mute_);
    latchAttachment_ = std::make_unique<juce::ButtonParameterAttachment>(
        parameterOf(apvts, laneIndex, LaneParameter::latch), latch_);
    inputModeAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(inputModeParameter, inputMode_);
    baseTimingAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(baseTimingParameter, baseTiming_);
    subdivisionAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(subdivisionParameter, subdivision_);
    velocityModeAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(velocityParameter, velocityMode_);

    const auto attachBar = [&](juce::Slider& slider, LaneParameter parameter, const juce::String& label,
                               std::unique_ptr<juce::SliderParameterAttachment>& attachment,
                               const juce::String& suffix = {}) {
        configureBarSlider(slider, label);
        attachment = std::make_unique<juce::SliderParameterAttachment>(parameterOf(apvts, laneIndex, parameter), slider);
        setBarSliderText(slider, label, suffix);
    };
    attachBar(followIndex_, LaneParameter::followIndex, "Index", followIndexAttachment_);
    attachBar(followChannel_, LaneParameter::followChannel, "Ch", followChannelAttachment_);
    attachBar(outputChannel_, LaneParameter::outputChannel, "Out Ch", outputChannelAttachment_);
    attachBar(transpose_, LaneParameter::transpose, "Transpose", transposeAttachment_);
    attachBar(gate_, LaneParameter::gate, "Gate", gateAttachment_, "%");
    attachBar(swing_, LaneParameter::swing, "Swing", swingAttachment_, "%");
    attachBar(stepCount_, LaneParameter::stepCount, "Steps", stepCountAttachment_);

    auto& timingParameter = parameterOf(apvts, laneIndex, LaneParameter::timingSource);
    timingSourceAttachment_ = std::make_unique<juce::ParameterAttachment>(timingParameter, [this](float value) {
        const bool grid = value > 0.5f;
        timingBase_.setToggleState(!grid, juce::dontSendNotification);
        timingGrid_.setToggleState(grid, juce::dontSendNotification);
    });
    timingBase_.onClick = [this] { timingSourceAttachment_->setValueAsCompleteGesture(0.0f); };
    timingGrid_.onClick = [this] { timingSourceAttachment_->setValueAsCompleteGesture(1.0f); };
    timingSourceAttachment_->sendInitialUpdate();
}

void LaneControlStrip::layOut(const layout::LaneRowRegions& regions)
{
    auto sidebar = regions.sidebar;
    const int badgeWidth = layout::controlHeight;
    auto top = sidebar.removeFromTop(layout::controlHeight);
    top.removeFromLeft(badgeWidth + layout::gap);  // lane number badge is painted by the lane row
    colorPicker_.setBounds(top);
    sidebar.removeFromTop(layout::gap);
    auto toggles = layout::splitHorizontally(sidebar.removeFromTop(layout::controlHeight), {1, 1});
    mute_.setBounds(toggles[0]);
    latch_.setBounds(toggles[1]);
    sidebar.removeFromTop(layout::gap);
    auto source = layout::splitHorizontally(sidebar.removeFromTop(layout::controlHeight), {1, 1});
    timingBase_.setBounds(source[0]);
    timingGrid_.setBounds(source[1]);
    sidebar.removeFromTop(layout::gap);
    laneCommands_.setBounds(sidebar.removeFromTop(layout::controlHeight));

    auto row1 = layout::splitHorizontally(regions.controlRow1, {3, 2, 2, 2, 2});
    inputMode_.setBounds(row1[0]);
    followIndex_.setBounds(row1[1]);
    followChannel_.setBounds(row1[1]);
    outputChannel_.setBounds(row1[2]);
    transpose_.setBounds(row1[3]);
    gate_.setBounds(row1[4]);

    auto row2 = layout::splitHorizontally(regions.controlRow2, {2, 2, 2, 2, 3});
    baseTiming_.setBounds(row2[0]);
    subdivision_.setBounds(row2[1]);
    swing_.setBounds(row2[2]);
    stepCount_.setBounds(row2[3]);
    velocityMode_.setBounds(row2[4]);
}

void LaneControlStrip::update(const arpeggiator::LaneParameters& parameters, juce::Colour laneColour)
{
    const bool chordFollow = parameters.inputMode == sequencing::InputMode::chordFollow;
    followIndex_.setVisible(chordFollow);
    followChannel_.setVisible(!chordFollow);

    // Base-timing controls are disabled while the Timing Source is Text Grid (sec. 14).
    const bool baseTimingActive = parameters.timingSource == arpeggiator::TimingSource::baseTiming;
    for (auto* component : std::initializer_list<juce::Component*>{&baseTiming_, &subdivision_, &swing_, &stepCount_}) {
        component->setEnabled(baseTimingActive);
    }
    colorPicker_.setColour(laneColour);
}

}  // namespace phu::plugin::editor
