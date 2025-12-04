// Copyright (C) 2023 Advanced Micro Devices, Inc
//
// SPDX-License-Identifier: MIT
#include <aie_api/aie.hpp>        // for aie::mmul, load_v, store_v   // for window_get_ptr, window_writeincr
#include "system_settings.h"
#include <adf.h>

// Kernel: compute column-wise maxima of C = A x B using aie::mmul blocks
void matmult_float(input_window<float> *__restrict matA,
                   input_window<float> *__restrict matB,
                   output_window<float> *__restrict matColMax)
{
    // Choose a supported block configuration (M,K,N) for float.
    // You've been using M=4, K=2, N=4 -- keep that if it matches youBayGvbO_r mat layout.
    constexpr unsigned M = 4;
    constexpr unsigned K = 2;
    constexpr unsigned N = 4;

    // Derived block counts (must divide exactly)
    const unsigned rowA = F_Ra / M;   // number of M-row blocks
    const unsigned colA = F_Ca / K;   // number of K-column blocks (A's block columns)
    const unsigned colB = F_Cb / N;   // number of N-column blocks (B/C block columns)

    // Get raw pointers to the window data (correct API)
    const float* __restrict A = reinterpret_cast<const float *>(matA->ptr);
    const float* __restrict B = reinterpret_cast<const float *>(matB->ptr);

    
    // Global column maxima (one per final column = F_Cb)
    alignas(32) float colMax[F_Cb];
    for (unsigned j = 0; j < F_Cb; ++j) colMax[j] = -1e30f;

    // Small temporary block buffer to hold one MxN output block
    alignas(32) float Cblk[M * N];

    using MMUL = aie::mmul<M, K, N, float, float>;

    // Iterate over output blocks and compute using vector mmul
    for (unsigned z = 0; z < rowA; ++z) {          // block-row index (embeddings grouped by M)
        for (unsigned jb = 0; jb < colB; ++jb) {   // block-column index (queries grouped by N)
            // Create accumulator and perform block multiply-accumulate across colA
            MMUL acc;

            // First K-block (i=0)
            unsigned i = 0;
            const float *a_ptr = A + (z * colA + i) * MMUL::size_A;  // points to MxK block
            const float *b_ptr = B + (i * colB + jb) * MMUL::size_B; // points to KxN block

            auto a_block = aie::load_v<MMUL::size_A>(a_ptr);
            auto b_block = aie::load_v<MMUL::size_B>(b_ptr);
            acc.mul(a_block, b_block);

            // Remaining K-blocks (if any)
            for (i = 1; i < colA; ++i) {
                a_ptr = A + (z * colA + i) * MMUL::size_A;
                b_ptr = B + (i * colB + jb) * MMUL::size_B;
                a_block = aie::load_v<MMUL::size_A>(a_ptr);
                b_block = aie::load_v<MMUL::size_B>(b_ptr);
                acc.mac(a_block, b_block);
            }

            // Move results from acc registers to small scalar buffer Cblk (MxN)
            aie::store_v(Cblk, acc.template to_vector<float>());

            // Update global column maxima for the N columns covered by block jb
            // global column index = jb * N + n (n in 0..N-1)
            for (unsigned n = 0; n < N; ++n) {
                const unsigned gcol = jb * N + n;
                float curMax = colMax[gcol];
                // scan M rows inside this block
                for (unsigned m = 0; m < M; ++m) {
                    float v = Cblk[m * N + n]; // row-major within block: rows are contiguous groups of N
                    if (v > curMax) curMax = v;
                }
                colMax[gcol] = curMax;
            }
        }
    }

    // Write out all column maxima to output window (one float per column)
    for (unsigned j = 0; j < F_Cb; ++j) {
        window_writeincr(matColMax, colMax[j]);
    }
}


// #include <aie_api/aie.hpp>
// #include "system_settings.h"
// #include <adf.h>

// alignas(32) float colMax_buf[F_Cb];
// alignas(32) float C00_buf[16];
// alignas(32) float C01_buf[16];
// alignas(32) float C10_buf[16];
// alignas(32) float C11_buf[16];

// void matmult_float(input_window<float> *__restrict matA,
//                    input_window<float> *__restrict matB,
//                    output_window<float> *__restrict matColMax)
// {
//     constexpr unsigned M = 4;
//     constexpr unsigned K = 2;
//     constexpr unsigned N = 4;

//     const unsigned rowA = F_Ra / M;
//     const unsigned colA = F_Ca / K;
//     const unsigned colB = F_Cb / N;

//     const float* __restrict A = reinterpret_cast<const float*>(matA->ptr);
//     const float* __restrict B = reinterpret_cast<const float*>(matB->ptr);


//     float* colMax = colMax_buf;
//     float* C00 = C00_buf;
//     float* C01 = C01_buf;
//     float* C10 = C10_buf;
//     float* C11 = C11_buf;

//     for (unsigned j = 0; j < F_Cb; j++) colMax[j] = -1e30f;

//     using MMUL = aie::mmul<M,K,N,float,float>;

//     [[chess::min_loop_count(2)]]
//     for (unsigned z = 0; z < rowA; z += 2)
//     {
//         [[chess::min_loop_count(2)]]
//         for (unsigned jb = 0; jb < colB; jb += 2)
//         {
//             MMUL c00, c01, c10, c11;

//             unsigned i = 0;

//             const float* a0 = A + (z*colA + i) * MMUL::size_A;
//             const float* a1 = A + ((z+1)*colA + i) * MMUL::size_A;

//             const float* b0 = B + (i*colB + jb) * MMUL::size_B;
//             const float* b1 = B + (i*colB + jb+1) * MMUL::size_B;

//             auto va0 = aie::load_v<MMUL::size_A>(a0);
//             auto va1 = aie::load_v<MMUL::size_A>(a1);
//             auto vb0 = aie::load_v<MMUL::size_B>(b0);
//             auto vb1 = aie::load_v<MMUL::size_B>(b1);

//             c00.mul(va0, vb0);
//             c01.mul(va0, vb1);
//             c10.mul(va1, vb0);
//             c11.mul(va1, vb1);

//             [[chess::prepare_for_pipelining]]
//             for (i = 1; i < colA; i++)
//             {
//                 a0 = A + (z*colA + i) * MMUL::size_A;
//                 a1 = A + ((z+1)*colA + i) * MMUL::size_A;

//                 b0 = B + (i*colB + jb) * MMUL::size_B;
//                 b1 = B + (i*colB + jb+1) * MMUL::size_B;

//                 va0 = aie::load_v<MMUL::size_A>(a0);
//                 va1 = aie::load_v<MMUL::size_A>(a1);
//                 vb0 = aie::load_v<MMUL::size_B>(b0);
//                 vb1 = aie::load_v<MMUL::size_B>(b1);

//                 c00.mac(va0, vb0);
//                 c01.mac(va0, vb1);
//                 c10.mac(va1, vb0);
//                 c11.mac(va1, vb1);
//             }

//             aie::store_v(C00, c00.to_vector<float>());
//             aie::store_v(C01, c01.to_vector<float>());
//             aie::store_v(C10, c10.to_vector<float>());
//             aie::store_v(C11, c11.to_vector<float>());

//             for (unsigned n = 0; n < N; n++)
//             {
//                 unsigned g0 = (jb*N) + n;
//                 unsigned g1 = g0 + N;

//                 float m0 = colMax[g0];
//                 float m1 = colMax[g1];

//                 for (unsigned r = 0; r < M; r++)
//                 {
//                     float v00 = C00[r*N+n];
//                     float v10 = C10[r*N+n];
//                     float v01 = C01[r*N+n];
//                     float v11 = C11[r*N+n];

//                     if (v00 > m0) m0 = v00;
//                     if (v10 > m0) m0 = v10;

//                     if (v01 > m1) m1 = v01;
//                     if (v11 > m1) m1 = v11;
//                 }

//                 colMax[g0] = m0;
//                 colMax[g1] = m1;
//             }
//         }
//     }

//     for (unsigned j = 0; j < F_Cb; j++)
//         window_writeincr(matColMax, colMax[j]);
// }


// // Revised kernel: avoid store_v and hoist b loads; lower stack pressure
// aie_kernels/matmult_float.cpp
// #include <aie_api/aie.hpp>
// #include "system_settings.h"
// #include <adf.h>

// // Kernel: compute column-wise maxima of C = A x B using MMUL 4x2x4
// void matmult_float(input_window<float> *__restrict matA,
//                    input_window<float> *__restrict matB,
//                    output_window<float> *__restrict matColMax)
// {
//     // Block shape
//     constexpr unsigned M = 4;
//     constexpr unsigned K = 2;
//     constexpr unsigned N = 4;

//     // Derived block counts (must exactly divide the full sizes)
//     const unsigned rowA = F_Ra / M;   // number of M-row blocks
//     const unsigned colA = F_Ca / K;   // number of K-column blocks (A's block columns)
//     const unsigned colB = F_Cb / N;   // number of N-column blocks (B/C block columns)

//     // Raw pointers into the window memory (mat2file_tile must have written blocks in the same order)
//     const float* __restrict A = reinterpret_cast<const float *>(matA->ptr);
//     const float* __restrict B = reinterpret_cast<const float *>(matB->ptr);

//     // small global per-invocation scratch (fits on stack easily)
//     alignas(32) float colMax[F_Cb];
//     for (unsigned j = 0; j < F_Cb; ++j) colMax[j] = -1e30f;

//     using MMUL = aie::mmul<M, K, N, float, float>;

//     //
//     // Loop ordering: jb outer, z inner so we can reuse B-block loads across all z rows.
//     // This reduces B loads and improves memory efficiency.
//     //
//     [[chess::min_loop_count(2)]]
//     for (unsigned jb = 0; jb < colB; ++jb) {         // iterate block-columns of output (N columns per block)
//         // For each jb, we will process every block-row z (optionally unrolled by 2)
//         [[chess::min_loop_count(2)]]
//         for (unsigned z = 0; z < rowA; z += 2) {     // z += 2 unroll: process two M-row blocks together
//             // ---- First block-row (z) ----
//             MMUL acc00; // accumulator for block (z, jb)
//             unsigned i = 0;
//             const float* a0_ptr = A + (z * colA + i) * MMUL::size_A;   // pointer to MxK block of A
//             const float* b_ptr  = B + (i * colB + jb) * MMUL::size_B; // pointer to KxN block of B

//             auto a0 = aie::load_v<MMUL::size_A>(a0_ptr);
//             auto b0 = aie::load_v<MMUL::size_B>(b_ptr);
//             acc00.mul(a0, b0);

//             // ---- Second block-row (z+1) ----
//             MMUL acc10; // accumulator for block (z+1, jb)
//             const float* a1_ptr = A + ((z + 1) * colA + i) * MMUL::size_A;
//             auto a1 = aie::load_v<MMUL::size_A>(a1_ptr);
//             acc10.mul(a1, b0); // reuse b0 for same i

//             // Remaining K-blocks (accumulate)
//             [[chess::prepare_for_pipelining]]
//             for (i = 1; i < colA; ++i) {
//                 a0_ptr = A + (z * colA + i) * MMUL::size_A;
//                 a1_ptr = A + ((z + 1) * colA + i) * MMUL::size_A;
//                 b_ptr  = B + (i * colB + jb) * MMUL::size_B;

//                 a0 = aie::load_v<MMUL::size_A>(a0_ptr);
//                 a1 = aie::load_v<MMUL::size_A>(a1_ptr);
//                 b0 = aie::load_v<MMUL::size_B>(b_ptr);

//                 acc00.mac(a0, b0);
//                 acc10.mac(a1, b0);
//             }

//             // --- Extract accumulator results in-register (no store_v) ---
//             // acc.to_vector<float>() returns aie::vector<float, MMUL::size_C> with M*N lanes.
//             auto out00 = acc00.template to_vector<float>();
//             auto out10 = acc10.template to_vector<float>();

//             // Update column maxima for the N columns covered by block jb
//             const unsigned gbase = jb * N; // global column base for this block-column
//             for (unsigned n = 0; n < N; ++n) {
//                 // combine maxima from both block-rows (z and z+1)
//                 float curMax = colMax[gbase + n];

//                 // scan M rows inside out00 (rows are contiguous groups of N)
//                 for (unsigned m = 0; m < M; ++m) {
//                     const unsigned idx = m * N + n;
//                     float v = out00[idx];
//                     if (v > curMax) curMax = v;
//                 }
//                 // scan M rows inside out10
//                 for (unsigned m = 0; m < M; ++m) {
//                     const unsigned idx = m * N + n;
//                     float v = out10[idx];
//                     if (v > curMax) curMax = v;
//                 }

//                 colMax[gbase + n] = curMax;
//             }
//         } // z loop
//     } // jb loop

//     // Write out column maxima (one float per final column)
//     for (unsigned j = 0; j < F_Cb; ++j) {
//         window_writeincr(matColMax, colMax[j]);
//     }
// }



// #include <aie_api/aie.hpp>
// #include "system_settings.h"
// #include <adf.h>

// // global buffers (if needed) must match sizes; colMax_buf present already
// alignas(32) float colMax_buf[F_Cb];

// void matmult_float(input_window<float> *__restrict matA,
//                    input_window<float> *__restrict matB,
//                    output_window<float> *__restrict matColMax)
// {
//     constexpr unsigned M = 4;
//     constexpr unsigned K = 2;
//     constexpr unsigned N = 4;

//     const unsigned rowA = F_Ra / M;
//     const unsigned colA = F_Ca / K;
//     const unsigned colB = F_Cb / N;

//     const float* __restrict A = reinterpret_cast<const float*>(matA->ptr);
//     const float* __restrict B = reinterpret_cast<const float*>(matB->ptr);

//     float* colMax = colMax_buf;
//     for (unsigned j = 0; j < F_Cb; ++j) colMax[j] = -1e30f;

//     using MMUL = aie::mmul<M,K,N,float,float>;

//     // Outer loop over jb (so we can reuse B blocks)
//     [[chess::min_loop_count(2)]]
//     for (unsigned jb = 0; jb < colB; ++jb)
//     {
//         // We will compute for all z (block rows). Optionally do z+=2 unroll.
//         [[chess::min_loop_count(2)]]
//         for (unsigned z = 0; z < rowA; z += 2) // 2x unroll on z
//         {
//             // ---- First row z
//             MMUL acc00;
//             unsigned i = 0;
//             const float* a0_ptr = A + (z * colA + i) * MMUL::size_A;
//             const float* b_ptr  = B + (i * colB + jb) * MMUL::size_B;

//             auto a0 = aie::load_v<MMUL::size_A>(a0_ptr);
//             auto b0 = aie::load_v<MMUL::size_B>(b_ptr);
//             acc00.mul(a0, b0);

//             // ---- Second row z+1
//             MMUL acc10;
//             const float* a1_ptr = A + ((z+1) * colA + i) * MMUL::size_A;
//             auto a1 = aie::load_v<MMUL::size_A>(a1_ptr);
//             acc10.mul(a1, b0); // reuse b0

//             // Remaining K blocks
//             [[chess::prepare_for_pipelining]]
//             for (i = 1; i < colA; ++i)
//             {
//                 a0_ptr = A + (z * colA + i) * MMUL::size_A;
//                 a1_ptr = A + ((z+1) * colA + i) * MMUL::size_A;
//                 b_ptr   = B + (i * colB + jb) * MMUL::size_B;

//                 a0 = aie::load_v<MMUL::size_A>(a0_ptr);
//                 a1 = aie::load_v<MMUL::size_A>(a1_ptr);
//                 b0 = aie::load_v<MMUL::size_B>(b_ptr);

//                 acc00.mac(a0, b0);
//                 acc10.mac(a1, b0);
//             }

//             // --- No store_v: extract lanes directly from accumulators' vector ---
//             auto out00 = acc00.template to_vector<float>(); // in-register vector
//             auto out10 = acc10.template to_vector<float>();

//             // Update colMax for the N columns of this block jb
//             unsigned gbase = jb * N;
//             for (unsigned n = 0; n < N; ++n)
//             {
//                 float cur = colMax[gbase + n];

//                 // scan M rows for both z and z+1 (4 rows each)
//                 for (unsigned r = 0; r < M; ++r) {
//                     float v0 = out00[r * N + n];
//                     if (v0 > cur) cur = v0;
//                 }
//                 for (unsigned r = 0; r < M; ++r) {
//                     float v1 = out10[r * N + n];
//                     if (v1 > cur) cur = v1;
//                 }

//                 colMax[gbase + n] = cur;
//             }
//         } // z loop
//     } // jb loop

//     // Write out final column maxima
//     for (unsigned j = 0; j < F_Cb; ++j)
//         window_writeincr(matColMax, colMax[j]);
// }
// // 