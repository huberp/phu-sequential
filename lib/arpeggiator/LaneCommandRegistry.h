#pragma once

#include "arpeggiator/LaneCommand.h"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace phu::arpeggiator {

// Positional arguments of a registry-created command; missing integers read as 0.
struct CommandArguments {
    std::vector<int> numbers;
    std::string text;

    [[nodiscard]] int number(std::size_t index) const noexcept { return index < numbers.size() ? numbers[index] : 0; }
};

// Name -> factory map for dynamic UI generation (sec. 24.8). Instance owned by the adapter, no singleton (D-56). [MT]
class LaneCommandRegistry {
public:
    using Factory =
        std::function<std::unique_ptr<LaneCommand>(LaneStateAccess&, int laneIndex, const CommandArguments&)>;

    // Re-registering a name replaces its factory.
    void registerCommand(const std::string& name, Factory factory);

    // nullptr for unknown names.
    [[nodiscard]] std::unique_ptr<LaneCommand> createCommand(const std::string& name, LaneStateAccess& access,
                                                             int laneIndex, const CommandArguments& = {}) const;

    [[nodiscard]] std::vector<std::string> getCommandNames() const;  // registration order

    // Registers every command of sec. 24.2 under its class name without the "Command" suffix.
    void registerDefaultCommands();

private:
    std::vector<std::pair<std::string, Factory>> entries_;
};

}  // namespace phu::arpeggiator
