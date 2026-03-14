# Issue Tracker

Format: `[ID] [LIB] [PRI] file:line — description`

Priority: **P1** correctness/safety · **P2** robustness/contract · **P3** quality/maintainability

Status: `open` | `fixed` | `wontfix(reason)`

---

## asrtl

| ID | Pri | Location | Issue | Status |
|----|-----|----------|-------|--------|
| L01 | P1 | `chann.c: asrtl_chann_cobs_dispatch` | Return values of `asrtl_cobs_ibuffer_insert` and `asrtl_chann_dispatch` are silently ignored | **fixed** |
| L02 | P2 | `cobs.c: asrtl_cobs_ibuffer_insert` | Recursive call — stack overflow on adversarial input; convert to iterative | **fixed** |
| L03 | P2 | `cobs.h: asrtl_cobs_encoder_iter` | No bounds guard; contract not documented; callers must not let `enc.p` reach `out_end` before call, but this is invisible | **fixed** |
| L04 | P2 | `status.h:19` | XXX: error codes across recv paths need revision — some paths return generic codes instead of useful distinctions | **fixed** |
| L05 | P3 | `core_proto.h:32` | XXX: "stop running test" message ID defined but unimplemented | open |
| L06 | P3 | `chann.c: asrtl_chann_cobs_dispatch` | Log source says `"test_rsim"` not `"asrtl_chann"` | **fixed** |
| L07 | P1 | `util.h: asrtl_u8d4_to_u32` | `uint8_t` shifted left as `int` — UB when high bit set (`data[0] << 24` overflows `int32_t` when `data[0] >= 0x80`). `u8d2_to_u16` is fine (`0xFF << 8 = 65280` fits in `int32_t`). Must cast to `uint32_t` before shifting in the `u32` variant. | **fixed** |
| L08 | P2 | `core_proto.h: asrtl_msg_rtoc_desc`, `asrtl_msg_rtoc_test_info` | Only check buffer has room for the 2-byte header, then silently truncate payload via `asrtl_fill_buffer` — no SIZE_ERR when description is truncated | **wontfix: truncation intentional — description is informational only** |
| L09 | P2 | `cobs.c: asrtl_cobs_ibuffer_iter` | Does not consume the 0-terminator after decoding; leaves it as a subsequent empty-message result. Surprising API: every message produces two iter calls. Callers must discard empty results. | **wontfix: intentional — documented in header** |
| L10 | P3 | `span.h: asrtl_span_unfit_for` | Casts `uint32_t size` to `int32_t` — technically UB if size > INT32_MAX. Also `ptrdiff_t` result implicitly compared as `int32_t`. | **fixed** |
| L11 | P3 | `cobs.c: asrtl_cobs_encode_buffer` | `out_ptr` variable is set but never used after `asrtl_cobs_encoder_init` | **fixed** |
| L12 | P3 | `core_proto.h` | `asrtl_msg_ctor_test_start` and `asrtl_msg_rtoc_test_start` have identical implementations — one appears to be an accidental duplicate | **fixed** |
| L13 | P3 | `cobs.c: asrtl_cobs_ibuffer_insert` | Uses `int` for byte counts (`s`, `capacity`) instead of `ptrdiff_t` — silently wrong for spans > 2 GB (theoretical but incorrect type) | **fixed** |
| L14 | P3 | `cobs.c`, `cobs.h`, `chann.h` | Uses bare `assert()` for precondition checks — callers on embedded targets cannot opt out or redirect failures; replace with a custom `ASRTL_ASSERT(x)` macro that defaults to `assert(x)` but can be overridden at compile time | open |

## asrtr

| ID | Pri | Location | Issue | Status |
|----|-----|----------|-------|--------|
| R01 | P2 | `asrtr.c:139` | Protocol version hardcoded as `(0,1,0)` — XXX noted | **fixed** |
| R02 | P2 | `asrtr.c:167` | Flag fallthrough branch (`else` at end of chain) silently zeroes flags without reporting error | **fixed** |
| R03 | P2 | `asrtr.c:260,269` | Rapid repeat of test-start or test-info messages is unhandled — XXX noted | **fixed** |
| R04 | P2 | `asrtr.c:315` | Test registration not locked after first tick — XXX noted | **fixed** |
| R05 | P3 | `asrtr.c: asrtr_reactor_add_test` | Linear scan to find list tail on every `add_test` call — O(n) per registration; a tail pointer in `asrtr_reactor` would make it O(1) | **fixed** |
| R06 | P2 | `asrtr.c` | Error logging audit: scan all error-path `if` branches in `asrtr.c` and ensure every failure return emits an `ASRTL_ERR_LOG` before returning | **fixed** |
| R07 | P3 | `asrtr.c` | Uses bare `assert()` for precondition checks — replace with a custom `ASRTR_ASSERT(x)` macro that can be overridden at compile time | open |

## asrtc

| ID | Pri | Location | Issue | Status |
|----|-----|----------|-------|--------|
| C01 | P2 | `allocator.c:17` | Magic upper-bound constant `10000` in `asrtc_realloc_str` — no rationale | **fixed** |
| C02 | P2 | `controller.c: ASRTC_STAGE_WAITING` | All five handlers (`init`, `test_count`, `desc`, `test_info`, `test_exec`) can get stuck in `ASRTC_STAGE_WAITING` indefinitely if the remote never replies — no timeout or escape path exists anywhere in the controller | **fixed** |
| C03 | P2 | `controller.c:79` | Protocol version is received but never validated — XXX noted | **fixed** |
| C04 | P3 | `controller.c:309` | Received `tid` in test-info response is parsed but unused | **fixed** |
| C05 | P3 | `controller.c:401` | Received `line` in test-result is parsed but unused | **wontfix: C10 drops `line` from the protocol** |
| C10 | P3 | `core_proto.h`, `asrtr.c`, `controller.c` | Drop `line` from `TEST_RESULT` protocol message — field is never consumed by the controller; remove from encoder, record struct, `asrtr_assert`, and parser | **fixed** |
| C06 | P3 | `controller.c:502` | Error code comment: different code should be used for trailing bytes — XXX noted | **fixed** |
| C07 | P3 | `handlers.h:21` | Callback typedef location XXX — belongs in a dedicated `callbacks.h` or `handlers.h`, already split but comment remains | **fixed** |
| C08 | P3 | `controller.c` | Uses bare `assert()` for precondition checks — replace with a custom `ASRTC_ASSERT(x)` macro that can be overridden at compile time | open |
| C08 | P1 | `controller.c: asrtc_cntr_recv_test_exec` | `TEST_RESULT` handler compares `rid` (run_id from message) against `h->res.test_id` instead of `h->res.run_id` — wrong field, test passes coincidentally because test uses equal values | **fixed** |
| C09 | P1 | `allocator.c: asrtc_realloc_str` | No null check after `asrtc_alloc` — if allocator returns NULL (OOM), `memcpy(NULL, …)` and `res[s-1]='\\0'` will crash | **fixed** |

## test/

| ID | Pri | Location | Issue | Status |
|----|-----|----------|-------|--------|
| T01 | P2 | `collector.h` | Function *definitions* in a header — ODR violation if included from multiple TUs | **fixed** |
| T02 | P2 | `asrtl_test.c:183` | XXX: corner cases missing for `asrtl_fill_buffer` | **fixed** |
| T03 | P2 | `asrtc_test.c:228` | XXX: desc callback doesn't receive test ID — open design question | open |
| T04 | P3 | coverage | Error paths untested: `asrtl_chann_dispatch` unknown-channel, `asrtc_cntr_recv` bad states, `asrtr_reactor_recv` duplicate messages | open |

---

## Coverage gaps (asrtc — deep read complete)

| ID | What is missing |
|----|-----------------|
| C-cov1 | `ASRTC_CNTR_BUSY_ERR` path never tested — no test calls a second operation while the controller is not idle |
| C-cov2 | `ASRTL_MSG_ERROR` path in `asrtc_cntr_recv` never triggered — error callback never invoked in tests |
| C-cov3 | `ASRTC_TEST_ERROR` result never produced — `result_cb` is only exercised with `ASRTL_TEST_SUCCESS` |
| C-cov4 | Wrong-run_id path in `asrtc_cntr_recv_test_exec` (`rid != h->res.test_id` branch) never triggered — masked by coincidentally equal values in the one test |
| C-cov5 | `ASRTC_CNTR_IDLE` recv path (`ASRTL_RECV_UNEXPECTED_ERR`) not tested |
| C-cov6 | Truncated-message recv error paths (unfit_for checks returning RECV_ERR) not tested for any handler |

---

## Coverage gaps (asrtr — deep read complete)

| ID | What is missing |
|----|-----------------|
| R-cov1 | `ASRTR_TEST_ERROR` path never exercised: no test has `continue_f` return a non-`ASRTR_SUCCESS` value (the path that forces `record->state = ASRTR_TEST_ERROR` and sends `ASRTL_TEST_ERROR` result) |
| R-cov2 | Multiple flags set simultaneously not tested: no test sends two requests before ticking; one-flag-per-tick behaviour is correct but unverified |
| R-cov3 | `asrtr_reactor_recv` error paths untested: truncated `TEST_INFO` (missing u16 ID), truncated `TEST_START` (missing u16+u32), and messages with trailing bytes |
| R-cov4 | `ASRTR_SEND_ERR` path from `asrtr_reactor_tick_flags` and `ASRTR_REAC_TEST_REPORT` send never triggered (64-byte buffer is always large enough in tests) |

---

## Coverage gaps (asrtl — deep read complete)

| ID | What is missing |
|----|-----------------|
| L-cov1 | `asrtl_chann_find` and `asrtl_chann_dispatch` have no isolation tests; `head==NULL` path of `asrtl_chann_dispatch` untested |
| L-cov2 | `asrtl_span_unfit_for` has no direct test |
| L-cov3 | `asrtl_u8d2_to_u16` / `asrtl_u8d4_to_u32` never tested with values where high bit is set (which triggers L07 UB) |
| L-cov4 | `asrtl_fill_buffer` corner cases: zero-size source, zero-capacity destination (T02) |
| L-cov5 | `asrtl_cobs_ibuffer_insert` second-recursive path (insert after shift still fails) — confirmed unreachable by code logic, but not explicitly shown by test |
| L-cov6 | `asrtl_chann_cobs_dispatch`: no test where `ibuffer_insert` returns SIZE_ERR (ibuffer too small for incoming data) |
