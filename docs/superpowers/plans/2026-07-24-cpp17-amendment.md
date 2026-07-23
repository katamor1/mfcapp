# C++17 Constraint Plan Amendment

> This amendment is authoritative where it conflicts with `2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md` or the CSV mock amendment.

**Decision source:** Product owner instruction on 2026-07-24.

## Changed assumptions

- The complete solution is constrained to C++17.
- C++20 language and library features are not permitted in production code, test code, adapters, or MFC code.
- The architecture and CSV/COM replacement boundary remain unchanged.

## Required implementation changes

- Set the shared MSBuild language standard to `stdcpp17`.
- Add `/Zc:__cplusplus` and a compile-time test requiring the C++17 language level.
- Replace defaulted C++20 equality and `<=>` operators with explicit comparisons.
- Replace `std::jthread`/`std::stop_token` with `std::thread`, an atomic stop flag, and explicit join.
- Replace `std::atomic<std::shared_ptr<const MachineSnapshot>>` with C++17 `std::atomic_load` and `std::atomic_compare_exchange` free functions operating on a `std::shared_ptr` object.
- Keep the provisional ID catalog and CSV-backed adapter behind `IMachineStateReader` and `IMachineCommandGateway`.

## Amendments to later tasks

### Task 9 and later MFC work

All new MFC shell, routing, view, and composition-root code must compile under C++17. No task may introduce a per-file C++20 override.

### Task 11 operation executor

Use `std::thread`, condition variables, atomics, and explicit lifecycle management instead of `std::jthread` or stop tokens.

### Task 13 COM adapter

The real COM adapter, STA executor, catalog, and codecs must expose the existing Application ports and must compile as C++17. The adapter swap must not require a language-level change in Domain, Application, or Presentation.

## Acceptance additions

- `_MSVC_LANG` or `__cplusplus` is exactly `201703L` in the shared bootstrap test.
- Debug/Release × Win32/x64 build and test successfully with `/std:c++17`.
- No source file contains `<=>`, `std::jthread`, `std::stop_token`, or `std::atomic<std::shared_ptr`.
- No project or source file overrides the shared setting with C++20 or later.
