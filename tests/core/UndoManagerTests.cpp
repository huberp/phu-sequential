#include <catch2/catch_test_macros.hpp>

#include "sequencing/Constants.h"
#include "sequencing/UndoManager.h"

#include <memory>
#include <string>
#include <vector>

// M25: generic command / undo infrastructure (sec. 23).

using namespace phu::sequencing;

namespace {

struct Recorder {
    std::vector<std::string> log;
    int value = 0;
};

class AddCommand final : public Command {
public:
    AddCommand(Recorder& recorder, std::string name, int delta)
        : recorder_(recorder), name_(std::move(name)), delta_(delta) {}
    void execute() override {
        recorder_.value += delta_;
        recorder_.log.push_back("do " + name_);
    }
    void undo() override {
        recorder_.value -= delta_;
        recorder_.log.push_back("undo " + name_);
    }
    [[nodiscard]] std::string getName() const override { return name_; }

private:
    Recorder& recorder_;
    std::string name_;
    int delta_;
};

std::unique_ptr<Command> add(Recorder& r, const std::string& name, int delta) {
    return std::make_unique<AddCommand>(r, name, delta);
}

}  // namespace

TEST_CASE("UndoManager: execute, undo and redo restore exact state; new execute clears redo", "[M25][REQ-23-1]") {
    Recorder r;
    UndoManager manager;
    manager.execute(add(r, "A", 1));
    manager.execute(add(r, "B", 10));
    CHECK(r.value == 11);
    manager.undo();
    manager.undo();
    CHECK(r.value == 0);
    manager.redo();
    CHECK(r.value == 1);
    CHECK(manager.canRedo());
    manager.execute(add(r, "C", 100));
    CHECK(r.value == 101);
    CHECK_FALSE(manager.canRedo());
    manager.redo();  // no-op
    CHECK(r.value == 101);
}

TEST_CASE("UndoManager: macro is one undo step undone in reverse order", "[M25][REQ-23-1]") {
    Recorder r;
    UndoManager manager;
    std::vector<std::unique_ptr<Command>> macro;
    macro.push_back(add(r, "A", 1));
    macro.push_back(add(r, "B", 2));
    manager.executeMacro(std::move(macro));
    CHECK(r.value == 3);
    manager.undo();
    CHECK(r.value == 0);
    CHECK_FALSE(manager.canUndo());
    CHECK(r.log == std::vector<std::string>{"do A", "do B", "undo B", "undo A"});
    manager.redo();
    CHECK(r.value == 3);
}

TEST_CASE("UndoManager: names and availability follow the stacks", "[M25][REQ-23-2]") {
    Recorder r;
    UndoManager manager;
    CHECK_FALSE(manager.canUndo());
    CHECK_FALSE(manager.canRedo());
    CHECK(manager.getUndoName().empty());
    CHECK(manager.getRedoName().empty());
    manager.execute(add(r, "A", 1));
    manager.execute(add(r, "B", 1));
    CHECK(manager.getUndoName() == "B");
    manager.undo();
    CHECK(manager.getUndoName() == "A");
    CHECK(manager.getRedoName() == "B");
    manager.clear();
    CHECK_FALSE(manager.canUndo());
    CHECK_FALSE(manager.canRedo());
}

TEST_CASE("UndoManager: depth is bounded and the oldest command is dropped", "[M25][REQ-23-3]") {
    Recorder r;
    UndoManager manager;
    for (std::size_t i = 0; i <= constants::defaultUndoDepth; ++i) {
        manager.execute(add(r, std::to_string(i), 1));
    }
    std::size_t undone = 0;
    while (manager.canUndo()) {
        manager.undo();
        ++undone;
    }
    CHECK(undone == constants::defaultUndoDepth);
    CHECK(r.value == 1);  // command 0 could not be undone

    UndoManager small(2);
    small.execute(add(r, "x", 1));
    small.execute(add(r, "y", 1));
    small.execute(add(r, "z", 1));
    small.undo();
    small.undo();
    CHECK_FALSE(small.canUndo());
}
