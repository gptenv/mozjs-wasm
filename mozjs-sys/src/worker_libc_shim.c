/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Cloudflare Workers (and the other wasm hosts this fork targets) load
 * wasm32-unknown-unknown modules directly via WebAssembly.instantiate()
 * with a small hand-written import object -- they do not provide a real
 * WASI runtime. But we still link wasi-sysroot's libc.a for its malloc,
 * string, printf-family, and locale implementations, which are correct
 * and not worth reimplementing (verified: removing libc.a entirely leaves
 * 83 undefined symbols spanning locale/wide-char/printf internals, far
 * more surface than is safe to hand-roll).
 *
 * Those 83 higher-level functions are themselves implemented on top of a
 * much smaller set of raw WASI syscalls (wasi_snapshot_preview1::*), and
 * *that* is the layer that actually can't be satisfied by a Worker.
 *
 * The fix implemented here is not a JS-side WASI shim (that would just
 * push the "no real filesystem/clock" problem onto whoever writes the
 * Worker's import object, and would require Cloudflare's own WASI support,
 * which is still labeled experimental and covers only a subset of
 * syscalls). Instead, this file provides our own definitions of the
 * higher-level POSIX functions that wasi-sysroot's precompiled libc.a
 * objects implement. Verified empirically (see the investigation this
 * file resulted from): wasi-libc's syscall-level WASI import declarations
 * are baked directly into its precompiled .o files as real wasm imports,
 * not resolvable by providing a same-named replacement for the raw
 * syscall function -- but static archives only pull in a .o member to
 * satisfy a symbol that is *still undefined* when the archive is
 * searched. Defining the higher-level function ourselves (e.g.
 * `clock_gettime`, not the raw `clock_time_get` import) means the
 * archive's own object for that function, and therefore its internal WASI
 * import, is never linked in at all.
 *
 * This eliminates all 15 wasi_snapshot_preview1 imports the module
 * otherwise requires, replacing them with 2-3 small `env`-namespaced
 * imports the Worker actually can supply.
 */

#include <stddef.h>
#include <stdint.h>

/* ---- time: clock_gettime (backs js::PRMJ_Now() / Date.now(), and TLS
 * certificate/session-ticket validation via rustls-pki-types::UnixTime) --
 *
 * CLOCK_REALTIME needs true wall-clock time; CLOCK_MONOTONIC reuses the
 * import mozilla::TimeStamp already relies on (see TimeStamp_worker.cpp)
 * -- monotonic and wall-clock are genuinely different clocks (monotonic
 * has an arbitrary origin, e.g. Node's process.hrtime, and must never be
 * fed into a wall-clock/Unix-epoch computation), so they get separate
 * host imports rather than sharing one.
 */
/* Must match wasi-sysroot's actual layout exactly (see
 * include/__typedef_time_t.h and include/__struct_timespec.h): time_t is
 * `long long` (64-bit, to avoid the 2038 problem), not `long` (32-bit on
 * wasm32) -- getting this wrong doesn't just risk memory corruption in
 * struct fields, it changes clock_gettime/time's wasm-level function
 * signature (return type i64 vs i32), which traps immediately with
 * "signature mismatch" the moment a real caller compiled against the
 * actual header invokes it. */
struct worker_timespec {
  long long tv_sec;
  long tv_nsec;
};

#define WORKER_CLOCK_REALTIME 0
#define WORKER_CLOCK_MONOTONIC 1

extern uint64_t servo_worker_monotonic_now_ns(void);

/* Host-facing import, matching the worker_monotonic_now_ns / worker_log_error
 * naming convention used elsewhere in this fork (servo_worker_* is reserved
 * for the internal Rust-wrapper-callable-from-C++ layer; this call site has
 * no such wrapper, so it imports directly). */
__attribute__((import_module("env"), import_name("worker_unix_time_now_ns")))
extern uint64_t worker_unix_time_now_ns(void);

int clock_gettime(int clk_id, struct worker_timespec *tp) {
  uint64_t ns = (clk_id == WORKER_CLOCK_MONOTONIC) ? servo_worker_monotonic_now_ns()
                                                    : worker_unix_time_now_ns();
  tp->tv_sec = (long long)(ns / 1000000000ULL);
  tp->tv_nsec = (long)(ns % 1000000000ULL);
  return 0;
}

/* time_t is 64-bit here (see the struct comment above) -- this must return
 * long long, not long, or the wasm function signature itself (i64 vs i32
 * return) mismatches what real callers compiled against <time.h> expect. */
long long time(long long *out) {
  struct worker_timespec ts;
  clock_gettime(WORKER_CLOCK_REALTIME, &ts);
  if (out) {
    *out = ts.tv_sec;
  }
  return ts.tv_sec;
}

/* ---- stdio output: write (backs fprintf/fputs/fputc/perror/vfprintf when
 * targeting stdout/stderr -- functions writing only into a string buffer,
 * like snprintf/vsnprintf/sprintf, never call this). Routed to the same
 * error-reporting host import used for panic/error messages elsewhere in
 * this fork (see ports/servo-js-wasm/lib.rs's diplomat_throw_error_js). */
__attribute__((import_module("env"), import_name("worker_log_error")))
extern void worker_log_error(const uint8_t *ptr, size_t len);

long write(int fd, const void *buf, unsigned long count) {
  if (fd == 1 || fd == 2) {
    worker_log_error((const uint8_t *)buf, (size_t)count);
    return (long)count;
  }
  return -1;
}

/* musl-derived libcs (wasi-libc included) route buffered stdio
 * (fprintf/fputs/fwrite/...) through internal __stdio_write/__stdio_read,
 * which call writev()/readv() directly -- *not* the public write()/read()
 * above. Both need overriding, or wasi-libc's own writev.o/readv.o (each
 * with their own real WASI import) still get linked in. Traced via
 * llvm-objdump on wasi-sysroot's __stdio_write.o/__stdio_read.o rather
 * than assumed. */
struct worker_iovec {
  void *iov_base;
  unsigned long iov_len;
};

long writev(int fd, const struct worker_iovec *iov, int iovcnt) {
  if (fd != 1 && fd != 2) {
    return -1;
  }
  long total = 0;
  for (int i = 0; i < iovcnt; i++) {
    worker_log_error((const uint8_t *)iov[i].iov_base, (size_t)iov[i].iov_len);
    total += (long)iov[i].iov_len;
  }
  return total;
}

long readv(int fd, const struct worker_iovec *iov, int iovcnt) {
  (void)fd;
  (void)iov;
  (void)iovcnt;
  return 0; /* EOF */
}

/* Also musl-internal, called by __stdio_seek (traced the same way). */
long long __lseek(int fd, long long offset, int whence) {
  (void)fd;
  (void)offset;
  (void)whence;
  return -1;
}

/* Called by __stdout_write before it decides whether to go through the
 * line-buffered or fully-buffered path; a Worker is never attached to a
 * real terminal. */
int __isatty(int fd) {
  (void)fd;
  return 0;
}

/* fcntl/ioctl are the only remaining wasi-sysroot objects referencing
 * both fd_fdstat_get and fd_fdstat_set_flags together (traced via
 * llvm-nm across every extracted libc.a member; isatty.o only needs
 * fd_fdstat_get alone, already covered by __isatty above). There is no
 * real file descriptor here to query or reconfigure flags on. */
int fcntl(int fd, int cmd, ...) {
  (void)fd;
  (void)cmd;
  return -1;
}

int ioctl(int fd, unsigned long request, ...) {
  (void)fd;
  (void)request;
  return -1;
}

/* ---- filesystem: there is no real filesystem here. `read`/`close` fail
 * the way they would for a file that doesn't exist; callers in this
 * codebase that use them (optional GC/debug config files, self-hosted
 * code caches) already treat "not found" as a normal, handled outcome
 * rather than a hard error. */
long read(int fd, void *buf, unsigned long count) {
  (void)fd;
  (void)buf;
  (void)count;
  return 0; /* EOF */
}

int close(int fd) {
  (void)fd;
  return -1;
}

/* `open`, `remove`, `rmdir`, and `unlink` are all compiled into a single
 * wasi-sysroot translation unit ("posix.o"; confirmed via `ar t` on
 * libc.a). Needing *any one* of them from that object pulls in all four,
 * along with their real WASI imports (path_open, path_remove_directory,
 * path_unlink_file) -- so all four have to be overridden together, not
 * just the one a given call site happens to need, or the object still
 * gets linked for whichever one is left. `RandomNum.cpp`'s
 * `open("/dev/urandom", ...)` is one such direct caller. */
int open(const char *path, int flags, ...) {
  (void)path;
  (void)flags;
  return -1;
}

int remove(const char *path) {
  (void)path;
  return -1;
}

int rmdir(const char *path) {
  (void)path;
  return -1;
}

int unlink(const char *path) {
  (void)path;
  return -1;
}

/* `fopen` bypasses the public `open()` above entirely -- it calls this
 * wasi-libc-internal helper directly (confirmed via llvm-nm on wasi-
 * sysroot's fopen.o), which in turn is what actually reaches path_open
 * and the fd_prestat_get/fd_prestat_dir_name preopened-directory
 * resolution machinery. */
int __wasilibc_open_nomode(const char *path, int oflag) {
  (void)path;
  (void)oflag;
  return -1;
}

/* ---- environment: a Worker has no environment variables to report. This
 * is the normal "unset" outcome for `getenv`, which every real call site
 * (GC zeal tuning, MOZ_LOG, locale overrides) already treats as "use the
 * default", not an error. */
char *getenv(const char *name) {
  (void)name;
  return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
  (void)name;
  (void)value;
  (void)overwrite;
  return -1;
}

/* ---- process exit: there is no process to exit. A Worker request that
 * hits this has hit an unrecoverable condition; trap immediately rather
 * than pretending an exit-then-continue model exists. */
__attribute__((noreturn)) void exit(int code) {
  (void)code;
  __builtin_trap();
}

__attribute__((noreturn)) void _Exit(int code) {
  (void)code;
  __builtin_trap();
}

/* Not `abort`: memory/mozalloc already defines it for this target
 * (Unified_cpp_memory_mozalloc0.o), and a second definition here is a
 * duplicate-symbol link error rather than a silent override. */
