#pragma once


#include <planet/serialise/forward.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <string_view>


namespace rask::data {


    /// ## Name-tree address
    /**
     * The base-32 encoding of the SHA-256 of an inode's (or tenant's) name.
     * Each base-32 character selects one of 32 children at one level of the
     * name tree (5 bits per level), so the hash doubles as the entry's address
     * within the tree (see `SPEC.md` §4.2). Although it is also a 256-bit
     * SHA-256, it is a *distinct* type from `hash`: it addresses by **name**,
     * never by **content**, and the two must never be interchanged.
     */
    struct name_hash {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:name-hash"};

        /// ### The raw 32 bytes of the digest
        std::array<std::byte, 32> digest = {};

        /// ### Total ordering over the raw digest
        friend auto operator<=>(name_hash const &, name_hash const &) = default;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, name_hash const &);
    void load(planet::serialise::box &, name_hash &);


}
