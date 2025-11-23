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
#include <span>

namespace ln {

/**
 * @brief Simple RAII wrapper class around standard C FILE*.
 */
class File {
public:
    /**
     * @brief Construct for standard files: stdin, stdout, stderr.
     */
    File(FILE *file) {
        if (!file) {
            LN_PANIC();
        }
        if (file != stdout && file != stdin && file != stderr) {
            LN_PANIC();
        }
        this->file = file;
    }

    /**
     * @brief Construct for regular files on filesystem.
     */
    File(const char *path, const char *mode) {
        this->file = fopen(path, mode);
        if (!this->file) {
            LN_PANIC();
        }
    }

    /**
     * @brief Construct for memory files.
     */
    File(std::span<char> mem_data, const char *mode) {
        this->file = fmemopen(mem_data.data(), mem_data.size(), mode);
        if (!this->file) {
            LN_PANIC();
        }
    }

    operator FILE *() { return this->file; }

    ~File() {
        if (!this->file) {
            return;
        }
        if (this->file == stdout || this->file == stdin || this->file == stderr) {
            this->file = nullptr;
            return;
        }
        fclose(this->file);
        this->file = nullptr;
    };

private:
    FILE *file = nullptr;
};

} // namespace ln
