# LowLatencyCppMastery

Low-latency C++ benchmarking primitives. AArch64 (`cntvct_el0`, 1 GHz / 1 ns per tick on M-series) and x86_64 (`rdtsc`/`rdtscp` + `lfence`, TSC rate calibrated once against `steady_clock`). Requires C++23.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench
```

`compile_commands.json` is emitted to `build/` for clangd / IDE tooling.

## API

All functions live in `namespace bench` (header: `bench/il_benchmark.hpp`).

### Timer primitives: `timer_start()` / `timer_stop()` / `timer_freq()`

Arch-specific counter reads. Leading barrier before the read, trailing barrier after the stop read (`isb` on ARM64, `lfence`/`rdtscp` on x86). `timer_freq()` returns ticks/sec. Hot paths are `always_inline`. Unsupported archs `#error`.

### `keep(T& v)`

Output sink. Emits an empty `asm volatile` with `"+r"` / `"memory"` constraints so the compiler must materialize `v` in a register and treat it as read+written. This prevents constant folding and dead-code elimination by compiler. Used to pin intermediate results inside benchmark loops.

### `il_benchmark(F&& f, Args&&... args)` → `std::pair<result, uint64_t>`

Times a **single** invocation of `f(args...)` with one `timer_start()`/`timer_stop()` pair around it. Perfect-forwards the args; sinks the result with `keep()`.

- **Use case:** work well above the counter's tick period (hundreds of ns+).
- **Caveat:** unreliable for sub-tick work — per-sample timer/barrier overhead (~30+ cycles) dominates the measurement, and quantization reads 0 most of the time.

### `benchmark_N<N>(F&& f, Args&&... args)` → `uint64_t`

Times **N** iterations as a **single aggregate** — one timer pair around the whole loop. Sinks each iteration's result with `keep()` so the loop body can't be DCE'd. Args are captured once by reference (`std::forward_as_tuple`, outside the timed window; they must outlive the timed calls).

- **Use case:** sub-tick work (the default choice for fast functions). The timer overhead amortizes to ~0 per iteration and the work accumulates above one tick, giving a meaningful per-rep cost = `total / N`.
- **Requirement:** `f` must return non-void so the sink has something to pin.

### `benchmark_samples<N, M = 0>(F&& f, Args&&... args)` → `std::vector<uint64_t>`

Reuses `il_benchmark` per iteration and collects the N tick counts. `reserve(N)` upfront; `push_back` runs strictly after the stop timer read so vector bookkeeping never enters the timed window.

- **Warmup:** the optional second template argument `M` runs M untimed warmup iterations before sampling (primes caches / branch predictors / frequency). Since args are captured by reference, warmup may mutate stateful `f` — timed samples see post-warmup state.

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
| Portable / cross-check | `wallclock_spend` | Not tied to the CPU counter |

## Tricky bits
- make sure the inputs passed to the benchmark functions cannot be optimized away by the compiler (constant folding). This can be achieved by declaring them volatile.


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

- Timers are arch-specific: `mrs cntvct_el0` + `isb` on ARM64, `rdtsc`/`rdtscp` + `lfence` on x86_64 (TSC assumed invariant; frequency calibrated against `steady_clock`). Other archs fail at compile time.
- Defeat constant folding by feeding `volatile` / runtime inputs — compile-time-constant args let the optimizer fold the work away, measuring nothing.
- `keep()` is the single tool that makes the benchmarks honest: sink each iteration's result inside any loop you time.

- Still need to test the implementation in bench for intel arch. For now, dev on `aarch64` only.


