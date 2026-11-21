#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace phu::sequencing {

// Wait-free single-producer/single-consumer "latest value" hand-off (sec. 17, arch. 3.12).
// The producer fills the back slot and publishes it as the middle slot; the consumer swaps the
// middle slot into the front slot when it is fresh. All operations are [RT].
template <typename T>
class TripleBuffer {
    static_assert(std::is_nothrow_copy_assignable_v<T>, "TripleBuffer requires a nothrow-copy-assignable T");
    static_assert(std::atomic<std::uint8_t>::is_always_lock_free);

public:
    TripleBuffer() = default;
    explicit TripleBuffer(const T& initial) : slots_{initial, initial, initial} {}

    TripleBuffer(const TripleBuffer&) = delete;
    TripleBuffer& operator=(const TripleBuffer&) = delete;

    // Producer side (exactly one thread at a time).
    [[nodiscard]] T& writeBuffer() noexcept { return slots_[backIndex_]; }

    void publish() noexcept {
        const auto previous = middleAndFresh_.exchange(static_cast<std::uint8_t>(backIndex_ | freshBit),
                                                       std::memory_order_acq_rel);
        backIndex_ = static_cast<std::uint8_t>(previous & indexMask);
    }

    void write(const T& value) noexcept {
        writeBuffer() = value;
        publish();
    }

    // Consumer side (exactly one thread). Returns true if a newer value was taken.
    bool update() noexcept {
        if ((middleAndFresh_.load(std::memory_order_relaxed) & freshBit) == 0) {
            return false;
        }
        const auto previous = middleAndFresh_.exchange(frontIndex_, std::memory_order_acq_rel);
        frontIndex_ = static_cast<std::uint8_t>(previous & indexMask);
        return true;
    }

    [[nodiscard]] const T& read() const noexcept { return slots_[frontIndex_]; }

private:
    static constexpr std::uint8_t indexMask = 0x3;
    static constexpr std::uint8_t freshBit = 0x4;

    std::array<T, 3> slots_{};
    std::atomic<std::uint8_t> middleAndFresh_{1};
    std::uint8_t backIndex_ = 0;
    std::uint8_t frontIndex_ = 2;
};

}  // namespace phu::sequencing
