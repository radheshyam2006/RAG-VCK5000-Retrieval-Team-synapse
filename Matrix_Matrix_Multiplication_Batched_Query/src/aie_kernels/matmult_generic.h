// Copyright (C) 2023 Advanced Micro Devices, Inc
//
// SPDX-License-Identifier: MIT

#ifndef MATMULT_GENERIC_H
#define MATMULT_GENERIC_H

#include <aie_api/aie.hpp>

/**
 * \brief Performs a matrix multiplication in chunks of MxN and KxN elements
 *
 * \param M    Number of rows in blocks of matrices A and C
 * \param K    Number of columns in a block of matrix A and rows in a block of matrix B
 * \param N    Number of columns in blocks of matrices B and C
 * \param T    Element type
 * \input rowA Number of rows of A in block units (total rows / M)
 * \input colA Number of columns of A in block units (total rows / K)
 * \input colB Number of columns of B in block units (total cols / N)
 * \input A    1D array with elements of A ordered in blocks of MxK
 * \input B    1D array with elements of B ordered in blocks of KxN
 * \input C    1D array where the result will be stored in blocks of MxN
 */
template <unsigned M, unsigned K, unsigned N, typename T>
[[gnu::always_inline]]
static void mmul_blocked(
    unsigned rowA, unsigned colA, unsigned colB, const T *__restrict A, const T *__restrict B, T *__restrict C)
{
    using MMUL = aie::mmul<M, K, N, T, T>;

    for (unsigned z = 0; z < rowA; z++) {
        for (unsigned j = 0; j < colB; j++) {
            unsigned i = 0;
            const T *a_ptr = A + (z * colA + i) * MMUL::size_A;
            const T *b_ptr = B + (i * colB + j) * MMUL::size_B;
            T       *c_ptr = C + (z * colB + j) * MMUL::size_C;

            auto block_a = aie::load_v<MMUL::size_A>(a_ptr);
            auto block_b = aie::load_v<MMUL::size_B>(b_ptr);

            MMUL block_c;
            block_c.mul(block_a, block_b);

            for (i = 1; i < colA; ++i) {
                a_ptr = A + (z * colA + i) * MMUL::size_A;
                b_ptr = B + (i * colB + j) * MMUL::size_B;

                block_a = aie::load_v<MMUL::size_A>(a_ptr);
                block_b = aie::load_v<MMUL::size_B>(b_ptr);

                block_c.mac(block_a, block_b);
            }

            aie::store_v(c_ptr, block_c.template to_vector<T>());
        }
    }
}

#endif // MATMULT_GENERIC_H
