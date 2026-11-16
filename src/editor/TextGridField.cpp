#include "editor/TextGridField.h"

#include "editor/EditorLookAndFeel.h"

#include <algorithm>

namespace phu::plugin::editor {

namespace {

// Errors carry byte offsets of the UTF-8 text; TextEditor addresses characters.
int characterIndex(const std::string& text, std::size_t byteOffset)
{
    const auto clamped = static_cast<int>(std::min(byteOffset, text.size()));
    return juce::String::fromUTF8(text.data(), clamped).length();
}

}  // namespace

TextGridField::TextGridField(PatternStore& store, int laneIndex) : store_(store), laneIndex_(laneIndex)
{
    editor_.setMultiLine(false);
    editor_.setTextToShowWhenEmpty("no grid", juce::Colours::grey);
    editor_.setFont(juce::FontOptions(14.0f));
    editor_.onTextChange = [this] { textChanged(); };
    addAndMakeVisible(editor_);
}

void TextGridField::setText(const std::string& text)
{
    if (editor_.getText().toStdString() != text) {
        editor_.setText(juce::String::fromUTF8(text.c_str()), false);
        refreshErrors();
    }
}

void TextGridField::textChanged()
{
    store_.setTextGridText(laneIndex_, editor_.getText().toStdString());
    refreshErrors();
}

void TextGridField::refreshErrors()
{
    const auto text = editor_.getText().toStdString();
    errors_ = sequencing::parseTextGrid(text).errors;
    if (text.find_first_not_of(" \t") == std::string::npos) {
        errors_.clear();  // an empty field shows the "no grid" placeholder, not a red marker
    }
    repaint();
}

void TextGridField::resized()
{
    editor_.setBounds(getLocalBounds());
}

void TextGridField::paintOverChildren(juce::Graphics& g)
{
    const auto text = editor_.getText().toStdString();
    const int length = editor_.getText().length();
    g.setColour(EditorLookAndFeel::invalidMarker().withAlpha(0.45f));
    for (const auto& error : errors_) {
        int begin = characterIndex(text, error.textBegin);
        int end = characterIndex(text, error.textEnd);
        if (end <= begin) {
            begin = std::max(0, begin - 1);
            end = std::min(length, begin + 1);
        }
        if (end <= begin) {
            continue;
        }
        const auto rects = editor_.getTextBounds({begin, end});
        for (const auto& rect : rects) {
            g.fillRect(rect.translated(editor_.getX(), editor_.getY()));
        }
    }
}

}  // namespace phu::plugin::editor
