#include <rask/data/name_hash.hpp>

#include <planet/serialise.hpp>

#include <span>


/// The digest is saved as a raw byte array — see the note in `hash.cpp`.
void rask::data::save(planet::serialise::save_buffer &ab, name_hash const &h) {
    ab.save_box(name_hash::box, std::span<std::byte const, 32>{h.digest});
}
void rask::data::load(planet::serialise::box &b, name_hash &h) {
    b.named(name_hash::box, h.digest);
}
