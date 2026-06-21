# Rask — Design Specification

> Status: design spec for a **fresh implementation**. This document consolidates
> the original `README.md` and `DESIGN.md` with what was actually learned from
> the first (now-retired) implementation. It is deliberately **library- and
> language-agnostic**: it describes *what* Rask does and the contracts it must
> honour, not the specific libraries used to build it. Where the first
> implementation made a choice that is worth keeping — or worth avoiding — it is
> called out explicitly.

Rask is a multi-way, peer-to-peer, low-latency file replication system. ("Rask"
is Norwegian for "fast".) Every node holds a full copy of the replicated files;
changes made on any node propagate to all the others with latency dominated by
the network and disk, not by Rask itself.


## 1. Motivating use case

A pool of web application servers serves a site with a file-upload feature. A
file uploaded to one server must be servable, almost immediately, by any other
server — because the user's next request may be routed to a different node.

Existing options were rejected:

- **Networked filesystem (NFS, …)** — no good redundancy story. If the share
  host goes down, every application server loses the files.
- **`rsync` on a cron job** — secure and efficient for the transfer itself, but
  the polling latency is far too high to serve a just-uploaded file.

Rask targets the gap between these: redundant like replication, low-latency like
a push.


## 2. Scope (MVP)

The first release deliberately implements the minimum needed to cover the
motivating use case.

### In scope

- **Eventual superset convergence.** An initial *sweep* of each watched
  directory brings every node to a state that is the *superset* of the files
  across all nodes. (Deletes are the exception — see below.)
- **Low-latency change capture.** Use the OS filesystem-change notification
  mechanism (Linux `inotify`) on watched directories so changes are observed and
  propagated with minimal delay, rather than by polling.
- **Delete propagation only via live events.** A delete propagates only when a
  node observes the delete event live. A delete is *not* reconstructed during a
  sweep — this is what makes the steady state a superset.
- **Binary peer-to-peer protocol** supporting prioritised transmission of data
  and instructions between nodes.
- **Persistent local metadata** describing files and directories, so that
  resynchronisation after a network partition is cheap (compare hashes, transfer
  only differences) rather than re-reading every file.
- **Library-first.** The core is a library; the runnable server is a thin shell
  over it, so other applications can embed Rask.
- **Terminal-log observability.** Synchronisation activity and performance
  counters are surfaced by logging to the terminal. No embedded web server or
  HTTP/JSON monitoring API in the MVP (deferred — see below).

### Scale targets

- Up to ~10,000 watched directories.
- Up to ~1,000,000 files per watched directory.
- Up to ~10,000,000 files overall.
- Rask's own CPU load is near zero when idle; network, bandwidth, and disk are
  the dominant factors when active.

### Explicitly out of scope for the MVP

- Runtime reconfiguration. Configuration is read at startup; changes require a
  restart.
- Synchronising file attributes (permissions, ownership, timestamps as data).
- Efficient delta transfer of changes *within* a file. The MVP re-sends changed
  file content at block granularity, not byte-diff granularity.
- Memory- or disk-usage optimisation of Rask's own metadata.
- Transport security. Connections between nodes are **neither authenticated nor
  encrypted**. Deployments crossing untrusted networks must tunnel Rask over a
  VPN so all nodes can assume safe transit.
- An embedded web server and HTTP/JSON monitoring API. The MVP relies on
  terminal logs; remote/programmatic monitoring is a post-MVP feature.
- Large-file support on 32-bit builds.

All of the above are candidate features once the MVP ships; none should be
*precluded* by MVP design decisions.


## 3. Architecture overview

A Rask deployment is a set of **symmetric peer nodes**. There is no master.
After the initial handshake the protocol is fully symmetric: either side may
initiate a transfer, and either side independently decides what the other needs
based on its model of the other's state.

Each node is built from these cooperating concerns. The first implementation
realised them as four asynchronous worker pools plus a filesystem-watch thread;
a new implementation may structure them differently, but the *separation of
concerns* should be preserved because it keeps latency-sensitive work off the
back of slow work.

| Concern | Responsibility | Latency profile |
| --- | --- | --- |
| **Network I/O** | Accept connections, read/write framed packets, heartbeats. | Must stay responsive; never blocked by disk or hashing. |
| **Response logic** | Decide what to send a peer in reaction to a received packet. | Short, CPU-light. |
| **File management** | Apply changes to the local filesystem; create/truncate/move/write. | Disk-bound. |
| **Hashing** | (Re)hash files and metadata-tree nodes; longer-running. | CPU- and disk-bound; lowest priority. |
| **Filesystem watch** | Translate `inotify` events into Rask change operations. | Event-driven. |

Two persistent stores back a node (see §5):

- a small **server/metadata store** (identity, clock, tenant list, subscriptions);
- per-tenant **metadata trees** describing every inode and its hash.


## 4. Data model

### 4.1 Three levels

Rask reasons about three nested levels, each with its own hash so that nodes can
agree on state and quickly localise differences:

1. **Tenants** — the top-level watched directories (the units of subscription).
2. **Inodes** — the file-system nodes (files and directories) within a tenant.
3. **File data** — the contents of a file, hashed block by block.

Each level rolls up into a single hash; the node as a whole has one top-level
hash that summarises everything it holds. Two nodes whose top-level hashes match
are in sync and need no conversation. When they differ, the nodes descend the
trees comparing sub-hashes to localise exactly which inodes/blocks differ, then
exchange only those.

### 4.2 The name tree (tenants and inodes)

Tenants and inodes are keyed by **name**, so their tree distributes entries by a
hash of the name rather than by file content:

- A node's position in the tree is determined by the **base-32 encoding of the
  SHA-256 of its name**. Each base-32 character selects one of 32 children at one
  level of the tree (5 bits per level). This gives an even distribution and
  bounds tree depth.
- A **leaf** holds the actual entries. When a leaf exceeds a **split
  threshold**, it is replaced by a parent whose 32 children partition the entries
  by the next hash character. (The retired build split at 96 entries.)
- When entries are removed and the total across a parent's children falls below a
  **coalesce threshold**, the children collapse back into a single leaf.
- The split and coalesce thresholds must be far enough apart to avoid thrashing
  as files are rapidly added and removed.

> **Gap carried over from the retired build.** Coalescing was **never
> implemented** — the old tree only ever split. Worse, removals were recorded as
> `move-out` *tombstones* rather than deletions, so a leaf's entry count never
> actually dropped and a coalesce threshold could not have fired even if it
> existed. A long-lived tenant's tree therefore only grows. The rebuild must
> decide deliberately: implement real coalescing (which requires reclaiming
> tombstones), or accept monotonic growth and document why. This interacts with
> delete semantics (§2) and move handling (Appendix A).

### 4.3 The file-data hash tree

A file's content is hashed bottom-up into a single file hash:

- The file is split into fixed-size **data blocks**. Block size is **32 KiB**.
- Each block is hashed (SHA-256) to produce a leaf hash.
- Leaf hashes are grouped and re-hashed level by level. In the retired build the
  grouping was **1024-wide**, because a level's hashes are packed (32 bytes each)
  and re-blocked at the same 32 KiB block size — so one 32 KiB block spans
  1024 hashes. This repeats until a level reduces to a single hash.
- When hashing finishes, if the outermost level holds more than one hash, those
  are hashed once more to yield a single **file hash**. A file of one block or
  less therefore has a file hash equal to that single block's hash.

> **Decision for the new build.** The file-data tree (1024-wide) and the name
> tree (§4.2, 32-wide) genuinely use **different fan-outs** — this is not a doc
> slip, it falls out of re-blocking 32-byte hashes at the 32 KiB block size. Two
> options: (a) **keep them different** — the file tree's wide fan-out keeps it
> shallow and the extra internal hashes are negligible (§11); or (b) **unify on
> 32** so the file-data tree, name tree, and wire encoding share one radix at the
> cost of a deeper file tree. Either way, fix the block size and fan-out(s) as
> named constants. Note the choice barely affects storage overhead: per-block
> leaf hashes dominate and cost ~0.1% regardless (§11).

### 4.4 Hashes

- Algorithm: **SHA-256** at all three levels. It is fast enough, has acceptable
  collision resistance for content verification, and 256 bits fits the protocol's
  fixed-width hash fields.
- Hashes are rendered in **base-32** when used as tree addresses, so each
  character maps to exactly one 5-bit tree level.

### 4.5 Logical clock (the "tick")

Every change carries a **priority** that is a Lamport-style logical clock value,
used both to order events and to resolve conflicts (last-writer-by-tick wins).

A tick is the triple:

- **time** — 64-bit monotonic logical counter (the Lamport component);
- **server identity** — 32-bit node id, used as a deterministic tie-breaker when
  two ticks share the same time;
- **reserved** — 32 bits, currently zero, reserved so the tick occupies a clean
  16 bytes in memory. (The retired encoding sent only time + server — 96 bits —
  on the wire; how the tick is serialised is now the library's concern, §6.5.)

Ordering is lexicographic: compare `time`, then `server`, then `reserved`.

Clock rules:

- **`next()`** — atomically increment and persist the stored `time`, returning a
  new tick stamped with this node's identity. The stored time is persisted
  *without* the identity so the node's identity can change without disturbing
  future ordering.
- **`overheard(t)`** — on receiving a peer's tick, advance the local stored time
  to it if it is greater than the local time (standard Lamport update).
- A node must have a persistent server store to mint ticks; minting a tick
  without one is an error.

### 4.6 Server identity

On first start a node picks a random **32-bit identity** (from a good entropy
source) and persists it. It may be overridden manually in the node's
configuration/database. Identity is the tie-breaker in tick comparison and
labels the origin of changes.


## 5. Metadata storage

Each node persists:

- **Server store** — identity, current clock time, and the node's top-level
  hash.
- **Tenant list** — the known tenants and their tenant-level hashes.
- **Subscription list** — which tenants this node holds locally and where on
  local disk.
- **Per-tenant inode trees** — the name tree of §4.2, storing for each inode its
  metadata (type, size, modified time, content hash, and the priority tick of the
  last change).

The first implementation stored all of this as partitioned JSON document stores
("beanbags"), one database file per tree node, ideally on a separate spindle
from the replicated data.

> **Lesson from the retired implementation.** The first build layered a
> hand-written in-memory block tree (`block` / `leaf_block` / `mid_block` /
> `root_block`) *on top of* the document store, and a source comment candidly
> doubted it: *"I'm starting to think that this is not a smart way of doing this
> at all. The [store] already takes care of these things."* For the new build,
> **pick one tree mechanism** — either lean entirely on the storage layer's own
> partitioning/iteration, or own the tree explicitly — but do not maintain two
> parallel tree structures that must be kept in step. The storage choice itself
> is open; the requirements it must meet are: durable, transactional per-node
> updates; cheap point lookup by name hash; ordered iteration for sweeps and
> hash rollups; and partitioning so no single store node grows unbounded.

A node need not be a *subscriber* to every tenant it *knows about*: it tracks
tenant existence and hashes for all tenants, but only stores file content for
tenants it subscribes to locally.


## 6. Wire protocol

The protocol is **stateful and symmetric**. Each node maintains a view of each
peer's state and sends what it judges the peer to be missing.

**Serialisation is delegated** to the implementation's serialisation library
(§6.5). This section defines the protocol's *messages and their fields
abstractly* — not a byte layout. The retired build's hand-rolled binary encoding
is retained only as non-normative reference (§6.6).

Field types used below:

- **tick** — a logical-clock priority value (time + server id); see §4.5.
- **name** — a UTF-8 string: a tenant name, or a tenant-root-relative path.
- **hash** — a 256-bit SHA-256 value.
- **data block** — up to one block (32 KiB) of raw file bytes.

### 6.1 Messages

**Hash / metadata exchange:**

| Message | Fields | Purpose |
| --- | --- | --- |
| **Version / heartbeat** | version number; optionally { server identity, current tick, top-level hash } | Handshake and keep-alive (§6.2). |
| **Tenant** | tenant name; tenant hash | Announce a tenant and its rolled-up hash. |
| **Tenant hash** | tenant name; name-hash prefix (identifies the layer); up to 32 × { child digit 0–31, child hash } | One layer of sub-hashes within a tenant's name tree. |
| **File-hash request** | tenant name; file name | "Send me this file's data." (No priority.) |
| **File-hash with priority** | tick; tenant name; file name | *(Reserved — defined in the original design, never built.)* |

**Change events** (a node commanding peers to follow an observed change):

| Message | Fields | Purpose |
| --- | --- | --- |
| **File exists** | tick; tenant name; file name; optional file size; optional content hash | Assert a file exists (and, if known, its size/content hash). |
| **Create directory** | tick; tenant name; directory name | Assert a directory exists. |
| **Truncate file** | — | *(Reserved; never built.)* |
| **Move inode out** | tick; tenant name; inode name | Remove/move an inode away (delete / move-source signal). |
| **Move inode in** | — | *(Reserved; never built.)* |
| **Data** | tick; tenant name; file name; byte offset; block content hash; data block | A block of file content destined for a file. |

> **Reserved messages.** "File-hash with priority", "Truncate", and "Move inode
> in" were defined in the original design but never implemented; the retired build
> handled only the others. The protocol redesign should decide deliberately
> whether to keep them (e.g. truncate / move handling) or drop them.

### 6.2 Handshake, heartbeat & convergence

- On connect, each side sends a **version packet** (`0x80`). The effective
  protocol version is the **minimum of the two advertised versions** (the highest
  both understand).
- The version packet also carries (optionally) the sender's identity, current
  tick, and **top-level hash**. On receipt a node calls `overheard()` on the tick
  and compares the peer's top-level hash to its own:
  - **Equal** → already in sync; no conversation needed.
  - **Different** → begin a sync conversation by sending the tenant set, after
    which the nodes descend the trees (§6.3).
- The version packet doubles as the **heartbeat**: if nothing has been sent for
  the heartbeat interval (the retired build used **5 seconds**), a node re-sends
  its version packet. Receiving a packet resets the heartbeat timer; receiving a
  *version* packet resets only the watchdog, not the outbound heartbeat, to avoid
  ping-pong.
- A reconnect watchdog re-establishes outbound connections that drop.

### 6.3 Synchronisation conversation

When two nodes' top-level hashes differ they localise and repair the difference
by descending the hash trees:

1. Exchange **tenant** packets (`0x81`); compare tenant hashes.
2. For a tenant whose hashes differ, exchange **tenant-hash** layers (`0x82`),
   descending the name tree one 5-bit layer at a time, comparing the up-to-32
   child hashes at each layer until the differing inodes are isolated.
3. For a differing file, exchange **file-hash** packets (`0x83`) and, where the
   content differs, the file-data hash tree, then transfer only the **data
   blocks** (`0x9f`) whose hashes do not match.

Because the hashes summarise state, a node that has just reconnected after a
partition compares hashes top-down and transfers only what actually differs,
rather than re-reading and re-sending everything.

### 6.4 Delivery, queuing, and back-pressure

- Outbound packets are queued per connection and written by a dedicated sender so
  multiple producers can enqueue concurrently.
- The queue is **bounded** (the retired build used a 256-slot ring). When full,
  newly offered packets are **spilled (dropped)**, not blocked on.
- Spilling is *safe by design*: the protocol is convergent. Any state lost to a
  spill is rediscovered by the next hash comparison (heartbeat or sweep) and
  re-sent. This is what lets the sender stay non-blocking and low-latency.
- Sends, queue depth, spills, and receives should be exposed as counters for
  monitoring (§9).

### 6.5 Serialisation

Serialisation and framing are **delegated to the implementation's serialisation
library** — the protocol is defined by the messages and fields of §6.1, not by a
byte layout. The library must be able to:

- discriminate the message types of §6.1;
- carry the field types (logical-clock tick, UTF-8 names, fixed 256-bit hashes,
  and binary data blocks up to the block size);
- frame messages over a stream connection.

Endianness, length-prefixing, and integer encoding are the library's concern.
Nothing in convergence depends on the wire encoding: content hashes are taken over
file/inode data (§4), not over serialised messages, and event ordering depends
only on the tick — so the encoding can change freely without touching the data
model. Both peers must, of course, share whatever encoding is chosen.

The retired build instead hand-rolled the binary encoding documented in §6.6.
Verifying that scheme surfaced exactly the accidental complexity that motivates
using a mature serialisation library instead: a size-prefix table whose ranges
overlapped themselves, an encoder that emitted lengths its own decoder rejected,
and a silent 64 KiB cap.

### 6.6 Retired wire encoding (non-normative)

Recorded for reference only — **not** part of the design for the rebuild, which
replaces it per §6.5. Kept because some of its constants leak into other sections
(e.g. the 64 KiB connection buffer).

The retired build framed every packet as `[ size-sequence ][ control byte ][
payload ]`, all multi-byte integers **big-endian**, the control byte excluded from
the length. Control bytes mapped to the §6.1 messages as: `0x80` version, `0x81`
tenant, `0x82` tenant-hash, `0x83` file-hash request, `0x84` file-hash with
priority *(unbuilt)*, `0x90` file-exists, `0x91` create-directory, `0x92` truncate
*(unbuilt)*, `0x93` move-out, `0x94` move-in *(unbuilt)*, `0x9f` data.

The **size-sequence** was a variable-length length prefix keyed on its leading
byte:

| Leading byte | Meaning |
| --- | --- |
| `0x00`–`0x7f` | A length of exactly that many bytes. |
| `0x80`–`0xbf` | Command-ID space (so a command byte never starts a length). |
| `0xc0`–`0xf8` | Reserved for fixed-size block markers — never implemented; the decoder threw. |
| `0xf9`–`0xff` | The following `(byte − 0xf8)` bytes hold the length, big-endian. |

(The original design text gave these ranges as `0x80`–`0xef` *and* `0xc0`–`0xf8`,
which overlap — one of the encoding's several rough edges.) In practice only
`0xf9`/`0xfa` (≤ 64 KiB) were ever emitted or accepted; larger leads threw.
Examples: a 200-byte string → `0xf9 0xC8`; a 1024-byte block → `0xfa 0x04 0x00`.

**Field encodings:** integers fixed-width big-endian; tick = 64-bit time then
32-bit server (96 bits; the reserved field was not sent); string = size-sequence
prefix + UTF-8 bytes; hash = 32 raw bytes, no prefix; data block = size-sequence
prefix + bytes.


## 7. Change application and conflict resolution

A single **change pipeline** applies every mutation, whether it originates from a
local `inotify` event, a sweep, or a peer packet. A change is configured with:

- a **target inode type** (file / directory / moved-out);
- a **priority tick** to compare against the stored inode's recorded priority;
- optional extra **predicates** (OR-combined with the priority check);
- how to compute the new inode's **hash**;
- what to **persist** on update vs. no-update;
- a **broadcast** packet to emit to peers when (and only when) an update is
  actually recorded;
- **post-update / post-otherwise / post-commit** hooks.

Resolution rule: an incoming change is applied **iff** its priority tick is newer
than the stored inode's priority (or a predicate forces it, or the target inode
type does not match what is stored). This makes replication **last-writer-wins,
ordered by logical clock**, with the server-identity tie-breaker giving a total,
deterministic order across nodes — so all nodes converge on the same result
regardless of message arrival order. A change is only broadcast onward when it
genuinely advanced local state, which prevents update storms.


## 8. Sweeps and bootstrap

A **sweep** walks a tenant's directory tree and reconciles the on-disk reality
with the metadata tree. Sweeps run:

- when a new top-level (tenant) directory is first published, and
- when the node starts up.

Startup sequence for a node:

1. Load configuration (one or more config documents).
2. Open/initialise the server store; establish identity (minting one on first
   run) and load the clock.
3. Start listening for inbound peer connections.
4. Load tenants and subscriptions; begin `inotify` watches and start sweeping
   subscribed tenants.
5. Initiate outbound connections to configured peers.

Sweep and live `inotify` events feed the same change pipeline (§7), so a file
discovered by a sweep and a file seen via an event are handled identically. A
freshly-swept node converges to the superset of all peers' files; deletes only
take effect through live move-out/delete events, never through a sweep.


## 9. Monitoring

For the MVP, observability is **terminal logging only** — no embedded web server
or HTTP/JSON API (that is deferred; see §2).

- The node logs synchronisation activity to the terminal at selectable verbosity
  (e.g. errors/warnings/info/debug), so an operator can watch connections,
  handshakes, sweeps, and change application as they happen.
- Performance counters (connections opened, packets queued/sent/spilled/
  received/processed, version packets, …) are sampled periodically and emitted to
  the log rather than exposed over the network.
- A node may also support an **exit-on-sync-success** mode (terminate once a
  version exchange shows it is in sync with at least one peer) for performance and
  integration testing.


## 10. Configuration

- Configuration is supplied as one or more documents merged at startup; **there
  is no runtime reconfiguration** in the MVP.
- Settings cover: the server store location and listen socket (bind address +
  port); the tenant, subscription, and peer store locations; logging (verbosity
  and sinks); and operational toggles (terminate-on-exception,
  exit-on-sync-success).
- The runnable server raises the open-file-descriptor limit at startup
  (the retired build targeted ~20 480) because every watched file and every
  connection consumes descriptors at the target scale.


## 11. Capacity analysis (sizing the trees)

Using a 32 KiB data block. The **name tree** (tenants/inodes) fans out 32-way;
the **file-data tree** fan-out is the open question of §4.3 — but, as shown below,
it barely affects total overhead because per-block leaf hashes dominate.

> The numbers below were re-derived for this spec; the original design's
> large-file figures contained a ~10× arithmetic slip (overhead stated as 0.01%
> where it is really ~0.1%). The corrected version is given here.

- **Files (name-tree framing).** Reading a "hash node" as one tree node carrying
  ≤32 child hashes: a file up to 32 blocks (1 MiB) is one node. A 32 MiB file
  (1024 blocks) is ~33 nodes (1 top + 32 mid over 1024 leaves). One byte beyond
  32 MiB adds a level (~36 nodes). What matters for depth is the number of files
  in an entire sub-tree, not at any one level.
- **Directories/inodes.** A directory of up to 32 entries needs one hash layer;
  growth adds a layer roughly every time the entry count crosses a power-of-32
  boundary. (The retired build defers splitting until ~96 entries, then partitions
  32-way — a storage optimisation that does not change these orders of magnitude.)
- **Realistic MVP target.** ~10,000,000 files in one tenant needs 6 base-32 levels
  (30 bits of addressing; 10⁷ is ~23 binary bits, comfortably inside). That is
  **~10,322,583 hashes** in total (**~315 MiB**): 10,000,000 leaf + 312,500 +
  9,766 + 306 + 10 + 1.
- **Overhead — small files.** With 10M × 4 KiB files (~38 GiB of data), each file
  is one block so no extra data-block hashes are needed; overhead is the
  ~315 MiB of inode/name hashes ≈ **0.8%**.
- **Overhead — large files.** With 10M × 1.5 MiB files (~13.6 TiB of data), each
  file adds ~48 data-block leaf hashes (plus ~2 internal), i.e. ~50 hashes/file →
  ~**500,000,000** extra hashes ≈ **15 GiB** of hash data, for an overhead of
  ~**0.1%**.
- **The floor.** A 32-byte hash per 32 KiB block is **1/1024 ≈ 0.098%** of the
  data it covers, so any file large enough to need block hashing carries at least
  ~0.1% hash overhead regardless of tree fan-out. (A wider file-data fan-out only
  shrinks the negligible *internal*-node count; it cannot beat this leaf floor.)
  This is why the original "0.01%" cannot hold.

Real-world overhead is somewhat higher again because file names and instructions
also travel, but stays well under 1% in both regimes. The theoretical address
space (64-bit counts at each level) is astronomically larger than any real
machine's capacity; these numbers exist only to confirm the tree shape and
constants are comfortable for the MVP targets.


## 12. Requirements & environment

- Linux (the change-notification mechanism assumed is `inotify`).
- A node needs writable storage for its metadata stores, ideally on a separate
  device from the replicated data.
- Endianness is fixed to big-endian on the wire regardless of host architecture.
- 64-bit builds for large-file support; 32-bit builds do not support large files.


## Appendix A — Carried-over decisions and open questions

Decisions inherited from the original design that the new build should *keep*:

- Symmetric, stateful, hash-driven convergence protocol.
- Lamport tick (time + identity tie-break) as both event order and conflict
  resolver; last-writer-by-tick wins.
- 32 KiB blocks, SHA-256, base-32 tree addressing (name tree 32-way; file-data
  tree 1024-way as built — see §4.3 for whether to unify).
- Superset convergence with delete-by-live-event-only.
- Bounded, spill-on-full send queues relying on resync for safety.
- Separation of network / response / file / hash / watch concerns.

Open questions to resolve before/while building:

1. **Storage layer.** What backs the metadata trees, and does it own the tree
   partitioning so we can delete the redundant hand-rolled block tree? (§5)
2. **Serialisation library.** Which library provides the wire format, and does
   its framing handle payloads at least as large as the block size? (The retired
   hand-rolled framing capped at 64 KiB.) (§6.5)
3. **Reserved messages.** Keep or drop "file-hash with priority", "truncate", and
   "move-in" — defined in the original design, never built. (§6.1)
4. **Fan-out.** Keep the file-data tree at 1024-way (as built) or unify on
   32-way with the name tree? Overhead is unaffected either way. (§4.3)
5. **Coalescing & tombstones.** Implement real leaf coalescing (never built), or
   accept monotonic tree growth? Requires deciding how `move-out` tombstones are
   reclaimed. (§4.2)
6. **Move semantics.** Define how move-out/move-in pair up so a rename
   propagates as a move rather than delete-plus-recreate.
7. **Heartbeat / queue constants.** Re-confirm the 5 s heartbeat and 256-slot
   queue against the new I/O model.
