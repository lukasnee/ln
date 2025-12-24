/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

namespace ln {

template <typename T> class StaticForwardListNode {
public:
    // TODO: try to make `next` private
    StaticForwardListNode<T> *next = nullptr;
};

template <typename T> class StaticForwardList {
public:
    class iterator {
    public:
        iterator(StaticForwardListNode<T> *node) : current(node) {}

        T &operator*() { return static_cast<T &>(*current); }
        T *operator->() { return static_cast<T *>(current); }

        iterator &operator++() {
            if (current) {
                current = current->next;
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator &other) const { return current == other.current; }

        bool operator!=(const iterator &other) const { return !(*this == other); }

    private:
        StaticForwardListNode<T> *current;
    };

    class const_iterator {
    public:
        const_iterator(const StaticForwardListNode<T> *node) : current(node) {}

        const T &operator*() const { return static_cast<const T &>(*current); }
        const T *operator->() const { return static_cast<const T *>(current); }

        const_iterator &operator++() {
            if (current) {
                current = current->next;
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator &other) const { return current == other.current; }

        bool operator!=(const const_iterator &other) const { return !(*this == other); }

    private:
        const StaticForwardListNode<T> *current;
    };

    void push_front(StaticForwardListNode<T> &node) {
        if (head) {
            node.next = head;
        }
        head = &node;
    }

    iterator begin() { return iterator(head); }
    iterator end() { return iterator(nullptr); }

    const_iterator begin() const { return const_iterator(head); }
    const_iterator end() const { return const_iterator(nullptr); }

    const_iterator cbegin() const { return const_iterator(head); }
    const_iterator cend() const { return const_iterator(nullptr); }

private:
    StaticForwardListNode<T> *head = nullptr;
};
} // namespace ln
