#pragma once


/// # Rask persistent data model
/**
 * Convenience header pulling in every Rask persistent-data struct — the
 * structs that replace the retired implementation's on-disk JSON blobs
 * (see `BOOKKEEPING.md`) and describe state that arises during execution.
 * Mirrors the structure of `planet/serialise.hpp`. These reference the
 * `rask/wire` vocabulary and the `rask/config` layer underneath it; the
 * peers and subscriptions that describe local-machine setup now live under
 * `rask/config`, not here.
 */

#include <rask/data/filetype.hpp>
#include <rask/data/inode.hpp>
#include <rask/data/name_hash.hpp>
#include <rask/data/server.hpp>
#include <rask/data/tenants.hpp>
