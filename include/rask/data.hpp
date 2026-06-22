#pragma once


/// # Rask persistent data model
/**
 * Convenience header pulling in every Rask persistent-data struct — the
 * structs that replace the retired implementation's on-disk JSON blobs
 * (see `BOOKKEEPING.md`). Mirrors the structure of `planet/serialise.hpp`.
 */

#include <rask/data/filetype.hpp>
#include <rask/data/inode.hpp>
#include <rask/data/name_hash.hpp>
#include <rask/data/peers.hpp>
#include <rask/data/server.hpp>
#include <rask/data/subscriptions.hpp>
#include <rask/data/tenants.hpp>
