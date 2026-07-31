#pragma once
#include "register.hpp"
#include <cstring>

template <size_t Width>
class Wire {
    static_assert(Width > 0 && Width <= 64, "Wire width must be 1..64");
public:
    using storage_t = rtl_detail::storage_t<Width>;
    static constexpr uint64_t kMask = rtl_detail::full_mask<Width>();
private:
    mutable storage_t value_{0};
public:
    Wire() = default;
    explicit Wire(storage_t init) : value_(static_cast<storage_t>(init & kMask)) {}
    void write(storage_t val) const { value_ = static_cast<storage_t>(val & kMask); }
    auto read() const -> storage_t { return value_; }
    template <size_t SubW> auto read_sub() const -> storage_t {
        static_assert(SubW > 0 && SubW <= Width);
        if constexpr (SubW >= Width) return value_;
        else return static_cast<storage_t>(value_ & rtl_detail::sub_mask<SubW>());
    }
    auto read_sub(size_t sw) const -> storage_t {
        switch(sw){case 8:return read_sub<8>();case 16:return read_sub<16>();case 32:return read_sub<32>();default:return read();}
    }
};

/// Helper: zero an array of Wires
template <typename T, size_t N>
static inline void wire_clear(T (&arr)[N]) {
    std::memset(arr, 0, sizeof(arr));
}
