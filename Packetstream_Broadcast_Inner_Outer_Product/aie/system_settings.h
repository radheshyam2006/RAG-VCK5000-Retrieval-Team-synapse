#pragma once

// Dimensions for matmul C = A x B
// Ensure F_Rb == F_Ca and F_Rc == F_Ra, F_Cc == F_Cb.
#define F_Ra 128
#define F_Ca 32
#define F_Rb (F_Ca)
#define F_Cb 32
#define F_Rc (F_Ra)
#define F_Cc (F_Cb)
