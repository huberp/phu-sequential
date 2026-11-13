#include "AllocationGuard.h"

#include <cstdlib>
#include <new>

namespace phu::test::detail {
namespace {
// Per-thread so Catch2's own (parallel-unused here, but future-proof) bookkeeping never
// interferes with another thread's guard. depth_ > 0 means "currently counting".
thread_local std::size_t depth_ = 0;
thread_local std::size_t allocationCount_ = 0;
}  // namespace

bool trackingActive() noexcept { return depth_ > 0; }
void enterTracking() noexcept { ++depth_; }
void exitTracking() noexcept { --depth_; }
std::size_t currentAllocationCount() noexcept { return allocationCount_; }
void recordAllocation() noexcept { ++allocationCount_; }

}  // namespace phu::test::detail

namespace phu::test {

AllocationGuard::AllocationGuard() noexcept : startCount_(detail::currentAllocationCount()) {
    detail::enterTracking();
}

AllocationGuard::~AllocationGuard() noexcept {
    detail::exitTracking();
}

std::size_t AllocationGuard::allocationCount() const noexcept {
    return detail::currentAllocationCount() - startCount_;
}

}  // namespace phu::test

// Global operator new/delete overrides: route through std::malloc/free (matching the default
// implementation) and count calls while a phu::test::AllocationGuard is active. This affects the
// whole test binary but is inert (zero overhead beyond the depth_ check) outside guarded scopes.
void* operator new(std::size_t size) {
    if (phu::test::detail::trackingActive()) {
        phu::test::detail::recordAllocation();
    }
    if (void* ptr = std::malloc(size == 0 ? 1 : size)) {
        return ptr;
    }
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }
