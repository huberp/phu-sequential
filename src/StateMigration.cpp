#include "StateMigration.h"

namespace phu::plugin {

std::span<const MigrationFunction> productionMigrations() noexcept {
    return {};
}

std::optional<juce::ValueTree> migrateToVersion(juce::ValueTree tree, int targetVersion,
                                                std::span<const MigrationFunction> chain) {
    if (!tree.isValid()) {
        return std::nullopt;
    }
    int version = static_cast<int>(tree.getProperty("formatVersion", 0));
    if (version < 1 || version > targetVersion) {
        return std::nullopt;
    }
    while (version < targetVersion) {
        const auto index = static_cast<std::size_t>(version - 1);
        if (index >= chain.size() || chain[index] == nullptr) {
            return std::nullopt;
        }
        tree = chain[index](tree);
        if (!tree.isValid() || static_cast<int>(tree.getProperty("formatVersion", 0)) != version + 1) {
            return std::nullopt;
        }
        ++version;
    }
    return tree;
}

}  // namespace phu::plugin
