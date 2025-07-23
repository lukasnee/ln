#include <functional> // std::function
#include <array>      // std::array
#include <cstddef>    // size_t
#include <algorithm>  // std::min

namespace fonas {

template <typename T, size_t N> struct Array : public std::array<T, N> {
    using std::array<T, N>::array;

    size_t size_used() const { return this->pos; }
    size_t size_free() const { return this->size() - this->size_used(); }
    bool is_full() const { return this->size_free() == 0; }
    bool is_empty() const { return this->size_used() == 0; }
    void clear() { this->pos = 0; }

    size_t append(const T *data, size_t size) {
        if (!data || size == 0) {
            return 0;
        }
        const size_t element_append_size = std::min(size, this->size_free());
        std::copy_n(data, element_append_size, this->begin() + this->pos);
        this->pos += element_append_size;
        return element_append_size;
    }

    size_t append(const T &data) { return this->append(&data, 1); }

    size_t append(const T *data, size_t size, std::function<bool(const T *data, size_t size)> flush_f) {
        size_t elements_appended_in_total = 0;
        if (!data || size == 0) {
            return 0;
        }
        while (size > 0) {
            const size_t elements_appended = this->append(data, size);
            data += elements_appended;
            size -= elements_appended;
            elements_appended_in_total += elements_appended;
            if (this->size_free() > 0) {
                continue;
            }
            if (flush_f(this->data(), this->size_used())) {
                this->clear();
                continue;
            }
            break;
        }
        return elements_appended_in_total;
    }

    size_t pos = 0;
};

} // namespace fonas