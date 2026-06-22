#pragma once


#include <planet/serialise/forward.hpp>

#include <compare>
#include <cstdint>
#include <string_view>


namespace rask::wire {


    /// ## Node identity
    /**
     * A node's 32-bit identity, chosen at random from a good entropy source on
     * first start and then persisted (it may be overridden in configuration).
     * It labels the origin of changes and is the deterministic tie-breaker when
     * two ticks share the same logical time (see `SPEC.md` §4.5 / §4.6). It
     * crosses the wire both on its own — the version packet's "server identity"
     * field — and as a component of every `tick`.
     */
    struct node_id {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:node-id"};

        std::uint32_t value = {};

        friend auto operator<=>(node_id const &, node_id const &) = default;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, node_id const &);
    void load(planet::serialise::box &, node_id &);


}
