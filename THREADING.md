# Rask — Threading & concurrency design

> The **concurrency model** for the rebuild: how work is partitioned across
> thread pools, where the system deliberately throttles itself, and the
> synchronisation and failure-handling disciplines that hold it together.
> `SPEC.md` §3 gives the library-agnostic "separation of concerns" view; this
> document is the concrete threading model that realises it.

The model is built on **reactor pools**: each pool owns an async I/O service /
event loop run by a set of worker threads and kept alive until shutdown. Work is
either posted onto a pool or driven by coroutines running on it.


## 1. The pools

A node runs **four pools**, distinguished by the *latency class* of the work
they carry rather than by their size. Each owns its own async I/O service and a
set of worker threads, alongside the filesystem-notification reader.

> Each pool's thread count is an **operator-tunable setting with a sensible
> default**. Defaults should scale with hardware and may differ by pool role — a
> latency-critical I/O pool and a CPU-bound hashing pool want different sizing.
> What matters is the set of pools below and what each is for.

| Pool | Responsibility | Why separate |
| --- | --- | --- |
| `io` | Network I/O: accept, socket read/write, heartbeats, the per-connection send loop. Also hosts the **filesystem-event reader**. | Latency-critical; must never stall behind disk or CPU work. |
| `responses` | Decide what to send a peer in reaction to a received packet (tenant / tenant-hash descent). | Short, CPU-light reactions kept off the I/O threads. |
| `files` | Filesystem mutations: create dir, allocate/write/move files; runs **sweeps** and the change pipeline; hosts the local filesystem-state bookkeeping. | Disk-bound; isolated so blocking syscalls don't choke I/O. |
| `hashes` | Long-running (re)hashing of file content and inode-tree nodes. | Lowest priority; the heaviest work, throttled (see §4). |

Two more runners share these pools rather than owning their own:

- **filesystem-event reader** — runs on the I/O pool; events arrive there and are
  immediately handed off to `files`.
- **local filesystem-state bookkeeping** — runs on `files`; its state changes are
  serialised on a strand on that pool, so they need no separate lock.


## 2. The work-handoff pattern

The recurring discipline is: **do the latency-sensitive part on `io`, then hand
the heavy part off to the right pool.** This keeps the network responsive under
load. Examples:

- A received packet is read and framed on `io`, then dispatched; handlers
  immediately hop pools — a tenant-hash comparison goes to `responses`; directory
  and file operations post their disk work to `files`.
- A received data block is verified (BLAKE3) **on the I/O thread** and only
  written if it matches — bad data is dropped without ever touching `files`.
- A sweep runs as a coroutine on `files`, walks the directory tree, and posts each
  file-hash job to `hashes`.
- Inode-tree partitioning, when a node splits, posts each child's rehash to
  `hashes` and lets the current transaction commit first, rather than holding the
  database lock across the rehash.

Outbound sending is per-connection: a coroutine on a per-connection strand drains
that connection's queue, so one connection's writes are serialised while
different connections proceed in parallel.


## 3. Shared-state synchronisation

Cross-thread state is held in thread-safe containers and atomics rather than ad
hoc locking:

- **Thread-safe (mutex-guarded) maps and sets** for the global registries: live
  connections, the watch-descriptor → tenant map, known tenants, and the
  rehash/hashing dedup sets.
- **A thread-safe ring** for each connection's outbound queue (§4).
- **Strands** — the per-connection send loop and the local-state bookkeeping each
  run on a strand, serialising related work without explicit locks.
- **Atomics** for small hot values: a connection's peer identity, a block's state,
  a tenant's rolled-up hash, the node identity, and the connection-id counter.
- **Transactional** document-store writes, so concurrent mutations to the same
  store are serialised by the store itself.


## 4. Rate limiting & back-pressure

This is where the system deliberately throttles itself. There are three internal
mechanisms plus two OS-level ceilings.

### 4a. Hashing concurrency limiter (the main throttle)

A sweep does **not** fire off a hash job for every file it finds. It holds a
concurrency limiter capped at a bounded number of in-flight hash jobs:

- Each file found takes a slot. When the cap is reached the sweep coroutine
  **yields (blocks) until a hash completes**, then continues. So the
  directory-walk producer is paced to the hashing consumer.
- The limiter **drains all outstanding jobs before the sweep returns**, so a
  sweep can't "finish" while hashes are still in flight.

Effect: no matter how large a tenant is, a sweep keeps at most a bounded number
of files being hashed at once — bounding memory, disk churn, and `hashes`-pool
contention. This is the rate-limiting behaviour to carry forward; the cap itself
is a tunable default, not a fundamental.

> Hashing is also **de-duplicated**: a file already being hashed isn't hashed
> again concurrently, and rehashing is skipped entirely when the file's stat is
> unchanged since the last recorded hash.

### 4b. Bounded outbound queue with spill (network back-pressure)

Each connection's outbound queue is a fixed-size ring. On overflow the new packet
is **dropped ("spilled"), not blocked on**.

Spilling is safe by design: the protocol is convergent, so anything lost to a
spill is rediscovered by the next hash comparison (heartbeat or sweep) and
re-sent. A fast producer therefore never blocks the sender — the queue depth
itself is the back-pressure, and the drop is the relief valve.

### 4c. Unlimited producer/consumer signal

The sender's wake-up channel only *counts* queued work and never blocks the
producer. The actual bound lives in the fixed-size ring (4b); the signal just
tells the sender how many items to drain.

### 4d. OS ceilings

- **inotify watches.** One watch per directory; at the ten-thousand-directory
  scale the kernel's `max_user_watches` limit must be raised.
- **File descriptors.** Every socket, eventfd, watch, and memory-mapped file
  consumes an fd, so the server raises its open-file limit well above the default
  at startup.


## 5. Failure handling

Each pool thread runs its event loop inside a loop wrapped by a shared exception
handler. On an uncaught exception it logs at `critical`, then either:

- **terminate on exception (default)** — flush the log queue and terminate the
  whole process; or
- **carry on** — the thread re-enters its run loop and attempts to continue.

So by default any unhandled error in any pool thread brings the node down
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

Decisions for the rebuild:

- **Pools are defined by role, not by a fixed thread count.** Keep the four
  latency classes (§1); make each pool's thread count an operator-tunable setting
  with a sensible default that scales with hardware and may differ by role.

Open questions / things to reconsider:

1. **Tuning constants.** The hash-job cap, outbound-queue size, heartbeat
   interval, reconnect watchdog, and fd limit are tuning knobs — settle on
   sensible defaults, and expose the operationally relevant ones (like the pool
   sizes above) as configuration.
2. **Terminate-on-exception default.** Whole-process abort on any thread's
   exception is blunt; decide the supervision strategy deliberately.
3. **Pool coupling.** The filesystem-event reader on `io` and the local-state
   bookkeeping on `files` share pools with unrelated work; confirm that's still
   wanted.
4. **Spill observability.** Spilled packets are only a counter today; if the new
   build keeps drop-on-full, make sure the resync that recovers them is easy to
   observe (ties to `SPEC.md` §9 terminal-log monitoring).
