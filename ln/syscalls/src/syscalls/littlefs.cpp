/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/syscalls/littlefs.hpp"

#include <sys/_default_fcntl.h> // for O_* constants
#include <sys/unistd.h>         // for STDIN_FILENO, etc.

#include <array>
#include <cerrno>
#include <cstddef>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

namespace ln::syscalls::littlefs {

template <size_t N> struct FileTable {
    int alloc() {
        for (size_t i = 0; i < N; i++) {
            if (!this->file_entries[i].active) {
                this->file_entries[i].active = true;
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void free(int fd) {
        if (fd < 0 || fd >= static_cast<int>(N)) {
            return;
        }
        this->file_entries[fd].active = false;
    }

    lfs_file_t *get_lfs_file_of(int fd) {
        if (fd < 0 || fd >= static_cast<int>(N)) {
            return nullptr;
        }
        if (!this->file_entries[fd].active) {
            return nullptr;
        }
        return &this->file_entries[fd].file;
    }

private:
    struct FileEntry {
        lfs_file_t file;
        bool active;
    };

    std::array<FileEntry, N> file_entries{};
};

static FileTable<config::max_open_files> file_table;

static lfs_t *curr_lfs = nullptr;

void set_lfs(lfs_t *lfs) { curr_lfs = lfs; }

extern "C" void ln_syscalls_littlefs_set_lfs(lfs_t *lfs) { set_lfs(lfs); }

int open(char *path, int flags) {
    if (!curr_lfs) {
        errno = ENODEV;
        return -1;
    }
    int lfs_flags = 0;
    if (flags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC | O_EXCL)) {
        errno = EINVAL;
        return -1;
    }
    struct FlagMap {
        int newlib_flag;
        int lfs_flag;
    };

    const std::array<FlagMap, 7> flag_map = {{{O_RDONLY, LFS_O_RDONLY},
                                              {O_WRONLY, LFS_O_WRONLY},
                                              {O_RDWR, LFS_O_RDWR},
                                              {O_APPEND, LFS_O_APPEND},
                                              {O_CREAT, LFS_O_CREAT},
                                              {O_TRUNC, LFS_O_TRUNC},
                                              {O_EXCL, LFS_O_EXCL}}};
    for (const auto &fm : flag_map) {
        if (flags & fm.newlib_flag) {
            lfs_flags |= fm.lfs_flag;
        }
    }
    static_assert(O_RDONLY == 0);
    if (lfs_flags == 0) {
        lfs_flags = LFS_O_RDONLY;
    }
    auto fd = file_table.alloc();
    if (fd < 0) {
        errno = ENFILE;
        return -1;
    }
    auto file = file_table.get_lfs_file_of(fd);
    if (!file) {
        file_table.free(fd);
        errno = EBADF;
        return -1;
    }
    int rc = lfs_file_open(curr_lfs, file, path, lfs_flags);
    if (rc < 0) {
        file_table.free(fd);
        errno = -rc;
        return -1;
    }
    return fd;
}

int close(int fd) {
    if (!curr_lfs) {
        errno = ENODEV;
        return -1;
    }
    lfs_file_t *file = file_table.get_lfs_file_of(fd);
    if (!file) {
        errno = EBADF;
        return -1;
    }
    int rc = lfs_file_close(curr_lfs, file);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    file_table.free(fd);
    return 0;
}

int read(int fd, char *ptr, int len) {
    if (!curr_lfs) {
        errno = ENODEV;
        return -1;
    }
    lfs_file_t *file = file_table.get_lfs_file_of(fd);
    if (!file) {
        errno = EBADF;
        return -1;
    }
    int rc = lfs_file_read(curr_lfs, file, ptr, len);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int write(int fd, char *ptr, int len) {
    if (fd == STDIN_FILENO) {
        errno = EBADF;
        return -1;
    }
    if (!curr_lfs) {
        errno = ENODEV;
        return -1;
    }
    lfs_file_t *file = file_table.get_lfs_file_of(fd);
    if (!file) {
        errno = EBADF;
        return -1;
    }
    int rc = lfs_file_write(curr_lfs, file, ptr, len);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int lseek(int fd, int ptr, int dir) {
    if (!curr_lfs) {
        errno = ENODEV;
        return -1;
    }
    lfs_file_t *file = file_table.get_lfs_file_of(fd);
    if (!file) {
        errno = EBADF;
        return -1;
    }
    int rc = lfs_file_seek(curr_lfs, file, ptr, dir);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

} // namespace ln::syscalls::littlefs
