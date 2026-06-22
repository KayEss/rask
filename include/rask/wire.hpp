#pragma once


/// # Rask wire protocol model
/**
 * Convenience header pulling in the structs that are part of Rask's binary
 * peer-to-peer protocol — the field types that cross the wire whole (see
 * `SPEC.md` §6). The persistent-data structs that are stored locally live under
 * `rask/data` instead; they reference these wire types as their shared
 * vocabulary, never the reverse.
 */

#include <rask/wire/hash.hpp>
#include <rask/wire/identity.hpp>
#include <rask/wire/tick.hpp>
