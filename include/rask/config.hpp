#pragma once


/// # Rask local configuration model
/**
 * Convenience header pulling in the structs that describe how *this machine* is
 * set up — its identity, the peers it dials, and the tenants it subscribes to
 * (see `SPEC.md` §10). This is the foundation layer: it depends on nothing in
 * `rask/wire` or `rask/data`, and both of those reference it instead — the wire
 * vocabulary sits on top of config (e.g. `wire::tick` carries a
 * `config::node_id`), and the persistent-data structs reference both.
 *
 * Config is the *local* counterpart to `rask/wire` (data transferred between
 * peers) and `rask/data` (state that arises during execution): these structs
 * are neither peer traffic nor execution state, they are the operator's setup
 * of the node. They are still persisted through `planet::serialise` like the
 * rest of the model, and although there is no runtime-reconfiguration mechanism
 * yet (`SPEC.md` §2), they are treated as values that may change at runtime
 * rather than as frozen-at-startup constants.
 */

#include <rask/config/identity.hpp>
#include <rask/config/peers.hpp>
#include <rask/config/subscriptions.hpp>
