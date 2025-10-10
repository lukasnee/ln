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

template <typename T> class OutStream {
public:
    virtual ~OutStream() = default;
    virtual void put(T e) = 0;
};

template <typename T> class InStream {
public:
    virtual ~InStream() = default;
    virtual T get() = 0;
};

template <typename T> class Stream : public OutStream<T>, public InStream<T> {
public:
    virtual ~Stream() = default;
};

} // namespace ln
