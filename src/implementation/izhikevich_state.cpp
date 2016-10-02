#include <cstdlib>
#include <cstdio>
#include <string>

#include "izhikevich_state.h"
#include "../framework/tools.h"
#include "../framework/parallel.h"

#define DEF_PARAM(name, a,b,c,d) \
    static const IzhikevichParameters name = IzhikevichParameters(a,b,c,d);

/******************************************************************************
 **************************** INITIALIZATION **********************************
 ******************************************************************************/

/* Izhikevich Parameters Table */
DEF_PARAM(DEFAULT          , 0.02, 0.2 , -70.0, 2   ); // Default
DEF_PARAM(REGULAR          , 0.02, 0.2 , -65.0, 8   ); // Regular Spiking
DEF_PARAM(BURSTING         , 0.02, 0.2 , -55.0, 4   ); // Intrinsically Bursting
DEF_PARAM(CHATTERING       , 0.02, 0.2 , -50.0, 2   ); // Chattering
DEF_PARAM(FAST             , 0.1 , 0.2 , -65.0, 2   ); // Fast Spiking
DEF_PARAM(LOW_THRESHOLD    , 0.02, 0.25, -65.0, 2   ); // Low Threshold
DEF_PARAM(THALAMO_CORTICAL , 0.02, 0.25, -65.0, 0.05); // Thalamo-cortical
DEF_PARAM(RESONATOR        , 0.1 , 0.26, -65.0, 2   ); // Resonator
DEF_PARAM(PHOTORECEPTOR    , 0   , 0   , -82.6, 0   ); // Photoreceptor
DEF_PARAM(HORIZONTAL       , 0   , 0   , -82.6, 0   ); // Horizontal Cell

static IzhikevichParameters create_parameters(std::string str) {
    if (str == "random positive") {
        // (ai; bi) = (0:02; 0:2) and (ci; di) = (-65; 8) + (15;-6)r2
        float a = 0.02;
        float b = 0.2; // increase for higher frequency oscillations

        float rand = fRand(0, 1);
        float c = -65.0 + (15.0 * rand * rand);

        rand = fRand(0, 1);
        float d = 8.0 - (6.0 * (rand * rand));
        return IzhikevichParameters(a,b,c,d);
    } else if (str == "random negative") {
        //(ai; bi) = (0:02; 0:25) + (0:08;-0:05)ri and (ci; di)=(-65; 2).
        float rand = fRand(0, 1);
        float a = 0.02 + (0.08 * rand);

        rand = fRand(0, 1);
        float b = 0.25 - (0.05 * rand);

        float c = -65.0;
        float d = 2.0;
        return IzhikevichParameters(a,b,c,d);
    }
    else if (str == "default")            return DEFAULT;
    else if (str == "regular")            return REGULAR;
    else if (str == "bursting")           return BURSTING;
    else if (str == "chattering")         return CHATTERING;
    else if (str == "fast")               return FAST;
    else if (str == "low_threshold")      return LOW_THRESHOLD;
    else if (str == "thalamo_cortical")   return THALAMO_CORTICAL;
    else if (str == "resonator")          return RESONATOR;
    else if (str == "photoreceptor")      return PHOTORECEPTOR;
    else if (str == "horizontal")         return HORIZONTAL;
    else throw ("Unrecognized parameter string: " + str).c_str();
}

void IzhikevichState::build(Model* model) {
    this->model = model;
    int num_neurons = model->num_neurons;

    // Local spikes for output reporting
    this->spikes = (int*) allocate_host(num_neurons * HISTORY_SIZE, sizeof(int));
    this->input = (float*) allocate_host(num_neurons, sizeof(float));
    this->voltage = (float*) allocate_host(num_neurons, sizeof(float));
    this->recovery = (float*) allocate_host(num_neurons, sizeof(float));
    this->neuron_parameters =
        (IzhikevichParameters*) allocate_host(num_neurons, sizeof(IzhikevichParameters));

    // Keep pointer to recent spikes
    // This will be the output pointer
    this->output = &this->spikes[(HISTORY_SIZE-1) * num_neurons];

    // Fill in table
    for (int i = 0 ; i < num_neurons ; ++i) {
        std::string &param_string = model->parameter_strings[i];
        IzhikevichParameters params = create_parameters(param_string);
        this->neuron_parameters[i] = params;
        this->input[i] = 0;
        this->voltage[i] = params.c;
        this->recovery[i] = params.b * params.c;
    }

#ifdef PARALLEL
    // Allocate space on GPU and copy data
    this->device_input = (float*)
        allocate_device(num_neurons, sizeof(float), this->input);
    this->device_voltage = (float*)
        allocate_device(num_neurons, sizeof(float), this->voltage);
    this->device_recovery = (float*)
        allocate_device(num_neurons, sizeof(float), this->recovery);
    this->device_spikes = (int*)
        allocate_device(num_neurons * HISTORY_SIZE, sizeof(int), this->spikes);
    this->device_neuron_parameters = (IzhikevichParameters*)
        allocate_device(num_neurons, sizeof(IzhikevichParameters), this->neuron_parameters);
    this->device_output = (void*) &this->device_spikes[(HISTORY_SIZE-1) * num_neurons];
#endif
    this->weight_matrices = build_weight_matrices(model, 1);
}