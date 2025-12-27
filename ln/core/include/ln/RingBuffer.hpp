/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <span>
#include <array>
#include <type_traits>
#include <optional>
#include <algorithm>

namespace ln {

/**
 * @brief Ring buffer (circular buffer) interface/manager over externally
 * provided storage.
 */

template <typename T> class RingBuffer {
    static_assert(std::is_trivially_destructible<T>::value,
                  "RingBuffer requires trivially destructible T for embedded safety");

public:
    /**
     * @brief Construct a ring buffer over the given span.
     *
     * The size of the span defines the capacity of the ring buffer.
     */
    explicit RingBuffer(std::span<T> backing_storage) noexcept : storage(backing_storage), head(0), tail(0), count(0) {}

    /**
     * @brief Returns the number of elements stored in the buffer.
     */
    [[nodiscard]] size_t size() const noexcept { return this->count; }

    /**
     * @brief Returns the capacity of the buffer.
     */
    [[nodiscard]] size_t capacity() const noexcept { return this->storage.size(); }

    /**
     * @brief Returns true if no elements are stored.
     */
    [[nodiscard]] bool empty() const noexcept { return this->count == 0; }

    /**
     * @brief Returns true if the buffer cannot accept more elements.
     */
    [[nodiscard]] bool full() const noexcept { return this->count == this->storage.size(); }

    /**
     * @brief Returns how many elements can still be inserted.
     */
    [[nodiscard]] size_t get_free_space() const noexcept { return this->capacity() - this->count; }

    /**
     * @brief Remove all elements from the buffer.
     */
    void clear() noexcept {
        this->head = 0;
        this->tail = 0;
        this->count = 0;
    }

    enum class PushMode {
        normal,   ///< push() fails when full (default behaviour)
        overwrite ///< push() overwrites the oldest element when full
    };

    /**
     * @brief Try to push an element to the back of the buffer.
     *
     * @return false if the buffer is already full.
     */
    bool push(const T &value, PushMode mode = PushMode::normal) noexcept {
        if (this->full()) {
            if (mode == PushMode::normal) {
                return false;
            }
            this->advance_tail(1);
        }
        this->storage[this->head] = value;
        this->advance_head(1);
        return true;
    }

    bool push_overwrite(const T &value) noexcept { return this->push(value, PushMode::overwrite); }

    bool push(const std::span<const T> &values, PushMode mode = PushMode::normal) noexcept {
        size_t to_push = values.size();
        if (to_push == 0) {
            return true;
        }
        size_t free_space = this->get_free_space();
        if (to_push > free_space) {
            if (mode == PushMode::normal) {
                return false;
            }
            size_t overwrite_count = to_push - free_space;
            this->advance_tail(overwrite_count);
            to_push = free_space + overwrite_count;
        }

        size_t first_chunk_size = this->get_contiguous_push_space(mode);
        if (first_chunk_size >= to_push) {
            std::copy_n(values.data(), to_push, &this->storage[this->head]);
            this->advance_head(to_push);
        }
        else {
            std::copy_n(values.data(), first_chunk_size, &this->storage[this->head]);
            size_t second_chunk_size = to_push - first_chunk_size;
            std::copy_n(values.data() + first_chunk_size, second_chunk_size, &this->storage[0]);
            this->advance_head(to_push);
        }
        return true;
    }

    bool push_overwrite(const std::span<const T> &values) noexcept { return this->push(values, PushMode::overwrite); }

    /**
     * @brief Pop the oldest element.
     *
     * @return std::optional containing the value, or std::nullopt if the buffer is empty.
     */
    [[nodiscard]] std::optional<T> pop() noexcept {
        if (this->empty()) {
            return std::nullopt;
        }
        T value = this->storage[this->tail];
        this->advance_tail(1);
        return value;
    }

    /**
     * @brief Access the front element without removing it.
     *
     * @return Pointer to the front element, or nullptr if the buffer is empty.
     */
    [[nodiscard]] const T *front() const noexcept {
        if (this->empty()) {
            return nullptr;
        }
        return &this->storage[this->head - 1];
    }

private:
    size_t get_contiguous_push_space(PushMode mode) const noexcept {
        if (this->full()) {
            if (mode == PushMode::normal) {
                return 0;
            }
            return this->capacity() - this->tail;
        }
        if (this->head >= this->tail) {
            return this->capacity() - this->head;
        }
        if (mode == PushMode::normal) {
            return this->tail - this->head;
        }
        return this->capacity() - this->head;
    }

    void advance_head(size_t step) noexcept {
        this->advance(this->head, step);
        this->count += step;
    }

    void advance_tail(size_t step) noexcept {
        this->advance(this->tail, step);
        this->count -= step;
    }

    void advance(size_t &index, size_t step) noexcept { index = (index + step) % storage.size(); }

    std::span<T> storage;
    size_t head; // index of oldest element
    size_t tail; // index one past newest element
    size_t count;
};

/**
 * @brief Owning fixed-size ring buffer with internal static storage.
 *
 * This is a convenience wrapper that owns a statically-sized array and
 * exposes the same API as RingBuffer by providing its storage span.
 */
template <typename T, size_t N> class RingBufferStatic : public RingBuffer<T> {
    static_assert(N > 0, "RingBufferStatic capacity N must be > 0");

public:
    using Base = RingBuffer<T>;
    RingBufferStatic() noexcept : Base{buffer} {}

private:
    std::array<T, N> buffer;
};

} // namespace ln
