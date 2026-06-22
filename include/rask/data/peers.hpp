#pragma once


#include <planet/serialise/forward.hpp>

#include <string>
#include <string_view>
#include <vector>


namespace rask::data {


    /// ## An outbound peer
    /**
     * A peer this node should dial and keep connected; the reconnect watchdog
     * redials these (inbound peers are not recorded). BOOKKEEPING §4 keyed
     * these by a name that the old code ignored, so the rebuild stores them as
     * a plain list. `port` is kept as a string so service names resolve too.
     */
    struct peer {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:peer"};

        std::string host;
        std::string port;
    };


    /// ## Peers store
    struct peers {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:peers"};

        std::vector<peer> connect;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, peer const &);
    void load(planet::serialise::box &, peer &);
    void save(planet::serialise::save_buffer &, peers const &);
    void load(planet::serialise::box &, peers &);


}
