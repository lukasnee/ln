
// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

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

    /**
     * @brief Check if initialized.
     *
     * @return true if initialized.
     * @return false if not initialized.
     */
    bool is_initialized() const { return this->initialized; }

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

private:
    bool initialized = false;
};

} // namespace ln::drivers
