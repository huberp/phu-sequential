#pragma once

#include <cstddef>

// REQ-17-1: verifies that a scope (typically one or a few SequencerCore::processBlock calls)
// performs zero heap allocations. Implemented by replacing the global operator new/delete with
// counting versions (tests/support/AllocationGuard.cpp); this is standard, portable C++ and
// works identically on every CI platform (Windows/Linux/macOS, any compiler), unlike relying on
// a platform-specific allocator hook.
namespace phu::test {

class AllocationGuard {
public:
    AllocationGuard() noexcept;
    ~AllocationGuard() noexcept;

    AllocationGuard(const AllocationGuard&) = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;

    // Number of operator new calls observed since construction (0 is the RT requirement).
    [[nodiscard]] std::size_t allocationCount() const noexcept;

private:
    std::size_t startCount_;
};

}  // namespace phu::test
