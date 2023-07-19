// Copyright 2022 XMOS LIMITED.
// This Software is subject to the terms of the XCORE VocalFusion Licence.
/// fixed gain utility
#ifndef FIXED_GAIN_H
#define FIXED_GAIN_H

#include <stddef.h>
#include <stdint.h>

/// Number of words in the vector registers
#define VECTOR_SIZE 8

/// Gain calculation has some setup involved for using the vector unit.
/// This structure holds the post init state
typedef struct {
	int32_t mul_buf[VECTOR_SIZE]; /// precalculated gain multiplier
	int32_t input_shift; /// shift to apply to input before multiplying
	uint32_t vstrpv_mask; /// mask for loading out data
} fixed_gain_t;

/// This function and the below multiply all the values in an array of up to VECTOR_SIZE
/// by the same value, saturating on overflow. `fixed_gain_apply` uses the vector unit 
/// to get this done very quickly, there is some setup required to to this though and
/// that only needs to be done once for a value of multiplier. The simple part of the setup
/// is that a vector of size VECTOR_SIZE needs to be created that will be applied to the inputs.
/// The complex part is that a left shift must be applied to the inputs of the vector unit
/// to get the correct output. The amount of shift depends on the value of the multiplier.
///
/// @note saturation will occur at INT32_MAX and -1 * INT32_MAX. **Not INT2_MIN**. this
/// behaviour is implemented by the vector unit
///
/// @param fg state being constructed for use by fixed_gain_apply
/// @param multiplier value that will be applied to all the inputs in fixed_gain_apply, must be 
/// greater than 0
/// @param max multiplier value that will ever be applied to all the inputs in fixed_gain_apply, must be 
/// greater than 0
/// @param buf_size size of input buffer that will be passed to fixed_gain_apply, must be no more
/// than VECTOR_SIZE
void fixed_gain_init_all_multipliers(fixed_gain_t* fg, int32_t multiplier, int32_t max_multiplier, int32_t buf_size);


void fixed_gain_set_single_multiplier(fixed_gain_t* fg, int32_t multiplier, int32_t idx);


/// @brief Do in place saturating multiplication of all elements in input by the multiplier specified
/// in `fixed_gain_set_multiplier`
void fixed_gain_apply(const fixed_gain_t* fg, int32_t* input);


#endif // FIXED_GAIN_H
