#ifndef rate_encoding_operations_h
#define rate_encoding_operations_h

#include "rate_encoding_driver.h"
#include "model.h"
#include "constants.h"
#include "parallel.h"

/* Generic versions to obfuscate preprocessor directives. */
void re_update_inputs(Connection &conn, float* mData, void* outputs,
                     float* inputs, int num_neurons);

void re_update_outputs(void* outputs, float* inputs,
                 RateEncodingParameters* neuron_params, int num_neurons);

void re_update_weights();

#ifdef PARALLEL

/* Parallel implementation of activation function for activation of
 *   neural connections.  Parameters are pointers to locations in various
 *   value arrays, which allows for optimzation of memory access.  In addition,
 *   the size of the matrix is provided via |from_size| and to_size|.
 */
 
/* This parallel kernel calculates the input to one neuron, the weights of which
 *   are located in a column of the matrix.  This is efficient because threads
 *   running siactivate_matrix will access sequential data from one row.
 */
__global__ void parallel_activate_matrix(float* outputs, float* weights,
        float* inputs, int from_size, int to_size, OPCODE opcode);

/* This parallel kernel calculates the input to one neuron, which only ahs one
 *   input weight.  Weight vectors represent one-to-one neural connections.
 */
__global__ void parallel_activate_vector(float* outputs, float* weights,
                    float* inputs, int size, OPCODE opcode);

__global__ void parallel_activation_function(float* outputs, float* inputs,
                RateEncodingParameters* neuron_params, int num_neurons);

#else

/* Serial implementation of activate_matrix function for activation of
 *   neural connections */
void serial_activate_matrix(float* outputs, float* weights, float* inputs,
                        int from_size, int to_size, OPCODE opcode);

/* Serial implementation of activate_vector function for activation of
 *   neural connections */
void serial_activate_vector(float* outputs, float* weights, float* inputs,
                                        int size, OPCODE opcode);

void serial_activation_function(float* outputs, float* inputs,
                RateEncodingParameters* neuron_params, int num_neurons);


#endif

#endif
