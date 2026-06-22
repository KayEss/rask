#pragma once


#include <rask/data/filetype.hpp>
#include <rask/data/name_hash.hpp>
#include <rask/wire/hash.hpp>
#include <rask/wire/tick.hpp>

#include <planet/serialise/forward.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>


namespace rask::data {


    /// ## File stat
    /**
     * The cached size and modification time of a file, used to skip re-hashing
     * an inode whose on-disk stat is unchanged (BOOKKEEPING §5a).
     */
    struct file_stat {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:file-stat"};

        /// ### File size in bytes
        std::uint64_t size = {};
        /// ### Last-modified timestamp (microsecond precision on disk)
        std::chrono::system_clock::time_point modified = {};

        friend auto operator<=>(file_stat const &, file_stat const &) = default;
    };


    /// ## Transient remote reconciliation state
    /**
     * Set when a peer's *file-exists* assertion arrives before the local
     * content has been reconciled, and cleared once the local hash is
     * established (BOOKKEEPING §5a).
     */
    struct remote_state {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:remote"};

        /// ### The peer's priority for the asserted state
        wire::tick priority = {};
        /// ### The peer's reported size, if known
        std::optional<std::uint64_t> size;
        /// ### The peer's reported content hash, if known
        std::optional<wire::hash> content;
    };


    /// ## Inode
    /**
     * One file or directory within a tenant. The inode's tenant-relative path
     * is the key in the owning `inode_tree` and so is not repeated here; `name`
     * keeps the *hash* of that path because it is the entry's address in the
     * name tree and is costly to recompute (`SPEC.md` §4.2).
     */
    struct inode {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:inode"};

        /// ### Whether this is a directory, file, or move-out tombstone
        filetype type = {};

        /// ### Name-tree address (hash of the tenant-relative path)
        name_hash name = {};

        /// ### Content / inode hash; absent until hashing completes
        std::optional<wire::hash> content;

        /// ### Last-writer-wins ordering key; absent until a tick is assigned
        std::optional<wire::tick> priority;

        /// ### Cached on-disk stat (files only, after local hashing)
        std::optional<file_stat> stat;

        /// ### Transient peer-asserted state, before local reconciliation
        std::optional<remote_state> remote;
    };


    /// ## Per-tenant inode tree
    /**
     * The logical map of every inode in a tenant, keyed by tenant-relative
     * path. Partitioning the tree across storage nodes is the storage layer's
     * concern, so the retired build's in-band cluster scaffolding
     * (`@context` / `layer` / child-hash rollups) is deliberately omitted
     * (`SPEC.md` §5; BOOKKEEPING §5b).
     */
    struct inode_tree {
        /// ### Serialisation box name
        static constexpr std::string_view box = {"r:inode-tree"};

        /// ### The tenant this tree belongs to
        std::string tenant;
        /// ### Every inode, keyed by tenant-relative path
        std::map<std::string, inode> inodes;
    };


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, file_stat const &);
    void load(planet::serialise::box &, file_stat &);
    void save(planet::serialise::save_buffer &, remote_state const &);
    void load(planet::serialise::box &, remote_state &);
    void save(planet::serialise::save_buffer &, inode const &);
    void load(planet::serialise::box &, inode &);
    void save(planet::serialise::save_buffer &, inode_tree const &);
    void load(planet::serialise::box &, inode_tree &);


}
