#ifndef backprop_rate_encoding_attributes_h
#define backprop_rate_encoding_attributes_h

#include "state/impl/rate_encoding_attributes.h"

class BackpropRateEncodingAttributes : public RateEncodingAttributes {
    public:
        BackpropRateEncodingAttributes(Layer *layer);

        virtual bool check_compatibility(ClusterType cluster_type) {
            return cluster_type == FEEDFORWARD;
        }

        virtual KernelList<SYNAPSE_ARGS> get_updater(Connection* conn);

        Pointer<float> error_deltas;

    ATTRIBUTE_MEMBERS
};

#endif
