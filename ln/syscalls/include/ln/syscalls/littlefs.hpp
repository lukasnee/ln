/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "lfs.h"

namespace ln::syscalls {

/**
 * @brief LittleFS file IO wrappers for newlib syscalls. These can be used to
 * redirect standard C file IO functions (fopen, fread, fwrite, fclose, etc.) to
 * LittleFS. Make sure to `set_lfs()` before using any of these functions.
 */
namespace littlefs {

namespace config {
constexpr size_t max_open_files = 4;
} // namespace config

void set_lfs(lfs_t *lfs);

int open(char *path, int flags);
int close(int fd);
int read(int fd, char *ptr, int len);
int write(int fd, char *ptr, int len);
int lseek(int fd, int ptr, int dir);
} // namespace littlefs
} // namespace ln::syscalls