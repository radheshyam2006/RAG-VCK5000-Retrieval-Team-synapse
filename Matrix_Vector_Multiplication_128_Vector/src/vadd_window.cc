// Copyright (C) 2023 Advanced Micro Devices, Inc
//
// SPDX-License-Identifier: MIT

#include <aie_api/aie.hpp>
#include <aie_api/aie_adf.hpp>
#include <aie_api/utils.hpp>


void aie_vadd_window(input_window<float> *in0, input_window<float> *in1, output_window<float> *out){

    float dot_product = 0;
    aie::vector<float, 16> a = window_readincr_v<16>(in0);

    float max_dot = -1;
    float index = -1;
    for (unsigned int i=0; i<256; i++) {
        aie::vector<float, 16> b = window_readincr_v<16>(in1);
        auto c = aie::mul(a, b);
        auto va = c.to_vector<float>(0);
        dot_product = aie::reduce_add(va);
        if (dot_product > max_dot) {
            max_dot = dot_product;
            index = i;
        }
    }

    window_writeincr(out, max_dot);
    window_writeincr(out, index);
}