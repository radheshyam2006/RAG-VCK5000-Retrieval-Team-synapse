// Copyright (C) 2023 Advanced Micro Devices, Inc
//
// SPDX-License-Identifier: MIT

#pragma once

#include <adf.h>
#include "system_settings.h"

void matmult_float(
    adf::input_buffer_1d<float, NSAMPLES_WINDOW_F_A>& __restrict matA,
    adf::input_buffer_1d<float, NSAMPLES_WINDOW_F_B>& __restrict matB,
    adf::output_buffer_1d<float, NSAMPLES_WINDOW_F_C>& __restrict matColMax);



