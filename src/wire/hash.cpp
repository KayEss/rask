#include <rask/wire/hash.hpp>

#include <planet/serialise.hpp>

#include <span>


/**
 * The digest is saved as a raw byte array (the `std_byte_array` marker) via an
 * explicit `std::span<std::byte const>`. Saving the `std::array<std::byte, 32>`
 * directly would take Planet's generic `poly_list` path, which the matching
 * array loader does not accept — the span keeps save and load symmetric.
 */
void rask::wire::save(planet::serialise::save_buffer &ab, hash const &h) {
    ab.save_box(hash::box, std::span<std::byte const, 32>{h.digest});
}
void rask::wire::load(planet::serialise::box &b, hash &h) {
    b.named(hash::box, h.digest);
}
