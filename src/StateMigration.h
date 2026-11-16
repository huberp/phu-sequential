#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <optional>
#include <span>

namespace phu::plugin {

// One function per version step: tree in version v -> tree in version v+1 (it must set formatVersion).
using MigrationFunction = juce::ValueTree (*)(juce::ValueTree);

// Production chain; element v-1 migrates v -> v+1. Empty while currentFormatVersion == 1.
[[nodiscard]] std::span<const MigrationFunction> productionMigrations() noexcept;

// Applies the chain from the tree's formatVersion up to targetVersion. Returns nullopt if the version is
// invalid, newer than the target, or the chain has a gap.
[[nodiscard]] std::optional<juce::ValueTree> migrateToVersion(juce::ValueTree, int targetVersion,
                                                              std::span<const MigrationFunction> chain);

}  // namespace phu::plugin
