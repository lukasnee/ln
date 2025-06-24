/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdio> 

namespace fonas {

class File {
public:
    File(FILE *file) : file(file) {
        if (!this->file) {
            FONAS_PANIC();
        }
    }

    File(const char *path, const char *mode) {
        this->file = fopen(path, mode);
        if (!this->file) {
            FONAS_PANIC();
        }
    }

    File(char *mem_data, size_t mem_size, const char *mode) {
        this->file = fmemopen(mem_data, mem_size, mode);
        if (!this->file) {
            FONAS_PANIC();
        }
    }

    operator FILE *() { return this->file; }

    ~File() {
        if (!this->file) {
            return;
        }
        fclose(this->file);
        this->file = nullptr;
    };

private:
    FILE *file = nullptr;
};

} // namespace fonas