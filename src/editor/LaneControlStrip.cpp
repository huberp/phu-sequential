#include "editor/LaneControlStrip.h"

#include "ParameterIds.h"
#include "sequencing/Constants.h"
#include "sequencing/sources/SourceSelection.h"

namespace phu::plugin::editor {

namespace {

using parameter_ids::LaneParameter;

juce::RangedAudioParameter& parameterOf(juce::AudioProcessorValueTreeState& apvts, int lane, LaneParameter parameter)
{
    return *apvts.getParameter(parameter_ids::laneParameterId(lane, parameter));
}

void styleToggle(juce::TextButton& button)
{
    button.setClickingTogglesState(true);
}

}  // namespace

LaneColorPicker::LaneColorPicker(PatternStore& store, int laneIndex) : store_(store), laneIndex_(laneIndex) {}

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
        Picker(PatternStore& store, int lane, juce::Colour initial)
            : selector_(juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders, 4, 4),
              store_(store), lane_(lane)
        {
            selector_.setCurrentColour(initial, juce::dontSendNotification);
            selector_.addChangeListener(this);
            addAndMakeVisible(selector_);
            setSize(240, 260);
        }
        ~Picker() override { selector_.removeChangeListener(this); }
        void resized() override { selector_.setBounds(getLocalBounds()); }

    private:
        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            const auto colour = selector_.getCurrentColour();
            store_.setLaneColor(lane_, {colour.getRed(), colour.getGreen(), colour.getBlue()});
        }
        juce::ColourSelector selector_;
        PatternStore& store_;
        int lane_;
    };

    juce::CallOutBox::launchAsynchronously(std::make_unique<Picker>(store_, laneIndex_, colour_),
                                           getScreenBounds(), nullptr);
}

LaneControlStrip::LaneControlStrip(juce::AudioProcessorValueTreeState& apvts, PatternStore& store, int laneIndex,
                                   juce::Component& parent)
    : apvts_(apvts), laneIndex_(laneIndex), colorPicker_(store, laneIndex)
{
    for (auto* component : std::initializer_list<juce::Component*>{
             &mute_, &latch_, &inputMode_, &followIndex_, &followChannel_, &outputChannel_, &transpose_, &gate_,
             &timingBase_, &timingGrid_, &baseTiming_, &subdivision_, &swing_, &stepCount_, &velocityMode_,
             &colorPicker_}) {
        parent.addAndMakeVisible(*component);
    }
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
