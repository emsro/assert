# Project Instructions

## Building and Running the Project

Always use `cmake --workflow --preset <name>` to build and run tests. Never use bare `cmake --build` or `ctest` in isolation. Available presets are defined in `CMakePresets.json`.

## Fix Workflow

When fixing an issue:
1. Write a test that reproduces the problem first.
2. Build and confirm the test fails (manifesting the issue).
3. Apply the fix.
4. Build and confirm all tests pass.
