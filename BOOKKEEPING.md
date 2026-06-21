# Rask — Bookkeeping & on-disk JSON formats

> Reference catalogue of the **persistent JSON blobs** the current (now-retired)
> implementation keeps on disk. It documents the existing data model so the
> rebuild (see `SPEC.md`) can either reproduce, migrate, or deliberately depart
> from it. This describes *what the old code actually wrote*, including its
> quirks — it is descriptive, not prescriptive.

All persistent state is stored as JSON document databases ("beanbags"): each is a
single JSON file, updated transactionally and (by default) pretty-printed. There
are five logical stores plus per-file binary hash sidecars.

```
var/lib/rask/
  server.json              # node identity, clock, top-level hash
  tenants.json             # known tenants + their rolled-up hashes
  subscriptions.json       # which tenants this node holds locally
  peers.json               # outbound peer connections to dial
  tenants.<tenant>.json    # per-tenant inode tree (root node)
  tenants.<tenant>/        # per-tenant inode tree (partitioned child nodes)
    <nh>/<nh>-… .json      #   ... named by name-hash prefix
    <nh>/<nh>-<lvl>.hashes #   per-file binary block-hash sidecars (NOT JSON)
```

> **Config vs. data.** The files under `etc/` (e.g. `server.json`, `peer.json`)
> are *configuration* and share the beanbag "database config" envelope
> (`{"name", "filepath", "initial"}`) — `initial` is the document written when the
> data file is first created. The files under `var/lib/rask/` are the *data*
> documents described below. This doc covers the data.


## Conventions

These encodings recur throughout the stores:

| Thing | Encoding | Example |
| --- | --- | --- |
| **Content / inode hash** | base64 of a 32-byte SHA-256 | `"n4bQgYhMfWWaL+qgxVrQFaO/TxsrC4Is0V1sFbDwCgg="` |
| **Name hash** | **base32** of SHA-256 of the name (≈52 chars); each char selects one tree layer | `"pcg…"` |
| **Name-hash path** | name hash with `/` inserted in growing groups (2, 2, 3, 4, … chars) to spread files across directories | `"pc/g4/abc/…"` |
| **Tick (priority / clock)** | 2-element array `[time, server]` — `time` is the int64 Lamport counter, `server` the uint32 node id. The 32-bit reserved field is **not** stored. | `[42, 2748190923]` |
| **filetype** | one of `"directory"`, `"file"`, `"move-out"` | |

> **Quirk to note for the rebuild.** Although a code comment says the clock time
> is "stored without the server identity," the tick→JSON coercion in fact writes
> `[time, server]`, so the persisted `time` field *does* carry the current
> identity. Decide deliberately whether the new format keeps that.


## 1. Server store — `server.json`

One document per node. Holds identity, the logical clock, and the node's
top-level (whole-node) hash.

```json
{
  "identity": 2748190923,
  "time": [42, 2748190923],
  "hash": "n4bQgYhMfWWaL+qgxVrQFaO/TxsrC4Is0V1sFbDwCgg="
}
```

| Key | Type | Notes |
| --- | --- | --- |
| `identity` | uint32 | Picked randomly from `/dev/urandom` on first start; persisted; may be hand-edited. Tie-breaker in tick comparison. |
| `time` | tick `[int64, uint32]` | Highest Lamport time seen. Advanced by `next()` (mint) and `overheard()` (peer's clock). Absent until the first tick is minted. |
| `hash` | base64 SHA-256 | Rolled up from all tenant hashes (see `rehash_tenants`). This is what peers compare in the version handshake. Absent until first rollup. |

(The matching config envelope also carries a `socket` block — `{"bind", "port"}` —
but that is configuration, not stored data.)


## 2. Tenants store — `tenants.json`

The set of tenants this node knows about (whether or not it holds their content),
each with a pointer to its inode-tree database and its rolled-up tenant hash.

```json
{
  "known": {
    "photos": {
      "database": {
        "filepath": "var/lib/rask/tenants.photos.json",
        "name": "tenant/photos",
        "initial": { "tenant": "photos" }
      },
      "hash": { "data": "Q1Wv…=" }
    }
  }
}
```

| Path | Type | Notes |
| --- | --- | --- |
| `known` | object | Keyed by tenant name. |
| `known.<name>.database` | beanbag config | Locates the per-tenant inode tree root file (§5). |
| `known.<name>.hash.data` | base64 SHA-256 | The tenant's top-level hash. Rolled up from the inode tree; fed into the node hash (§1). |


## 3. Subscriptions store — `subscriptions.json`

Which tenants this node actually holds on local disk, and where. A node can *know*
a tenant (§2) without *subscribing* to it.

```json
{
  "subscription": {
    "photos": { "path": "/srv/photos" }
  }
}
```

| Path | Type | Notes |
| --- | --- | --- |
| `subscription` | object | Keyed by tenant name. |
| `subscription.<name>.path` | string | Local filesystem root for the tenant. Only `path` is read; other keys, if present, are passed through as the tenant's configuration. A trailing `/` is enforced internally. |


## 4. Peers store — `peers.json`

Outbound connections to establish and keep alive (the reconnect watchdog redials
these). Inbound peers are not recorded here.

```json
{
  "connect": {
    "node-b": { "host": "10.0.0.2", "port": "20543" }
  }
}
```

| Path | Type | Notes |
| --- | --- | --- |
| `connect` | object | Iterated; the **key is ignored**, only the value is used. |
| `connect.*.host` | string | Hostname or address to resolve. |
| `connect.*.port` | string | Port (string, passed to the resolver). |


## 5. Per-tenant inode tree — `tenants.<tenant>.json` (+ partition children)

The heart of the bookkeeping: one tree per subscribed tenant, mapping every
file/directory to its metadata and hash. The tree is the name tree from
`SPEC.md` §4.2 — a leaf document until it grows too large, then partitioned into
32 child documents by the next character of each entry's name hash.

### 5a. Leaf node (unpartitioned)

```json
{
  "tenant": "photos",
  "inodes": {
    "holiday/beach.jpg": {
      "filetype": "file",
      "name": "holiday/beach.jpg",
      "hash": {
        "name": "pcg4abc…",
        "inode": "n4bQgYhMfWWaL+qgxVrQFaO/TxsrC4Is0V1sFbDwCgg="
      },
      "priority": [42, 2748190923],
      "stat": { "size": { "bytes": 184320 }, "modified": "2026-06-21T14:02:11.000000Z" }
    },
    "holiday": {
      "filetype": "directory",
      "name": "holiday",
      "hash": { "name": "9xk2…", "inode": "47DEQpj8…=" },
      "priority": [7, 2748190923]
    }
  }
}
```

`inodes` is keyed by the **tenant-relative path string**. Each entry:

| Key | Type | Present when | Notes |
| --- | --- | --- | --- |
| `filetype` | string | always | `"directory"` / `"file"` / `"move-out"`. A `move-out` entry is a tombstone for a removed/moved inode. |
| `name` | string | always | Tenant-relative path. Duplicates the map key. |
| `hash.name` | base32 | always | Name hash of `name`; selects the entry's position in the tree. |
| `hash.inode` | base64 | once known | The entry's content hash. For a directory it hashes the filetype; for a file it is the file-content hash. Absent until hashing completes. |
| `priority` | tick | once a clock tick is assigned | Last-writer-wins ordering key. A freshly-swept local file may exist briefly without one. |
| `stat` | object | files, after local hashing | `{ "size": { "bytes": <int64> }, "modified": <timestamp> }`. Used to skip re-hashing when unchanged. |
| `remote` | object | transient | Set when a peer's *file-exists* arrives but local content isn't reconciled yet: `{ "priority": <tick>, "size": <int64\|null>, "hash": <base64\|null> }`. Removed once the local hash is established. |

### 5b. Partitioned node (cluster)

When a leaf's `inodes` exceeds **96** entries it is replaced by a cluster: its
entries are pushed down into up to 32 child databases (one per next name-hash
character), and the node keeps only the children's rolled-up hashes.

```json
{
  "tenant": "photos",
  "@context": "db-cluster",
  "layer": { "index": 1, "hash": "p", "current": "p" },
  "inodes": {
    "c": { "hash": { "inode": "Q1Wv…=" } },
    "9": { "hash": { "inode": "teF0…=" } }
  }
}
```

| Key | Type | Notes |
| --- | --- | --- |
| `@context` | string | Literal `"db-cluster"` — marks this node as partitioned. Code keys all "is this a leaf or a cluster?" decisions off this. |
| `layer` | object | Present on sub-layer (non-root) nodes: `index` = depth, `hash` = name-hash prefix of this subtree, `current` = the single prefix char that led here. The **root** tenant node omits `layer`. |
| `tenant` | string | Carried into children via the config `initial`. |
| `inodes.<digit>` | object | Keyed by a single base32 digit (the child's next name-hash char). Holds only `{ "hash": { "inode": <base64> } }` — the child subtree's rolled-up hash. The actual entries live in the child database file. |

Child database files live under a directory named after the tenant DB (minus
`.json`), in subfolders derived from the name-hash path, so the tree fans out
across the filesystem rather than into one giant file.

> **Carried-over design smell.** This per-node block tree is maintained *in
> addition to* the document store's own structure, and a source comment in the
> old code openly doubted it was worth it. `SPEC.md` §5 flags collapsing the two
> as an open question for the rebuild. The **split threshold (96)** and **fan-out
> (32)** are the constants to revisit.


## 6. Per-file binary hash sidecars — `…-<level>.hashes` (not JSON)

Listed for completeness; these are **not** JSON. For each file, one memory-mapped
binary file per tree level, named `<name-hash-path>-<level>.hashes` (level encoded
in base32). Each holds a dense array of 32-byte SHA-256 block hashes:

- **Level 0** — one hash per 32 KiB block of the file's data.
- **Level n+1** — treats level *n*'s `.hashes` file as input and hashes *its*
  32 KiB blocks. Because each hash is 32 bytes, one 32 KiB block spans 1024
  hashes, so each level rolls up ~1024 hashes from the level below. Levels are
  added until a level reduces to a single hash — the file hash.

> **Discrepancy worth recording.** The file-data tree as built fans out **1024:1**
> per level (32 KiB ÷ 32-byte hash), whereas the *name* tree fans out **32:1**.
> The original design prose conflated the two. `SPEC.md` §4.3 treats keeping the
> file-data tree at 1024-way vs. unifying it on 32-way as a deliberate decision
> for the rebuild; this sidecar layout is where the 1024 actually comes from.


## Appendix — store-to-writer cross reference

Where each blob is read/written in the retired code, for anyone spelunking before
it is deleted:

| Store | Primary writers |
| --- | --- |
| `server.json` | `server()` (identity), `clock.cpp` (`time`), `rehash_tenants` (`hash`) |
| `tenants.json` | `subscriber` ctor (`database`), `rehash` (`hash.data`) |
| `subscriptions.json` | read by `tenants()` to spawn subscribers |
| `peers.json` | read by `peer_with()` to dial peers |
| `tenants.<tenant>.json` | `tree::add` / `subscriber::change::execute` (entries), `tree` partitioning, `rehash_inodes` (hashes) |
| `*.hashes` | `file::hashdb` via `rehash_file` |
