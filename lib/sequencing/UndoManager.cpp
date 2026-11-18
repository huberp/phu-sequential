#include "sequencing/UndoManager.h"

#include <algorithm>
#include <utility>

namespace phu::sequencing {

namespace {

class MacroCommand final : public Command {
public:
    explicit MacroCommand(std::vector<std::unique_ptr<Command>> commands) : commands_(std::move(commands)) {}

    void execute() override {
        for (auto& command : commands_) {
            command->execute();
        }
    }

    void undo() override {
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            (*it)->undo();
        }
    }

    [[nodiscard]] std::string getName() const override { return commands_.front()->getName(); }

private:
    std::vector<std::unique_ptr<Command>> commands_;
};

}  // namespace

UndoManager::UndoManager(std::size_t maxDepth) : maxDepth_(std::max<std::size_t>(maxDepth, 1)) {}

void UndoManager::push(std::unique_ptr<Command> executed) {
    redoStack_.clear();
    undoStack_.push_back(std::move(executed));
    while (undoStack_.size() > maxDepth_) {
        undoStack_.pop_front();
    }
}

void UndoManager::execute(std::unique_ptr<Command> command) {
    if (!command) {
        return;
    }
    command->execute();
    push(std::move(command));
}

void UndoManager::executeMacro(std::vector<std::unique_ptr<Command>> commands) {
    std::erase_if(commands, [](const auto& command) { return !command; });
    if (commands.empty()) {
        return;
    }
    auto macro = std::make_unique<MacroCommand>(std::move(commands));
    macro->execute();
    push(std::move(macro));
}

void UndoManager::undo() {
    if (undoStack_.empty()) {
        return;
    }
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    command->undo();
    redoStack_.push_back(std::move(command));
}

void UndoManager::redo() {
    if (redoStack_.empty()) {
        return;
    }
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    command->execute();
    undoStack_.push_back(std::move(command));
    while (undoStack_.size() > maxDepth_) {
        undoStack_.pop_front();
    }
}

void UndoManager::clear() noexcept {
    undoStack_.clear();
    redoStack_.clear();
}

std::string UndoManager::getUndoName() const {
    return undoStack_.empty() ? std::string{} : undoStack_.back()->getName();
}

std::string UndoManager::getRedoName() const {
    return redoStack_.empty() ? std::string{} : redoStack_.back()->getName();
}

}  // namespace phu::sequencing
