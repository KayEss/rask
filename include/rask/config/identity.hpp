#pragma once


#include <planet/serialise/forward.hpp>

#include <compare>
#include <cstdint>
#include <string_view>


namespace rask::config {


    /// ## Node identity
    /**
     * A node's 32-bit identity: local-machine configuration that names *this*
     * node. On first start it is chosen at random from a good entropy source
     * and then persisted; it may be overridden in configuration (see `SPEC.md`
     * §4.6). It labels the origin of changes and is the deterministic
     * tie-breaker when two ticks share the same logical time (`SPEC.md` §4.5).
     *
     * Although it is announced to peers — the version packet's "server
     * identity" field — and is a component of every `wire::tick`, its essential
     * nature is configuration of the local node, so it lives in the config
     * layer that the wire vocabulary sits on top of rather than in `rask/wire`.
     * Like the rest of the config layer it may change at runtime (there is no
     * mechanism for that yet).
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
