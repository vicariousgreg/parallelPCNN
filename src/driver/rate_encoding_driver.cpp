#include <math.h>

#include "driver/rate_encoding_driver.h"

DEVICE float re_calc_input(Output output) {
    return output.f;
}

DEVICE float (*re_calc_input_ptr)(Output) = re_calc_input;

RateEncodingDriver::RateEncodingDriver(Model *model) {
    this->re_state = new RateEncodingState(model);
    this->state = this->re_state;

#ifdef PARALLEL
    cudaMemcpyFromSymbol(&this->calc_input_ptr, re_calc_input_ptr, sizeof(void *));
#else
    this->calc_input_ptr = re_calc_input_ptr;
#endif
}

/*****************************************************************************/
/************************* GENERIC IMPLEMENTATIONS ***************************/
/*****************************************************************************/

void RateEncodingDriver::step_connection(Instruction *inst) {
    step<>(inst, this->calc_input_ptr);
}

void RateEncodingDriver::step_state() {
    RateEncodingState* state = (RateEncodingState*) this->state;

#ifdef PARALLEL
    int threads = 128;
    int blocks = calc_blocks(this->state->num_neurons, threads);
    shift_output<<<blocks, threads>>>(
        (float*)state->output, this->state->num_neurons);
    cudaCheckError("Failed to update neuron output!");
    activation_function<<<blocks, threads>>>(
        (float*)state->recent_output,
        state->input,
        state->neuron_parameters,
        this->state->num_neurons);
    cudaCheckError("Failed to update neuron output!");
#else
    shift_output(
        (float*)state->output, this->state->num_neurons);
    activation_function(
        (float*)state->recent_output,
        state->input,
        state->neuron_parameters,
        this->state->num_neurons);
#endif
}

void RateEncodingDriver::step_weights() {
#ifdef PARALLEL
    cudaCheckError("Failed to update connection weights!");
#endif
}


GLOBAL void shift_output(float* outputs, int num_neurons) {
#ifdef PARALLEL
    int nid = blockIdx.x * blockDim.x + threadIdx.x;
    if (nid < num_neurons) {
#else
    for (int nid = 0 ; nid < num_neurons; ++nid) {
#endif
        float curr_value;
        float next_value = outputs[nid];
        int index;
        for (index = 0 ; index < HISTORY_SIZE-1 ; ++ index) {
            curr_value = next_value;
            next_value = outputs[num_neurons * (index + 1) + nid];
            outputs[num_neurons*index + nid] = next_value;
        }
        outputs[num_neurons*index + nid] = next_value;
    }
}

GLOBAL void activation_function(float* outputs, float* inputs,
                RateEncodingParameters* neuron_params, int num_neurons) {
#ifdef PARALLEL
    int nid = blockIdx.x * blockDim.x + threadIdx.x;
    if (nid < num_neurons) {
#else
    for (int nid = 0 ; nid < num_neurons; ++nid) {
#endif
        if (inputs[nid] > 0.0)
            outputs[nid] = tanh(0.1*inputs[nid]);
    }
}
