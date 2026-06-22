#pragma once


#include <planet/serialise/forward.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>


namespace rask::data {


    /// ## A tenant subscription
    /**
     * Where a subscribed tenant's content lives on local disk. A node can
     * *know* a tenant (see `tenants`) without *subscribing* to it (BOOKKEEPING
     * §3, `SPEC.md` §5). The storage layer enforces a trailing separator on
     * `path`.
     */
    struct subscription {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:subscription"};

        /// ### Local filesystem root for the tenant's content
        std::filesystem::path path;
    };


    /// ## Subscriptions store
    /**
     * The tenants this node holds locally, keyed by tenant name.
     */
    struct subscriptions {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:subscriptions"};

        std::map<std::string, subscription> subscribed;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, subscription const &);
    void load(planet::serialise::box &, subscription &);
    void save(planet::serialise::save_buffer &, subscriptions const &);
    void load(planet::serialise::box &, subscriptions &);


}
