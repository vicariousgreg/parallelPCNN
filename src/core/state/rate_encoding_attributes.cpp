#include <string>

#include "state/rate_encoding_attributes.h"
#include "util/error_manager.h"

/******************************************************************************/
/******************************** PARAMS **************************************/
/******************************************************************************/

static RateEncodingParameters create_parameters(std::string str) {
    return RateEncodingParameters(0.0);

    //ErrorManager::get_instance()->log_error(
    //    "Unrecognized parameter string: " + str);
}

/******************************************************************************/
/******************************** KERNEL **************************************/
/******************************************************************************/

#include <math.h>

BUILD_ATTRIBUTE_KERNEL(re_attribute_kernel,
    float *f_outputs = (float*)outputs;

    ,

    float next_value = f_outputs[nid];
    int index;
    for (index = 0 ; index < history_size-1 ; ++index) {
        float curr_value = next_value;
        next_value = f_outputs[size * (index + 1) + nid];
        f_outputs[size * index + nid] = next_value;
    }
    float input = inputs[nid];
    f_outputs[size * index + nid] =
        (input > 0.0) ? tanh(0.1*input) : 0.0;
        //(input > 0.0) ? input : 0.0;
        //tanh(input);
)

/******************************************************************************/
/************************** CLASS FUNCTIONS ***********************************/
/******************************************************************************/

RateEncodingAttributes::RateEncodingAttributes(LayerList &layers)
        : Attributes(layers, FLOAT, get_re_attribute_kernel()) {
    this->neuron_parameters = Pointer<RateEncodingParameters>(total_neurons);

    // Fill in table
    int start_index = 0;
    for (auto& layer : layers) {
        RateEncodingParameters params = create_parameters(layer->params);
        for (int j = 0 ; j < layer->size ; ++j)
            neuron_parameters[start_index+j] = params;
        start_index += layer->size;
    }
}

RateEncodingAttributes::~RateEncodingAttributes() {
    this->neuron_parameters.free();
}

void RateEncodingAttributes::schedule_transfer() {
    Attributes::schedule_transfer();

    this->neuron_parameters.schedule_transfer(device_id);
}

