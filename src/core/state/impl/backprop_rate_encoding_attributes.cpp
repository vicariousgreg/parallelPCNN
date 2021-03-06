#include "state/impl/backprop_rate_encoding_attributes.h"
#include "util/logger.h"

REGISTER_ATTRIBUTES(BackpropRateEncodingAttributes, "backprop_rate_encoding", FLOAT)

KernelList<SYNAPSE_ARGS> BackpropRateEncodingAttributes::get_updater(
        Connection* conn) {
    return { };
}

/******************************************************************************/
/************************** CLASS FUNCTIONS ***********************************/
/******************************************************************************/

BackpropRateEncodingAttributes::BackpropRateEncodingAttributes(Layer *layer)
        : RateEncodingAttributes(layer) {
    this->error_deltas = Attributes::create_neuron_variable<float>(0.0);
    Attributes::register_neuron_variable("error deltas", &error_deltas);
}
