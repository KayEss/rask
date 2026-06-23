#pragma once


#include <rask/config/identity.hpp>
#include <rask/wire/hash.hpp>
#include <rask/wire/tick.hpp>

#include <planet/serialise/forward.hpp>

#include <optional>
#include <string_view>


namespace rask::data {


    /// ## Server store
    /**
     * One document per node: its identity, logical clock, and whole-node
     * top-level hash (BOOKKEEPING §1, `SPEC.md` §5). A node must have a
     * persistent server store before it can mint ticks. The identity is a
     * `config::node_id`: configuration supplies any override, while this store
     * records the active, persisted value (`SPEC.md` §4.6).
     */
    struct server {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:server"};

        /// ### This node's persisted identity
        config::node_id identity = {};

        /// ### Highest logical time seen; absent until the first tick is minted
        std::optional<wire::tick> clock;

        /// ### Top-level hash rolled up from all tenants; absent until first
        /// rollup
        std::optional<wire::hash> top_hash;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, server const &);
    void load(planet::serialise::box &, server &);


}
