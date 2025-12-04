/**********
© Copyright 2020-2022 Xilinx, Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**********/
// "/\n© Copyright 2020-2022 Xilinx, Inc.\nLicensed under the Apache License, Version 2.0 (the \"License\");\nyou may not use this file except in compliance with the License.\nYou may obtain a copy of the License at\n    http://www.apache.org/licenses/LICENSE-2.0\nUnless required by applicable law or agreed to in writing, software\ndistributed under the License is distributed on an \"AS IS\" BASIS,\nWITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\nSee the License for the specific language governing permissions and\nlimitations under the License.\n**/";
#include <aie_api/aie.hpp>
#include <aie_api/aie_adf.hpp>
#include "system_settings.h"

const uint32 pktType = 0;


static inline void matmult_float_buf(const float* __restrict A,
									 const float* __restrict B,
									 float* __restrict colMax,
									 unsigned Ra, unsigned Ca,
									 unsigned Rb, unsigned Cb) {
	constexpr unsigned M = 4;
	constexpr unsigned K = 2;
	constexpr unsigned N = 4;

	for (unsigned j = 0; j < Cb; ++j) colMax[j] = -1e30f;

	const unsigned rowA = Ra / M;
	const unsigned colA = Ca / K;
	const unsigned colB = Cb / N;
	using MMUL = aie::mmul<M, K, N, float, float>;
	alignas(32) float Cblk[M * N];

	for (unsigned z = 0; z < rowA; ++z) {
		for (unsigned jb = 0; jb < colB; ++jb) {
			MMUL acc;

			const float *a_ptr = A + (z * colA + 0) * MMUL::size_A;
			const float *b_ptr = B + (0 * colB + jb) * MMUL::size_B;
			auto a0 = aie::load_v<MMUL::size_A>(a_ptr);
			auto b0 = aie::load_v<MMUL::size_B>(b_ptr);
			acc.mul(a0, b0);

			
			for (unsigned i = 1; i < colA; ++i) {
				a_ptr = A + (z * colA + i) * MMUL::size_A;
				b_ptr = B + (i * colB + jb) * MMUL::size_B;
				auto ai = aie::load_v<MMUL::size_A>(a_ptr);
				auto bi = aie::load_v<MMUL::size_B>(b_ptr);
				acc.mac(ai, bi);
			}

			aie::store_v(Cblk, acc.template to_vector<float>());

			for (unsigned n = 0; n < N; ++n) {
				const unsigned gcol = jb * N + n;
				float curMax = colMax[gcol];
				for (unsigned m = 0; m < M; ++m) {
					float v = Cblk[m * N + n];
					if (v > curMax) curMax = v;
				}
				colMax[gcol] = curMax;
			}
		}
	}
}

void aie_core1(input_pktstream *in0, input_stream<int32> *in1, output_pktstream *out) {

	readincr(in0);
	uint32 ID = getPacketid(out, 0);
	writeHeader(out, pktType, ID);

	constexpr unsigned M = 4;
	constexpr unsigned K = 2;
	constexpr unsigned N = 4;

	const unsigned Ra = F_Ra;
	const unsigned Ca = F_Ca;
	const unsigned Rb = F_Rb;
	const unsigned Cb = F_Cb;

	static float A[F_Ra * F_Ca];
	static float B[F_Rb * F_Cb];
	bool tlast;
	for (unsigned i = 0; i < Ra * Ca; ++i) { int32 v = readincr(in0, tlast); A[i] = (float)v; }
	for (unsigned i = 0; i < Rb * Cb; ++i) { int32 v = readincr(in1);         B[i] = (float)v; }
	static float colMax[F_Cb];
	matmult_float_buf(A, B, colMax, Ra, Ca, Rb, Cb);

	for (unsigned j = 0; j < Cb; ++j) {
		writeincr(out, (int32)colMax[j], j == (Cb - 1));
	}
}

/// this is the code for outer product 

// #include <aie_api/aie.hpp>
// #include <aie_api/aie_adf.hpp>
// #include "system_settings.h"

// // Packet type forwarded to output
// const uint32 pktType = 0;

// // Configuration constants for this kernel
// constexpr unsigned Ra_cfg = 4;   // rows of Q (queries)
// constexpr unsigned Ca_cfg = 16;  // cols of Q (embedding dim)
// constexpr unsigned Rb_cfg = 16;  // rows of V (= Ca)
// constexpr unsigned Cb_cfg = 4;   // cols of V (database vectors)

// // Block sizes for MMUL tiling
// constexpr unsigned M = 4;
// constexpr unsigned Kt = 2; // small k-block size (matches aie::mmul K)
// constexpr unsigned N = 4;

// // Default: output row-wise maxima. Undefine to output full C matrix.
// #ifndef OUTPUT_MAXIMA
// #define OUTPUT_MAXIMA 1
// #endif

// // Safety: tile-local memory budget in bytes (VCK5000 safe baseline)
// constexpr size_t TILE_LOCAL_BYTES = 32 * 1024;
// constexpr size_t STACK_RESERVE = 8 * 1024; // reserve for stack/other usage

// // Static BSS buffers (tile-local, aligned). These keep memory off the stack.
// static float C_accum[Ra_cfg * Cb_cfg] __attribute__((aligned(32)));
// static float A_block[Ra_cfg * Kt] __attribute__((aligned(32)));
// static float B_block[Kt * Cb_cfg] __attribute__((aligned(32)));

// // Compile-time memory safety check
// static_assert((sizeof(C_accum) + sizeof(A_block) + sizeof(B_block) + STACK_RESERVE) <= TILE_LOCAL_BYTES,
//               "Tile-local buffers + stack reserve exceed TILE_LOCAL_BYTES (32KB). Reduce allocations.");

// void aie_core1(input_pktstream *inV, input_stream<int32> *inQ, output_pktstream *out) {
//     // 1) Consume input pktstream header for V and forward header to output
//     readincr(inV); // consume header word from inV
//     uint32 ID = getPacketid(out, 0);
//     writeHeader(out, pktType, ID);

//     // 2) Dimensions
//     const unsigned Ra = Ra_cfg;
//     const unsigned Ca = Ca_cfg;
//     const unsigned Rb = Rb_cfg;
//     const unsigned Cb = Cb_cfg;
//     const unsigned K = (Ca < Rb) ? Ca : Rb;
//     const unsigned k_block = Kt;
//     const unsigned Ksteps = K / k_block;

//     // 3) Initialize accumulator to zero
//     for (unsigned i = 0; i < Ra * Cb; ++i) C_accum[i] = 0.0f;

//     // Temporary MxN buffer used for storing mmul block outputs
//     float Cblk[M * N] __attribute__((aligned(32)));

//     // 4) Streaming k-block outer-product accumulation
//     using MMUL = aie::mmul<M, Kt, N, float, float>;
//     const unsigned rowA = Ra / M;
//     const unsigned colB = Cb / N;

//     for (unsigned step = 0; step < Ksteps; ++step) {
//         const unsigned kbase = step * k_block;

//         // Read A_block from broadcast stream inQ
//         for (unsigned kb = 0; kb < k_block; ++kb) {
//             for (unsigned r = 0; r < Ra; ++r) {
//                 int32 v = readincr(inQ);
//                 A_block[r * k_block + kb] = (float)v;
//             }
//         }

//         // Read B_block from pktstream inV
//         for (unsigned kb = 0; kb < k_block; ++kb) {
//             bool tlast = false;
//             for (unsigned j = 0; j < Cb; ++j) {
//                 int32 v = readincr(inV, tlast);
//                 B_block[kb * Cb + j] = (float)v;
//             }
//         }

//         // Tile the partial product with mmul blocks
//         for (unsigned z = 0; z < rowA; ++z) {
//             for (unsigned jb = 0; jb < colB; ++jb) {
//                 MMUL acc;
//                 const float *a_ptr = A_block + (z * M) * k_block;
//                 const float *b_ptr = B_block + jb * MMUL::size_B;
//                 auto a_vec = aie::load_v<MMUL::size_A>(a_ptr);
//                 auto b_vec = aie::load_v<MMUL::size_B>(b_ptr);
//                 acc.mul(a_vec, b_vec);
//                 aie::store_v(Cblk, acc.template to_vector<float>());

//                 for (unsigned m = 0; m < M; ++m) {
//                     const unsigned global_r = z * M + m;
//                     float *Crow = &C_accum[global_r * Cb];
//                     for (unsigned n = 0; n < N; ++n) {
//                         const unsigned global_c = jb * N + n;
//                         Crow[global_c] += Cblk[m * N + n];
//                     }
//                 }
//             }
//         }
//     }

//     // 5) Post-process and write output
// #if OUTPUT_MAXIMA
//     for (unsigned r = 0; r < Ra; ++r) {
//         float rowMax = -1e30f;
//         for (unsigned j = 0; j < Cb; ++j) {
//             float v = C_accum[r * Cb + j];
//             if (v > rowMax) rowMax = v;
//         }
//         const bool last = (r == (Ra - 1));
//         writeincr(out, (int32)rowMax, last);
//     }
// #else
//     for (unsigned r = 0; r < Ra; ++r) {
//         for (unsigned j = 0; j < Cb; ++j) {
//             const bool last = (r == (Ra - 1)) && (j == (Cb - 1));
//             writeincr(out, (int32)C_accum[r * Cb + j], last);
//         }
//     }
// #endif
// }
