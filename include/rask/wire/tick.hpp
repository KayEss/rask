#pragma once


#include <rask/config/identity.hpp>

#include <planet/serialise/forward.hpp>

#include <compare>
#include <cstdint>
#include <string_view>


namespace rask::wire {


    /// ## Logical clock tick (priority)
    /**
     * A Lamport-style logical clock value carried by every change, used both to
     * order events and to resolve conflicts — last-writer-by-tick wins (see
     * `SPEC.md` §4.5). It is the `priority` field of nearly every wire event
     * packet (file-exists, create-directory, move-out, data, …) and the current
     * tick of the version packet (`SPEC.md` §6.1). Ordering is lexicographic
     * over `time`, then `server`, then `reserved`, which is exactly the member
     * order below, so the defaulted comparison yields the required total order.
     *
     * Unlike the retired on-disk format — which folded the node identity into
     * the stored `time` — `time` here is the bare Lamport counter; the node
     * identity travels only in `server`.
     */
    struct tick {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:tick"};

        /// ### 64-bit monotonic logical counter (the Lamport component)
        std::int64_t time = {};
        /// ### Originating node identity; tie-breaker when `time` is equal
        config::node_id server = {};
        /// ### Reserved, currently zero — pads the tick to a clean 16 bytes
        std::uint32_t reserved = {};

        friend std::strong_ordering
                operator<=>(tick const &, tick const &) = default;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, tick const &);
    void load(planet::serialise::box &, tick &);


}
