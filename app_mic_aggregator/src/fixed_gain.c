// Copyright 2022 XMOS LIMITED.
// This Software is subject to the terms of the XCORE VocalFusion Licence.
///  Utilities for applying fixed gain as a multiplier to a buffer
///


#include "fixed_gain.h"
// #include "xs3_util.h"

#include <stdint.h>
#include <xcore/assert.h>

#define VLMUL_POST_SHIFT (30)

/**
 * @brief Count leading sign bits of `int32_t`.
 * 
 * This function returns the number of most-significant bits
 * in `a` which are equal to its sign bit.
 * 
 * @param[in] a Input value
 * 
 * @returns Number of leading sign bits
 */
static inline unsigned cls(
    const int32_t a)
{
#ifdef __XS3A__

    unsigned res;
    asm( "cls %0, %1" : "=r"(res) : "r"(a) );
    return res;

#else

    if(a == 0 || a == -1)
        return 32;
    
    if( a > 0 ){
        for(int i = 30; i >= 0; i--){
            if(a & (1<<i)) return 31-i;
        }
    } else {
        for(int i = 30; i >= 0; i--){
            unsigned mask = (1<<i);
            if((a | mask) != a) return 31-i;
        }
    }
    assert(0);
    return 0;

#endif //__XS3A__
}

void fixed_gain_set_multiplier(fixed_gain_t* fg, int32_t multiplier, int32_t buf_size) {
	xassert(NULL != fg);

	// To support gain of -1 or 0 (which have 32 sign bits) an extra
	// branch would be required in this function to clamp headroom
	// to <= 30
	xassert(multiplier > 0);
	xassert(buf_size <= VECTOR_SIZE);

	// vstrpv requires a byte mask of LSB bytes to copy
	// out of Vr
	int n_bytes = buf_size * sizeof(int32_t);
	asm("mkmsk %0, %1" : "=r"(fg->vstrpv_mask) : "r"(n_bytes));

	// count sign bits, -1 as the top sign bit must not 
	// be shifted away.
	// As we assert multiplier is greater that zero we know that 
	// cls(multiplier) - 1 <= VLMUL_POST_SHIFT is always true.
	int32_t headroom = cls(multiplier) - 1;

	// input shift must be minus number as there is no left shift instruction
	fg->input_shift = -VLMUL_POST_SHIFT + headroom; // mul_shift + input_shift == VLMUL_POST_SHIFT
	multiplier <<= headroom;

	for(int i = 0; i < buf_size; ++i) {
		fg->mul_buf[i] = multiplier;
	}
}


void fixed_gain_apply(const fixed_gain_t* fg, int32_t* input) {
	asm volatile (
		"ldc r11, 0\n"
		"vsetc r11\n"               // Vc = 0, configures vector unit to 32 bit
		"vlashr %0[0], %1\n"        // Vr = input >> input_shift
		"vlmul %2[0]\n"             // Vr *= mul_buf
		"vstrpv %0[0], %3\n"        // input[0:buf_size] = Vr[0:buf_size]
		: : "r"(input), "r"(fg->input_shift), "r"(fg->mul_buf), "r"(fg->vstrpv_mask) : "r11"
	);
}
