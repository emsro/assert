# Review Plan: C Libraries

Libraries under review: `asrtl`, `asrtc`, `asrtr` and their tests in `test/`.
Goal: bring all three to high standard — correct, robust, well-tested, clean.

## Process

For each library in order (`asrtl` → `asrtr` → `asrtc`):

1. **Read** all files (implementation + headers + tests)
2. **File issues** into `issues.md` — tag with inline `// ID` comments in source
3. **Stop — await human review** of filed issues before proceeding
4. Human approves/adjusts triage, then signals to continue
5. **Fix** approved items in priority order: test first, fix, verify
6. **Stop — await human review** of fixes for that library
7. Human approves, then move to next library

## Verification gate

Every fix must pass:
```
cmake --workflow --preset asan
cmake --workflow --preset ubsan
```
Zero leaks, zero sanitizer errors.

## Definition of done

- All P1 + P2 issues resolved
- Every public function has an error-path test
- No `XXX` left without a rationale or tracked issue
- Both sanitizer workflows green

## Future review pass: error logging audit

**Rule:** Every `if` branch that handles a failure return from a subcall (or any other error path) must emit an `ASRTL_ERR_LOG` (or equivalent) before returning/propagating. Silent error returns are not acceptable.

Applies to all three libraries (`asrtl`, `asrtc`, `asrtr`) and their C++ wrappers.
File any violations found as new issues with priority P2.
