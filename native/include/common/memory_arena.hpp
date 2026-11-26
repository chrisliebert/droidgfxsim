// Copyright (C) 2017-2026 Chris Liebert
// Cache-aligned contiguous bump-allocation memory arena with RAII scoping and std::span views

#ifndef _DROIDGFXSIM_MEMORY_ARENA_HPP_
#define _DROIDGFXSIM_MEMORY_ARENA_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <utility>

#include "common/concepts.hpp"

namespace droidgfxsim {

/// @brief Cache-aligned linear memory arena providing O(1) bump allocations and resets.
class MemoryArena {
public:
    static constexpr size_t kDefaultCacheAlignment = 64; // Standard L1/L2 cache line size

    explicit MemoryArena(size_t capacity_bytes = 1024 * 1024)
        : capacity_(capacity_bytes), offset_(0) {
        assert(capacity_bytes > 0 && "MemoryArena capacity must be greater than zero");
        buffer_ = static_cast<std::byte*>(
            ::operator new(capacity_, std::align_val_t{kDefaultCacheAlignment}, std::nothrow));
        assert(buffer_ != nullptr && "Failed to allocate memory for MemoryArena");
    }

    ~MemoryArena() {
        if (buffer_) {
            ::operator delete(buffer_, std::align_val_t{kDefaultCacheAlignment});
            buffer_ = nullptr;
        }
    }

    // Non-copyable, movable
    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;

    MemoryArena(MemoryArena&& other) noexcept
        : buffer_(other.buffer_), capacity_(other.capacity_), offset_(other.offset_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.offset_ = 0;
    }

    MemoryArena& operator=(MemoryArena&& other) noexcept {
        if (this != &other) {
            if (buffer_) {
                ::operator delete(buffer_, std::align_val_t{kDefaultCacheAlignment});
            }
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            offset_ = other.offset_;

            other.buffer_ = nullptr;
            other.capacity_ = 0;
            other.offset_ = 0;
        }
        return *this;
    }

    /// @brief Allocate a raw block of aligned bytes.
    /// @param bytes Number of bytes to allocate.
    /// @param alignment Required alignment in bytes (default: std::max_align_t).
    /// @return Pointer to the allocated block or nullptr if arena capacity is exceeded.
    void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) {
        if (!buffer_ || bytes == 0) {
            return nullptr;
        }

        void* current_ptr = buffer_ + offset_;
        size_t space_remaining = capacity_ - offset_;

        void* aligned_ptr = std::align(alignment, bytes, current_ptr, space_remaining);
        if (!aligned_ptr) {
            return nullptr; // Out of arena space
        }

        // Advance offset to account for alignment padding + allocated bytes
        size_t new_offset = static_cast<std::byte*>(aligned_ptr) - buffer_ + bytes;
        assert(new_offset <= capacity_);
        offset_ = new_offset;
        return aligned_ptr;
    }

    /// @brief Construct an object of type T in arena storage via placement new.
    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* memory = allocate(sizeof(T), alignof(T));
        if (!memory) {
            return nullptr;
        }
        return new (memory) T(std::forward<Args>(args)...);
    }

    /// @brief Allocate an array of trivially constructible/destructible objects returned as std::span.
    template <typename T>
    std::span<T> allocate_span(size_t count) {
        if (count == 0) {
            return std::span<T>{};
        }
        void* memory = allocate(sizeof(T) * count, alignof(T));
        if (!memory) {
            return std::span<T>{};
        }
        return std::span<T>(static_cast<T*>(memory), count);
    }

    /// @brief O(1) full reset of the arena. Note: Does not call individual destructors.
    void reset() noexcept {
        offset_ = 0;
    }

    /// @brief Get the current allocation watermark offset.
    [[nodiscard]] size_t get_marker() const noexcept {
        return offset_;
    }

    /// @brief Rewind the allocation watermark back to a previously saved marker.
    void reset_to_marker(size_t marker) noexcept {
        assert(marker <= offset_ && "Cannot reset to a marker ahead of current offset");
        offset_ = marker;
    }

    /// @brief Returns the number of bytes currently allocated (including alignment padding).
    [[nodiscard]] size_t used_bytes() const noexcept {
        return offset_;
    }

    /// @brief Returns the total byte capacity of this arena.
    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }

    /// @brief Returns the raw base pointer.
    [[nodiscard]] const std::byte* data() const noexcept {
        return buffer_;
    }

private:
    std::byte* buffer_{nullptr};
    size_t capacity_{0};
    size_t offset_{0};
};

// Static verification that MemoryArena satisfies LinearMemoryResource concept
static_assert(LinearMemoryResource<MemoryArena>, "MemoryArena must fulfill LinearMemoryResource concept");

/// @brief RAII scoped watermark restorer for MemoryArena.
class ArenaScope {
private:
    MemoryArena& arena_;
    size_t saved_offset_;
    bool active_{true};

public:
    explicit ArenaScope(MemoryArena& arena) noexcept
        : arena_(arena), saved_offset_(arena.get_marker()), active_(true) {}

    ArenaScope(ArenaScope&& other) noexcept
        : arena_(other.arena_), saved_offset_(other.saved_offset_), active_(other.active_) {
        other.active_ = false;
    }

    ArenaScope(const ArenaScope&) = delete;
    ArenaScope& operator=(const ArenaScope&) = delete;
    ArenaScope& operator=(ArenaScope&&) = delete;

    ~ArenaScope() {
        if (active_) {
            arena_.reset_to_marker(saved_offset_);
        }
    }

    /// @brief Returns the watermark saved at the start of this scope.
    [[nodiscard]] size_t saved_marker() const noexcept {
        return saved_offset_;
    }

    /// @brief Returns whether this RAII scope is actively managing a watermark rewind.
    [[nodiscard]] bool is_active() const noexcept {
        return active_;
    }
};

} // namespace droidgfxsim

#endif // _DROIDGFXSIM_MEMORY_ARENA_HPP_
