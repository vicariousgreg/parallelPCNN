#include <math.h>

#include "driver/rate_encoding_driver.h"

DEVICE float re_calc_input(float output) {
    return output;
}

DEVICE float (*re_calc_input_ptr)(float) = re_calc_input;

RateEncodingDriver::RateEncodingDriver () {
    this->re_state = new RateEncodingState();
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

void RateEncodingDriver::step_connection(Connection *conn) {
#ifdef PARALLEL
    float* outputs = (float*)this->state->device_output;
#else
    float* outputs = (float*)this->state->output;
#endif
    step<float>(this->state, conn, this->calc_input_ptr,
        outputs + conn->from_layer->index);
}

void RateEncodingDriver::step_output() {
    RateEncodingState* state = (RateEncodingState*) this->state;

#ifdef PARALLEL
    int blocks = calc_blocks(this->model->num_neurons);
    activation_function<<<blocks, THREADS>>>(
        (float*)state->device_output,
        state->device_input,
        state->device_neuron_parameters,
        this->model->num_neurons);
    cudaCheckError("Failed to update neuron output!");
#else
    activation_function(
        (float*)state->output,
        state->input,
        state->neuron_parameters,
        this->model->num_neurons);
#endif
}

void RateEncodingDriver::step_weights() {
#ifdef PARALLEL
    cudaCheckError("Failed to update connection weights!");
#endif
}


GLOBAL void activation_function(float* outputs, float* inputs,
                RateEncodingParameters* neuron_params, int num_neurons) {
#ifdef PARALLEL
    int nid = blockIdx.x * blockDim.x + threadIdx.x;
    if (nid < num_neurons and inputs[nid] > 0.0) {
        outputs[nid] = tanh(0.1*inputs[nid]);
    }
#else
    for (int nid = 0 ; nid < num_neurons; ++nid) {
        if (inputs[nid] > 0.0) {
            outputs[nid] = tanh(0.1*inputs[nid]);
        }
    }
#endif
}
