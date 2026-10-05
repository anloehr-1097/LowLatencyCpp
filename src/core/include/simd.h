#pragma once
// ---------------------------------------------------------------------------
// Backend selection
// ---------------------------------------------------------------------------
#if defined(__AVX512F__)
#include <immintrin.h>
#define TSIMD_AVX512 1
#define TSIMD_BACKEND_NAME "AVX512"
#elif defined(__AVX2__) && defined(__FMA__)
#include "avx2.h"
#define TSIMD_AVX2 1
#define TSIMD_BACKEND_NAME "AVX2"
#elif defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define TSIMD_SSE2 1
#define TSIMD_BACKEND_NAME "SSE2"
#elif (defined(__ARM_NEON) || defined(_M_ARM64)) &&                            \
    (defined(__aarch64__) || defined(_M_ARM64))
#include "neon.h"
#define TSIMD_NEON 1
#define TSIMD_BACKEND_NAME "NEON"
#else
#define TSIMD_SCALAR 1
#define TSIMD_BACKEND_NAME "SCALAR"
#include "scalar.h"
#endif
