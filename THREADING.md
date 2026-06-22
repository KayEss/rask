# Rask — Threading & concurrency design

> Reference description of the **concurrency model** of the current (now-retired)
> implementation, including its rate-limiting / back-pressure behaviour. It
> records how work is partitioned across thread pools and where the system
> deliberately throttles itself, so the rebuild (`SPEC.md`) can preserve the
> properties that matter and drop the accidents. `SPEC.md` §3 gives the
> library-agnostic "separation of concerns" view; this document is the concrete
> realisation.

The model is built on **reactor pools**: each pool owns one async I/O service
(Boost ASIO `io_service`) run by a fixed set of worker threads, kept alive by a
work guard until shutdown. Work is `post()`ed onto a pool, or driven by
coroutines (`boost::asio::spawn`) running on it.


## 1. The pools

A node constructs **four pools of 8 threads each (32 worker threads)**, plus the
filesystem-notification reader and the FRP wiring, all in one `workers` struct
(`workers.cpp:53`). The pools are *not* sized to the machine — 8 is hard-coded
(the pool default would otherwise be `hardware_concurrency()`).

| Pool | Threads | Responsibility | Why separate |
| --- | --- | --- | --- |
| `io` | 8 | Network I/O: accept, socket read/write, heartbeats, the per-connection send loop. Also hosts the **inotify reader**. | Latency-critical; must never stall behind disk or CPU work. |
| `responses` | 8 | Decide what to send a peer in reaction to a received packet (tenant / tenant-hash descent). | Short, CPU-light reactions kept off the I/O threads. |
| `files` | 8 | Filesystem mutations: create dir, allocate/write/move files; runs **sweeps** and the change pipeline; hosts the FRP `local_server`. | Disk-bound; isolated so blocking syscalls don't choke I/O. |
| `hashes` | 8 | Long-running (re)hashing of file content and inode-tree nodes. | Lowest priority; the heaviest work, throttled (see §4). |

Two more runners share these pools rather than owning their own:

- **inotify reader** — constructed on `w.io` (`notification.cpp:50`); filesystem
  events arrive on an I/O thread and are immediately `post()`ed onward to `files`.
- **FRP wiring (`local_server`)** — constructed on `w.files` (`workers.hpp:39`);
  its internal state changes run inside a **strand** on that pool so they
  serialise without a separate lock.


## 2. The work-handoff pattern

The recurring discipline is: **do the latency-sensitive part on `io`, then
`post()` the heavy part to the right pool.** This keeps the network responsive
under load. Examples:

- A received packet is read and framed on `io` (`read_and_process`), then
  dispatched; handlers immediately hop pools — e.g. `tenant_hash_packet` posts its
  tree comparison to `responses`; `create_directory` / `file_exists` /
  `file_data_block` post their disk work to `files`.
- `file_data_block` verifies the block's BLAKE3 **on the I/O thread** and only
  posts the disk write to `files` if it matches — bad data is dropped without ever
  touching `files`.
- A sweep runs as a coroutine on `files`, walks the directory tree, and posts each
  file-hash job to `hashes`.
- Inode-tree partitioning, when a node splits, posts each child's rehash to
  `hashes` and lets the current transaction commit first, rather than holding the
  database lock across the rehash (`tree.cpp`).

Outbound sending is per-connection: a coroutine on a **`sending_strand`** drains
that connection's queue, so a single connection's writes are serialised while
different connections proceed in parallel.


## 3. Shared-state synchronisation

Cross-thread state is held in thread-safe containers and atomics rather than ad
hoc locking:

- **`tsmap` / `tsset`** (mutex-guarded maps/sets) for global registries:
  `g_connections` (live sockets), `g_watches` (inotify wd → tenant), `g_tenants`,
  `g_inodes` / `g_hashing` (rehash/hashing dedup sets).
- **`tsring`** — the per-connection outbound queue (§4).
- **strands** — `sending_strand` (per connection) and the FRP `barrier` serialise
  without explicit locks.
- **`std::atomic`** — connection peer `identity`, inode block `state`, tenant
  `hash`, the global server identity, and the connection-id counter.
- Document-store writes are **transactional** (`transformation` + `commit`), so
  concurrent mutations to the same beanbag are serialised by the store itself.


## 4. Rate limiting & back-pressure

This is where the system deliberately throttles itself. There are three internal
mechanisms plus two OS-level ceilings.

### 4a. Hashing concurrency limiter (the main throttle)

A sweep does **not** fire off a hash job for every file it finds. It holds an
`eventfd::limiter` capped at **8 concurrent hash jobs** (`sweep.folder.cpp:38`,
`f5::eventfd::limiter limit(w.hashes.get_io_service(), yield, 8)`):

- Each file found takes a job slot (`++limit`). If 8 are already outstanding, the
  sweep coroutine **yields (blocks) until a hash completes**, then continues. So
  the directory-walk producer is paced to the hashing consumer.
- A job signals completion (via the eventfd) from the `rehash_file` callback; the
  limiter's destructor **drains all outstanding jobs before the sweep returns**,
  so a sweep can't "finish" while hashes are still in flight.

Effect: no matter how large a tenant is, a sweep keeps at most 8 files being
hashed at once — bounding memory, disk churn, and `hashes`-pool contention. This
is the rate-limiting behaviour to carry forward; the cap of 8 is a tuning
constant, not a fundamental.

> Hashing is also **de-duplicated**: `g_hashing` ensures a file already being
> hashed isn't hashed again concurrently, and `rehash_file` skips work entirely
> when the file's `stat` is unchanged since the last recorded hash.

### 4b. Bounded outbound queue with spill (network back-pressure)

Each connection's outbound queue is a fixed **256-slot ring** (`tsring`,
`connection.cpp:150`). On overflow the new packet is **dropped ("spilled"), not
blocked on** (`queue()` uses the predicate form of `push_back` that refuses the
insert and bumps the `packets/spills` counter).

Spilling is safe by design: the protocol is convergent, so anything lost to a
spill is rediscovered by the next hash comparison (heartbeat or sweep) and
re-sent. This lets a fast producer never block the sender thread — the queue
depth itself is the back-pressure, and the drop is the relief valve.

### 4c. Unlimited producer/consumer signal

The sender's wake-up channel is an `eventfd::unlimited` — it only *counts* queued
work and never blocks the producer. The actual bound lives in the 256-slot ring
(4b), not in this signal; the signal just tells the sender how many items to
drain.

### 4d. OS ceilings

- **inotify watches.** One watch per directory; on failure the log points at
  `/proc/sys/fs/inotify/max_user_watches` (`notification.cpp:185`). At the
  ten-thousand-directory scale this kernel limit must be raised.
- **File descriptors.** Every socket, eventfd, watch, and memory-mapped file
  consumes an fd, so the server raises `RLIMIT_NOFILE` to **20 480** at startup
  (`main.cpp`).


## 5. Failure handling

Each pool thread runs its `io_service` inside a loop wrapped by a shared exception
handler (`workers.cpp`). On an uncaught exception it logs at `critical`, then:

- if `terminate on exception` is **true (default)** — flush the log queue and
  `std::terminate()` the whole process;
- if **false** — the handler returns `true` and the thread re-enters `run()`,
  attempting to carry on.

So by default any unhandled error in any of the 32 threads brings the node down
(after flushing logs) rather than leaving it in a half-working state.


## 6. Notes for the rebuild

Properties worth preserving:

- **Tiered pools by latency class** (I/O / response / file / hash), with the
  do-it-on-I/O-then-hand-off discipline.
- **A concurrency limiter on hashing** so discovery can't flood the hashers
  (§4a) — the single most important throttle.
- **Bounded, spill-on-full outbound queues** that rely on convergence for safety
  (§4b).
- **Per-connection send serialisation** and transactional store writes instead of
  broad locking.

Open questions / things to reconsider:

1. **Fixed 8-thread pools.** Hard-coded regardless of core count and of pool role
   (I/O vs. CPU-bound hashing want different sizing). Revisit whether these should
   scale with hardware and differ per pool.
2. **Tuning constants.** The hash limit (8), queue size (256), heartbeat (5 s),
   reconnect watchdog (15 s), and fd limit (20 480) are scattered magic numbers —
   gather and justify them.
3. **Terminate-on-exception default.** Whole-process abort on any thread's
   exception is blunt; decide the supervision strategy deliberately.
4. **Pool coupling.** The inotify reader on `io` and the FRP server on `files`
   share pools with unrelated work; confirm that's still wanted.
5. **Spill observability.** Spilled packets are only a counter today; if the new
   build keeps drop-on-full, make sure the resync that recovers them is easy to
   observe (ties to `SPEC.md` §9 terminal-log monitoring).
