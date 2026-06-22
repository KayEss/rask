#pragma once


#include <planet/serialise/forward.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <string_view>


namespace rask::wire {


    /// ## Content / inode hash
    /**
     * A 256-bit SHA-256 value — the common currency of Rask's three hash levels
     * (tenant, inode, and file data; see `SPEC.md` §4.4). It crosses the wire
     * whole as a raw 32-byte field in the version, tenant, tenant-hash,
     * file-exists, and data messages (`SPEC.md` §6.1), which is why it lives
     * here alongside the rest of the wire vocabulary. Content and inode hashes
     * are rendered in base64 when displayed; name-tree *addresses* use the
     * base-32 rendering of `rask::data::name_hash` instead.
     */
    struct hash {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:hash"};

        /// ### The raw 32 bytes of the digest
        std::array<std::byte, 32> digest = {};

        /// ### Total ordering over the raw digest
        friend auto operator<=>(hash const &, hash const &) = default;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, hash const &);
    void load(planet::serialise::box &, hash &);


}
