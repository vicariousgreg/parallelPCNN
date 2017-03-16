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
/**************************** HEBBIAN LEARNING ********************************/
/******************************************************************************/

#define LEARNING_RATE 0.1

#define EXTRACT_OUT(to_index) \
    float dest_out = extractor(destination_outputs[to_index], 0);

#define UPDATE_WEIGHT(from_index, weight_index, dest_out) \
    float source_out = extractor(outputs[from_index], delay); \
    float old_weight = weights[weight_index]; \
    weights[weight_index] = old_weight + \
        (LEARNING_RATE * source_out * \
            (dest_out - (source_out*old_weight))); \

#define UPDATE_WEIGHT_CONVOLUTIONAL(index) \
    EXTRACT_OUT(index); \
    float weight_delta = 0.0; \
    float old_weight = weights[index]; \
    for (int i = 0 ; i < to_size ; ++i) { \
        float source_out = extractor(outputs[index], delay); \
        weight_delta += source_out * \
            (dest_out - (source_out*old_weight)); \
    } \
    weights[index] = old_weight + (LEARNING_RATE * weight_delta);

CALC_FULLY_CONNECTED(update_fully_connected_hebbian,
    ; ,
    EXTRACT_OUT(to_index);,
    UPDATE_WEIGHT(from_index, weight_index, dest_out);,
    ; );
CALC_ONE_TO_ONE(update_one_to_one_hebbian,
    ; ,
    EXTRACT_OUT(index);
    UPDATE_WEIGHT(index, index, dest_out);
    );
CALC_CONVERGENT(update_convergent_hebbian,
    ; ,
    EXTRACT_OUT(to_index);,
    UPDATE_WEIGHT(from_index, weight_index, dest_out);,
    ; );
CALC_ONE_TO_ONE(update_convolutional_hebbian,
    ; ,
    UPDATE_WEIGHT_CONVOLUTIONAL(index);
    );

Kernel<SYNAPSE_ARGS>RateEncodingAttributes::get_updater(ConnectionType conn_type) {
    switch (conn_type) {
        case FULLY_CONNECTED:
            return get_update_fully_connected_hebbian();
        case ONE_TO_ONE:
            return get_update_one_to_one_hebbian();
        case CONVERGENT:
            return get_update_convergent_hebbian();
        case CONVOLUTIONAL:
            return get_update_convolutional_hebbian();
        default:
            ErrorManager::get_instance()->log_error(
                "Unimplemented connection type!");
    }
}

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

