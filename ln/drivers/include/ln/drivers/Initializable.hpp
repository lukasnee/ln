
/*
 * Copyright (C) 2023 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

namespace ln::drivers {

class Initializable {
public:
    /**
     * @brief Initialize.
     *
     * @return true success.
     * @return false failure.
     */
    bool init() {
        if (this->initialized) {
            return true;
        }
        if (!this->ll_init()) {
            return false;
        }
        this->initialized = true;
        return true;
    }

    /**
     * @brief Deinitialize.
     *
     * @return true success.
     * @return false failure.
     */
    bool deinit() {
        if (!this->initialized) {
            return true;
        }
        if (!this->ll_deinit()) {
            return false;
        }
        this->initialized = false;
        return true;
    }

protected:
    /**
     * @brief Low-level initialization.
     *
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_init() = 0;

    /**
     * @brief Low-level deinitialization.
     *
     * @return true success.
     * @return false failure.
     */
    virtual bool ll_deinit() = 0;

    bool initialized = false;
};

} // namespace ln::drivers
