#ifndef izhikevich_attributes_h
#define izhikevich_attributes_h

#include "state/attributes.h"

class IzhikevichAttributes : public Attributes {
    public:
        IzhikevichAttributes(Layer *layer);

        virtual KernelList<SYNAPSE_ARGS> get_activators(Connection *conn);
        virtual KernelList<SYNAPSE_ARGS> get_updaters(Connection *conn);
        virtual void process_weight_matrix(WeightMatrix* matrix);

        /* Neuron Attributes */
        // Conductances for different ion channels
        Pointer<float> ampa_conductance;
        Pointer<float> nmda_conductance;
        Pointer<float> gabaa_conductance;
        Pointer<float> gabab_conductance;

        // Multiplicative factor
        Pointer<float> multiplicative_factor;

        // Reward signal
        Pointer<float> dopamine;

        // Voltage and recovery variables
        Pointer<float> voltage;
        Pointer<float> recovery;

        // Spike trace for learning
        Pointer<float> postsyn_exc_trace;
        Pointer<int> time_since_spike;

        // Neuron parameters
        Pointer<float> as;
        Pointer<float> bs;
        Pointer<float> cs;
        Pointer<float> ds;

    GET_KERNEL_DEF
    ATTRIBUTE_MEMBERS
};

class IzhikevichWeightMatrix : public WeightMatrix {
    public:
        Pointer<float> short_traces;
        Pointer<float> long_traces;
        Pointer<float> presyn_traces;
        Pointer<float> stp_rs;
        Pointer<float> stp_ws;
        Pointer<float> weight_derivatives;
        Pointer<int> time_since_spike;

        // Baseline conductances
        float baseline_conductance;

        // Learning rate
        float learning_rate;

        // Short term plasticity flag
        bool stp_flag;

    WEIGHT_MATRIX_MEMBERS(IzhikevichWeightMatrix);
    virtual void register_variables();
};

#endif
