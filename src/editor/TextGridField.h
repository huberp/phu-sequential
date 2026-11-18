#pragma once

#include "LaneEditHistory.h"
#include "sequencing/TextGridParser.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace phu::plugin::editor {

// Editable text grid of one lane; invalid entries are overlaid in red (sec. 5.3). Editable regardless of the
// Timing Source (sec. 14). [MT]
class TextGridField final : public juce::Component {
public:
    TextGridField(LaneEditHistory&, int laneIndex);

    void setText(const std::string& text);  // no-op if identical, keeps the caret while typing

    void resized() override;
    void paintOverChildren(juce::Graphics&) override;

private:
    void textChanged();
    void refreshErrors();

    LaneEditHistory& edits_;
    int laneIndex_;
    juce::TextEditor editor_;
    std::vector<sequencing::TextGridError> errors_;
};

}  // namespace phu::plugin::editor
