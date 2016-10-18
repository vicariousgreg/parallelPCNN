#include <math.h>

#include "driver/rate_encoding_driver.h"

DEVICE float re_calc_input(float output) {
    return output;
}

DEVICE float (*re_calc_input_ptr)(float) = re_calc_input;

RateEncodingDriver::RateEncodingDriver(Model *model) {
    this->re_state = new RateEncodingState(model);
    this->state = this->re_state;

#ifdef PARALLEL
    cudaMemcpyFromSymbol(&this->calc_input_ptr, re_calc_input_ptr, sizeof(void *));
#else
    this->calc_input_ptr = re_calc_input_ptr;
#endif

    for (int i = 0; i < model->connections.size(); ++i) {
        Connection *conn = model->connections[i];
        this->instructions.push_back(
            new Instruction<float>(conn,
                (float*) this->state->output,
                this->state->input,
                this->state->get_matrix(conn->id)));
    }
}

/*****************************************************************************/
/************************* GENERIC IMPLEMENTATIONS ***************************/
/*****************************************************************************/

void RateEncodingDriver::step_connections() {
    for (int i = 0; i < this->instructions.size(); ++i) {
        Instruction<float> *inst = this->instructions[i];
        step<float>(inst, this->calc_input_ptr);
    }
}

void RateEncodingDriver::step_state() {
    RateEncodingState* state = (RateEncodingState*) this->state;

#ifdef PARALLEL
    int threads = 128;
    int blocks = calc_blocks(this->state->num_neurons, threads);
    activation_function<<<blocks, threads>>>(
        (float*)state->output,
        state->input,
        state->neuron_parameters,
        this->state->num_neurons);
    cudaCheckError("Failed to update neuron output!");
#else
    activation_function(
        (float*)state->output,
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
