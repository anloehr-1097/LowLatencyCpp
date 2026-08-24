# LowLatencyCppMastery

AArch64 low-latency C++ benchmarking primitives. Targets Apple Silicon (`cntvct_el0` virtual counter at 1 GHz / 1 ns per tick on M-series) and requires C++23.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench
```

`compile_commands.json` is emitted to `build/` for clangd / IDE tooling.

## API

All functions live in `namespace bench` (header: `bench/il_benchmark.hpp`).

### `keep(T& v)`

Output sink. Emits an empty `asm volatile` with `"+r"` / `"memory"` constraints so the compiler must materialize `v` in a register and treat it as read+written. This prevents constant folding and dead-code elimination by compiler. Used to pin intermediate results inside benchmark loops.

### `il_benchmark(F&& f, Args&&... args)` → `std::pair<result, uint64_t>`

Times a **single** invocation of `f(args...)` with one `mrs cntvct_el0` pair around it. Perfect-forwards the args; sinks the result with `keep()`.

- **Use case:** work well above the counter's tick period (hundreds of ns+).
- **Caveat:** unreliable for sub-tick work — per-sample timer overhead (~30+ cycles of `mrs`/`isb`) dominates the measurement, and quantization reads 0 most of the time.

### `benchmark_N<N>(F&& f, Args&&... args)` → `uint64_t`

Times **N** iterations as a **single aggregate** — one `mrs` pair around the whole loop. Sinks each iteration's result with `keep()` so the loop body can't be DCE'd. Args are captured once into a `std::tuple` (outside the timed window).

- **Use case:** sub-tick work (the default choice for fast functions). The timer overhead amortizes to ~0 per iteration and the work accumulates above one tick, giving a meaningful per-rep cost = `total / N`.
- **Requirement:** `f` must return non-void so the sink has something to pin.

### `benchmark_samples<N>(F&& f, Args&&... args)` → `std::vector<uint64_t>`

Reuses `il_benchmark` per iteration and collects the N tick counts. `reserve(N)` upfront; `push_back` runs strictly after each stop `mrs` so vector bookkeeping never enters the timed window.

- **Use case:** statistical evaluation (min / median / variance) of workloads where each call **exceeds the tick period**.
- **Caveat:** not adequate for sub-tick `f` — per-sample timer overhead swamps the signal. Use `benchmark_N` instead.

### `wallclock_spend(F&& f, Args&&... args)` → `std::chrono::duration<double>`

Times a call with `std::chrono::steady_clock` and sinks the result. Independent of the CPU virtual counter.

- **Use case:** portability validation, cross-checking `benchmark_N`, and measuring workloads long enough that `steady_clock`'s ~µs overhead is negligible.
- **Caveat:** for sub-µs work, wrap an N-rep loop and divide by N to amortize the `now()` overhead — a single-shot wallclock of a sub-ns operation is dominated by the stopwatch itself.

## Choosing a function

| Work per call | Recommended | Why |
|---|---|---|
| Sub-tick (< ~tens of ns) | `benchmark_N` | Aggregate window amortizes timer overhead |
| Many ticks (hundreds of ns+) | `il_benchmark` or `benchmark_samples` | Single-shot is meaningful |
| Statistical distribution | `benchmark_samples` | Returns per-iteration samples |
| Portable / cross-check | `wallclock_spend` | Not tied to `cntvct_el0` |

## Project layout

```
.
├── CMakeLists.txt
├── bench/
│   ├── il_benchmark.hpp   # all benchmark primitives
│   └── main.cpp           # usage examples
└── src/core/              # reserved for future core library code
```

## Notes

- All CPU-timed functions use AArch64 `mrs cntvct_el0` with `isb` barriers. Not portable off ARM64.
- Defeat constant folding by feeding `volatile` / runtime inputs — compile-time-constant args let the optimizer fold the work away, measuring nothing.
- `keep()` is the single tool that makes the benchmarks honest: sink each iteration's result inside any loop you time.
