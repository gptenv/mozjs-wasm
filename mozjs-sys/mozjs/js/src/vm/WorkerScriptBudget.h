/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_WorkerScriptBudget_h
#define vm_WorkerScriptBudget_h

#ifdef SERVO_WORKER_WASM

#  include <stdint.h>

#  include "mozilla/Attributes.h"
#  include "mozilla/Likely.h"

struct JSContext;

namespace js {

// Servo's Cloudflare Worker port runs SpiderMonkey on the host's only thread:
// no watchdog thread can request an interrupt while a script runs, and the
// Worker clock does not advance during synchronous execution. The embedder
// instead grants a work budget that is charged at each interrupt check (loop
// heads, function entry, finally blocks, builtin loops and regexp
// backtracking) and at calls and backward jumps. When it runs out, the next
// check requests an urgent interrupt and HandleInterrupt terminates the script
// uncatchably without consulting the embedder's interrupt callbacks. A
// negative budget means unlimited.
extern int64_t gWorkerScriptBudget;

// Number of scripts terminated because the budget ran out.
extern uint64_t gWorkerScriptBudgetTerminations;

// Slow path: request the interrupt that terminates the running script.
bool WorkerScriptBudgetExhausted(JSContext* cx);

// Returns true when the budget is exhausted and an interrupt was requested.
MOZ_ALWAYS_INLINE bool ConsumeWorkerScriptBudget(JSContext* cx,
                                                 int64_t cost = 1) {
  if (MOZ_LIKELY(gWorkerScriptBudget != 0)) {
    if (gWorkerScriptBudget > 0) {
      gWorkerScriptBudget =
          gWorkerScriptBudget > cost ? gWorkerScriptBudget - cost : 0;
    }
    return false;
  }
  return WorkerScriptBudgetExhausted(cx);
}

// Each interrupt check consumes one unit. Backward jumps are also charged one
// unit per four bytes of bytecode they jump over, so a long loop body cannot
// run many operations per check for free, and calls are charged extra so the
// budget tracks interpreter time more evenly. With these costs the Worker
// build measured 0.25-0.55 us of Node wall time per unit across plain loops,
// property access, builtin and DOM calls, and scripted calls. Charging only
// subtracts; the next interrupt check performs the termination.
constexpr int64_t WorkerScriptCallCost = 16;
constexpr int64_t WorkerScriptNativeCallCost = 8;

MOZ_ALWAYS_INLINE void ChargeWorkerScriptBudget(int64_t cost) {
  if (gWorkerScriptBudget > 0) {
    gWorkerScriptBudget =
        gWorkerScriptBudget > cost ? gWorkerScriptBudget - cost : 0;
  }
}

// Charge a jump of `delta` bytecode bytes; only backward jumps cost anything.
MOZ_ALWAYS_INLINE void ChargeWorkerScriptJump(int32_t delta) {
  if (delta < 0) {
    ChargeWorkerScriptBudget(int64_t(-delta) >> 2);
  }
}

}  // namespace js

#  define JS_WORKER_SCRIPT_BUDGET_EXHAUSTED(cx) \
    js::ConsumeWorkerScriptBudget(cx)
#  define JS_WORKER_SCRIPT_BUDGET_CALL_EXHAUSTED(cx) \
    js::ConsumeWorkerScriptBudget(cx, js::WorkerScriptCallCost)
#  define JS_WORKER_SCRIPT_BUDGET_CHARGE(cost) \
    js::ChargeWorkerScriptBudget(js::WorkerScript##cost##Cost)
#  define JS_WORKER_SCRIPT_BUDGET_CHARGE_JUMP(delta) \
    js::ChargeWorkerScriptJump(delta)
#else
#  define JS_WORKER_SCRIPT_BUDGET_CHARGE_JUMP(delta) \
    do {                                            \
    } while (0)
#  define JS_WORKER_SCRIPT_BUDGET_EXHAUSTED(cx) false
#  define JS_WORKER_SCRIPT_BUDGET_CALL_EXHAUSTED(cx) false
#  define JS_WORKER_SCRIPT_BUDGET_CHARGE(cost) \
    do {                                      \
    } while (0)
#endif  // SERVO_WORKER_WASM

#endif  // vm_WorkerScriptBudget_h
