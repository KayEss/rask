#pragma once


#include <rask/wire/hash.hpp>

#include <planet/serialise/forward.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>


namespace rask::data {


    /// ## A known tenant
    /**
     * What the node tracks about a tenant it *knows about*, whether or not it
     * subscribes to (holds the content of) that tenant — just the tenant's
     * rolled-up top-level hash. Where the tenant's inode tree is stored is the
     * storage layer's concern, so the retired build's embedded beanbag
     * `database` envelope is deliberately dropped (BOOKKEEPING §2, `SPEC.md`
     * §5).
     */
    struct known_tenant {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:tenant"};

        /// ### The tenant's rolled-up hash; absent until first rollup
        std::optional<wire::hash> tenant_hash;
    };


    /// ## Tenants store
    /**
     * The set of tenants this node knows about, keyed by tenant name. The name
     * lives only in the key; it is not duplicated inside the value.
     */
    struct tenants {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:tenants"};

        std::map<std::string, known_tenant> known;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, known_tenant const &);
    void load(planet::serialise::box &, known_tenant &);
    void save(planet::serialise::save_buffer &, tenants const &);
    void load(planet::serialise::box &, tenants &);


}
