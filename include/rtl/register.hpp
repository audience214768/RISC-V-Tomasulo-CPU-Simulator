#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// ---------------------------------------------------------------------------
// internal: pick smallest unsigned integer that holds Width bits
// ---------------------------------------------------------------------------
namespace rtl_detail {

template <size_t Width>
struct storage_type {
    static_assert(Width <= 64, "Register/Wire width must be <= 64 bits");
    using type =
        std::conditional_t<Width <= 8,  uint8_t,
        std::conditional_t<Width <= 16, uint16_t,
        std::conditional_t<Width <= 32, uint32_t, uint64_t>>>;
};

template <size_t Width>
using storage_t = typename storage_type<Width>::type;

// full-width mask
template <size_t Width>
inline constexpr auto full_mask() -> uint64_t {
    if constexpr (Width == 64) return ~uint64_t{0};
    else                       return (uint64_t{1} << Width) - 1;
}

// mask for SubW bits  (SubW ≤ 64)
template <size_t SubW>
inline constexpr auto sub_mask() -> uint64_t {
    if constexpr (SubW == 64) return ~uint64_t{0};
    else                      return (uint64_t{1} << SubW) - 1;
}

} 
template <size_t Width>
class Register {
    static_assert(Width > 0 && Width <= 64, "Register width must be 1..64");

public:
    using storage_t = rtl_detail::storage_t<Width>;
    static constexpr uint64_t kMask = rtl_detail::full_mask<Width>();

private:
    storage_t cur_{0};
    storage_t next_{0};

public:
    Register() = default;
    explicit Register(storage_t init)
        : cur_(static_cast<storage_t>(init & kMask))
        , next_(static_cast<storage_t>(init & kMask)) {}

    // ---- Q output (read-only) ----------------------------------------
    auto cur() const -> storage_t { return cur_; }

    // ---- default hold (next ← cur, for eval()) -----------------------
    void hold() { next_ = cur_; }

    // ---- full-width read / write -------------------------------------
    auto read() const -> storage_t { return cur_; }
    void write(storage_t val) { next_ = static_cast<storage_t>(val & kMask); }

    // ---- sub-width write with extension (load semantics) -------------
    /// Zero-extend SubW-bit `val` to Width bits and write to next
    template <size_t SubW>
    void write_zx(uint64_t val) {
        static_assert(SubW > 0 && SubW <= Width, "SubW must be in (0, Width]");
        next_ = static_cast<storage_t>((val & rtl_detail::sub_mask<SubW>()) & kMask);
    }

    /// Sign-extend SubW-bit `val` to Width bits and write to next
    template <size_t SubW>
    void write_sx(uint64_t val) {
        static_assert(SubW > 0 && SubW <= Width, "SubW must be in (0, Width]");
        constexpr uint64_t kSubMask = rtl_detail::sub_mask<SubW>();
        constexpr uint64_t kSignBit = uint64_t{1} << (SubW - 1);
        uint64_t v = val & kSubMask;
        if (v & kSignBit) v |= ~kSubMask;
        next_ = static_cast<storage_t>(v & kMask);
    }

    // ---- sub-width read (store semantics) ----------------------------
    /// Return the lower SubW bits, no extension
    template <size_t SubW>
    auto read_sub() const -> storage_t {
        static_assert(SubW > 0 && SubW <= Width, "SubW must be in (0, Width]");
        if constexpr (SubW >= Width) return cur_;
        else return static_cast<storage_t>(cur_ & rtl_detail::sub_mask<SubW>());
    }

    // ---- runtime-width overloads (width in bits: 8 / 16 / 32) -------
    void write_zx(size_t sub_width, uint64_t val) {
        switch (sub_width) {
            case 8:  write_zx<8>(val);  break;
            case 16: write_zx<16>(val); break;
            case 32: write_zx<32>(val); break;
            default: write(val); break;
        }
    }

    void write_sx(size_t sub_width, uint64_t val) {
        switch (sub_width) {
            case 8:  write_sx<8>(val);  break;
            case 16: write_sx<16>(val); break;
            case 32: write_sx<32>(val); break;
            default: write(val); break;
        }
    }

    auto read_sub(size_t sub_width) const -> storage_t {
        switch (sub_width) {
            case 8:  return read_sub<8>();
            case 16: return read_sub<16>();
            case 32: return read_sub<32>();
            default: return read();
        }
    }

    auto next_raw() -> storage_t & { return next_; }

    void tick() { cur_ = next_; }

    void reset(storage_t val) {
        cur_  = static_cast<storage_t>(val & kMask);
        next_ = static_cast<storage_t>(val & kMask);
    }
};
