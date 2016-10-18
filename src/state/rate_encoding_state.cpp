#include <cstdlib>
#include <cstdio>
#include <string>

#include "state/rate_encoding_state.h"
#include "tools.h"
#include "parallel.h"

/******************************************************************************
 **************************** INITIALIZATION **********************************
 ******************************************************************************/

static RateEncodingParameters create_parameters(std::string str) {
    return RateEncodingParameters(0.0);
    //throw ("Unrecognized parameter string: " + str).c_str();
}

RateEncodingState::RateEncodingState(Model* model) : State(model, 1, sizeof(float)) {
    RateEncodingParameters* local_params =
        (RateEncodingParameters*) allocate_host(num_neurons, sizeof(RateEncodingParameters));

    // Fill in table
    for (int i = 0; i < model->layers.size(); ++i) {
        Layer *layer = model->layers[i];
        int start = layer->index;
        std::string &param_string = layer->params;
        RateEncodingParameters params = create_parameters(param_string);
        for (int j = 0 ; j < layer->size ; ++j) {
            local_params[start+j] = params;
        }
    }

#ifdef PARALLEL
    // Allocate space on GPU and copy data
    this->neuron_parameters = (RateEncodingParameters*)
        allocate_device(num_neurons, sizeof(RateEncodingParameters), local_params);
    free(local_params);
#else
    this->neuron_parameters = local_params;
#endif
}

RateEncodingState::~RateEncodingState() {
#ifdef PARALLEL
    cudaFree(this->neuron_parameters);
#else
    free(this->neuron_parameters);
#endif
}
