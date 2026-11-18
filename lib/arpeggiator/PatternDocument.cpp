#include "arpeggiator/PatternDocument.h"

#include "sequencing/StepData.h"

#include <algorithm>
#include <utility>

namespace phu::arpeggiator {

namespace {

using namespace sequencing::constants;

int clampLane(int laneIndex) noexcept {
    return std::clamp(laneIndex, 0, lanes::laneCount - 1);
}

bool validLane(int laneIndex) noexcept {
    return laneIndex >= 0 && laneIndex < lanes::laneCount;
}

bool validStep(int stepIndex) noexcept {
    return stepIndex >= 0 && stepIndex < lanes::maximumStepCount;
}

}  // namespace

LaneColor defaultLaneColor(int laneIndex) noexcept {
    const std::uint32_t rgb = colors::defaultLanePalette[static_cast<std::size_t>(clampLane(laneIndex))];
    return LaneColor{static_cast<std::uint8_t>((rgb >> 16) & 0xFF), static_cast<std::uint8_t>((rgb >> 8) & 0xFF),
                     static_cast<std::uint8_t>(rgb & 0xFF)};
}

PatternDocument::PatternDocument() {
    resetToDefaults();
}

void PatternDocument::resetToDefaults() {
    for (int i = 0; i < lanes::laneCount; ++i) {
        lanes_[static_cast<std::size_t>(i)] = LaneDocument{{}, {}, defaultLaneColor(i)};
    }
    ++revision_;
}

const LaneDocument& PatternDocument::lane(int laneIndex) const noexcept {
    return lanes_[static_cast<std::size_t>(clampLane(laneIndex))];
}

void PatternDocument::setStepEnabled(int laneIndex, int stepIndex, bool enabled) {
    if (!validLane(laneIndex) || !validStep(stepIndex)) {
        return;
    }
    auto& pattern = lanes_[static_cast<std::size_t>(laneIndex)].pattern;
    pattern.steps.setEnabled(stepIndex, enabled);
    pattern.stepEditRevision[static_cast<std::size_t>(stepIndex)] = ++revision_;
}

void PatternDocument::setStepValue(int laneIndex, int stepIndex, sequencing::VelocityMode mode, int value) {
    if (!validLane(laneIndex) || !validStep(stepIndex)) {
        return;
    }
    auto& pattern = lanes_[static_cast<std::size_t>(laneIndex)].pattern;
    pattern.steps.setValue(stepIndex, mode, value);
    pattern.stepEditRevision[static_cast<std::size_t>(stepIndex)] = ++revision_;
}

sequencing::TextGridParseResult PatternDocument::setTextGridText(int laneIndex, std::string text) {
    auto result = sequencing::parseTextGrid(text);
    if (!validLane(laneIndex)) {
        return result;
    }
    auto& doc = lanes_[static_cast<std::size_t>(laneIndex)];
    doc.textGridText = std::move(text);
    if (result.grid) {
        doc.pattern.appliedTextGrid = *result.grid;
    }
    ++revision_;
    return result;
}

void PatternDocument::setLaneColor(int laneIndex, LaneColor color) {
    if (!validLane(laneIndex)) {
        return;
    }
    lanes_[static_cast<std::size_t>(laneIndex)].color = color;
    ++revision_;
}

void PatternDocument::setLane(int laneIndex, LaneDocument document) {
    if (!validLane(laneIndex)) {
        return;
    }
    auto& target = lanes_[static_cast<std::size_t>(laneIndex)];
    document.pattern.acknowledgedDiscardRevision = target.pattern.acknowledgedDiscardRevision;
    target = std::move(document);
    ++revision_;
    target.pattern.stepEditRevision.fill(revision_);
}

void PatternDocument::enforceStepCountInvariant(int laneIndex, const LaneParameters& parameters) {
    if (!validLane(laneIndex)) {
        return;
    }
    auto& pattern = lanes_[static_cast<std::size_t>(laneIndex)].pattern;
    pattern.steps.applyStepCountChange(0, effectiveStepCount(parameters, pattern.appliedTextGrid));
    ++revision_;
}

PatternSnapshot PatternDocument::makeSnapshot() const {
    PatternSnapshot snapshot;
    for (int i = 0; i < lanes::laneCount; ++i) {
        snapshot.lanes[static_cast<std::size_t>(i)] = lanes_[static_cast<std::size_t>(i)].pattern;
    }
    snapshot.documentRevision = revision_;
    return snapshot;
}

bool operator==(const LaneDocument& x, const LaneDocument& y) noexcept {
    return x.pattern.steps == y.pattern.steps && x.pattern.appliedTextGrid == y.pattern.appliedTextGrid &&
           x.textGridText == y.textGridText && x.color == y.color;
}

bool operator==(const PatternDocument& a, const PatternDocument& b) noexcept {
    for (int i = 0; i < lanes::laneCount; ++i) {
        if (!(a.lanes_[static_cast<std::size_t>(i)] == b.lanes_[static_cast<std::size_t>(i)])) {
            return false;
        }
    }
    return true;
}

}  // namespace phu::arpeggiator
