#include <string>
#include <math.h>

#include "state/izhikevich_attributes.h"
#include "engine/kernel/synapse_kernel.h"
#include "util/tools.h"

REGISTER_ATTRIBUTES(IzhikevichAttributes, "izhikevich")

/******************************************************************************/
/******************************** PARAMS **************************************/
/******************************************************************************/

#define DEF_PARAM(name, a,b,c,d) \
    static const IzhikevichParameters name = IzhikevichParameters(a,b,c,d);

/* Izhikevich Parameters Table */
DEF_PARAM(DEFAULT          , 0.02, 0.2 , -70.0, 2   ); // Default
DEF_PARAM(REGULAR          , 0.02, 0.2 , -65.0, 8   ); // Regular Spiking
DEF_PARAM(BURSTING         , 0.02, 0.2 , -55.0, 4   ); // Intrinsically Bursting
DEF_PARAM(CHATTERING       , 0.02, 0.2 , -50.0, 2   ); // Chattering
DEF_PARAM(FAST             , 0.1 , 0.2 , -65.0, 2   ); // Fast Spiking
DEF_PARAM(LOW_THRESHOLD    , 0.02, 0.25, -65.0, 2   ); // Low Threshold
DEF_PARAM(THALAMO_CORTICAL , 0.02, 0.25, -65.0, 0.05); // Thalamo-cortical
DEF_PARAM(RESONATOR        , 0.1 , 0.26, -65.0, 2   ); // Resonator

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
    else
        ErrorManager::get_instance()->log_error(
            "Unrecognized parameter string: " + str);
}

/******************************************************************************/
/******************************** KERNEL **************************************/
/******************************************************************************/

/* Voltage threshold for neuron spiking. */
#define IZ_SPIKE_THRESH 30

/* Euler resolution for voltage update. */
#define IZ_EULER_RES 10

/* Milliseconds per timestep */
#define IZ_TIMESTEP_MS 1

#define TRACE_TAU 0.833  // 6ms

BUILD_ATTRIBUTE_KERNEL(IzhikevichAttributes, iz_attribute_kernel,
    IzhikevichAttributes *iz_att = (IzhikevichAttributes*)att;

    float *ampa_conductances = iz_att->ampa_conductance.get(other_start_index);
    float *nmda_conductances = iz_att->nmda_conductance.get(other_start_index);
    float *gabaa_conductances = iz_att->gabaa_conductance.get(other_start_index);
    float *gabab_conductances = iz_att->gabab_conductance.get(other_start_index);
    float *multiplicative_factors = iz_att->multiplicative_factor.get(other_start_index);

    float *voltages = iz_att->voltage.get(other_start_index);
    float *recoveries = iz_att->recovery.get(other_start_index);
    float *neuron_traces = iz_att->neuron_trace.get(other_start_index);
    int *delta_ts = iz_att->delta_t.get(other_start_index);
    unsigned int *spikes = (unsigned int*)outputs;
    IzhikevichParameters *params = iz_att->neuron_parameters.get(other_start_index);

    ,

    /**********************
     *** VOLTAGE UPDATE ***
     **********************/
    float ampa_conductance = ampa_conductances[nid];
    float nmda_conductance = nmda_conductances[nid];
    float gabaa_conductance = gabaa_conductances[nid];
    float gabab_conductance = gabab_conductances[nid];
    float multiplicative_factor = multiplicative_factors[nid];

    float voltage = voltages[nid];
    float recovery = recoveries[nid];
    float base_current = inputs[nid];

    float a = params[nid].a;
    float b = params[nid].b;

    // Euler's method for voltage/recovery update
    // If the voltage exceeds the spiking threshold, break
    for (int i = 0 ; i < IZ_TIMESTEP_MS * IZ_EULER_RES && voltage < IZ_SPIKE_THRESH ; ++i) {
        float current = -base_current * (voltage + 35);
        current -= ampa_conductance * voltage;
        float temp = powf((voltage + 80) / 60, 2);
        current -= nmda_conductance * (temp / (1+temp)) * voltage;
        current -= gabaa_conductance * (voltage + 70);
        current -= gabab_conductance * (voltage + 90);
        current *= (1+multiplicative_factor);

        float delta_v = (0.04 * voltage * voltage) +
                        (5*voltage) + 140 - recovery + current;
        voltage += delta_v / IZ_EULER_RES;
        recovery += a * ((b * voltage) - recovery) / IZ_EULER_RES;
    }

    ampa_conductances[nid] = 0.0;
    nmda_conductances[nid] = 0.0;
    gabaa_conductances[nid] = 0.0;
    gabab_conductances[nid] = 0.0;
    multiplicative_factors[nid] = 0.0;

    /********************
     *** SPIKE UPDATE ***
     ********************/
    // Determine if spike occurred
    unsigned int spike = voltage >= IZ_SPIKE_THRESH;

    // Reduce reads, chain values.
    unsigned int next_value = spikes[nid];

    // Shift all the bits.
    // Check if next word is odd (1 for LSB).
    int index;
    for (index = 0 ; index < history_size-1 ; ++index) {
        unsigned int curr_value = next_value;
        next_value = spikes[size * (index + 1) + nid];

        // Shift bits, carry over LSB from next value.
        spikes[size*index + nid] = (curr_value >> 1) | (next_value << 31);
    }

    // Least significant value already loaded into next_value.
    // Index moved appropriately from loop.
    spikes[size*index + nid] = (next_value >> 1) | (spike << 31);

    // Update trace, voltage, recovery
    neuron_traces[nid] = (spike)
        ? 1.0 : (neuron_traces[nid] * TRACE_TAU);
    delta_ts[nid] = (spike) ? 0 : (delta_ts[nid] + 1);
    voltages[nid] = (spike) ? params[nid].c : voltage;
    recoveries[nid] = recovery + ((spike) ? params[nid].d : 0.0);
)

/******************************************************************************/
/************************* TRACE ACTIVATOR KERNELS ****************************/
/******************************************************************************/

#define AMPA_TAU        0.8    // tau = 5
#define GABAA_TAU       0.857  // tau = 7
#define NMDA_TAU        0.993  // tau = 150
#define GABAB_TAU       0.993  // tau = 150
#define PLASTIC_TAU     0.444  // tau = 1.8

// Extraction at start of kernel
#define ACTIV_EXTRACTIONS \
    IzhikevichAttributes *att = \
        (IzhikevichAttributes*)synapse_data.to_attributes; \
    int to_start_index = synapse_data.to_start_index; \
    int connection_index = synapse_data.connection_index; \
    float *short_conductances = nullptr; \
    float *long_conductances = nullptr; \
\
    float baseline_short_conductance = \
        att->baseline_conductance.get()[connection_index]; \
    float baseline_long_conductance = baseline_short_conductance * 0.01; \
\
    float stp_p = att->stp_p.get()[connection_index]; \
    float stp_tau = att->stp_tau.get()[connection_index]; \
\
    float short_tau = 0.9; \
    float long_tau = 0.9; \
    switch(opcode) { \
        case(ADD): \
            short_conductances = att->ampa_conductance.get(to_start_index); \
            long_conductances  = att->nmda_conductance.get(to_start_index); \
            short_tau   = AMPA_TAU; \
            long_tau    = NMDA_TAU; \
            break; \
        case(SUB): \
            short_conductances = att->gabaa_conductance.get(to_start_index); \
            long_conductances  = att->gabab_conductance.get(to_start_index); \
            short_tau   = GABAA_TAU; \
            long_tau    = GABAB_TAU; \
            break; \
        case(MULT): \
            short_conductances = att->multiplicative_factor.get(to_start_index); \
            break; \
    } \
    float *short_traces = weights + (1*num_weights); \
    float *long_traces  = weights + (2*num_weights); \
    float *pl_traces    = weights + (3*num_weights); \
    float *stps         = weights + (4*num_weights);

// Neuron Pre Operation
#define INIT_SUM \
    float short_sum = 0.0; \
    float long_sum = 0.0;

// Weight Operation
#define CALC_VAL \
    bool spike = extractor(outputs[from_index], delay) > 0.0; \
\
    if (opcode == ADD or opcode == SUB) { \
        float stp = stps[weight_index]; \
        float weight = weights[weight_index] * stp; \
\
        float trace = short_traces[weight_index]; \
        short_traces[weight_index] = trace = (spike \
            ? baseline_short_conductance : (trace * short_tau)); \
        short_sum += trace * weight; \
\
        trace = long_traces[weight_index]; \
        long_traces[weight_index] = trace = (spike \
            ? baseline_long_conductance : (trace * long_tau)); \
        long_sum += trace * weight; \
    \
        stp *= (spike) ? stp_p : 1; \
        stps[weight_index] = stp + ((1.0 - stp) * stp_tau); \
        if (plastic) { \
            trace = pl_traces[weight_index]; \
            pl_traces[weight_index] = (spike \
                ? (trace + 1.0) : (trace * PLASTIC_TAU)); \
        } \
    } else { \
        short_sum += spike * weights[weight_index]; \
    }

// Neuron Post Operation
#define AGGREGATE \
    short_conductances[to_index] += short_sum; \
    if (long_conductances != nullptr) long_conductances[to_index] += long_sum;


/* Trace versions of activator functions */
CALC_ALL(activate_iz,
    ACTIV_EXTRACTIONS,

    INIT_SUM,

    CALC_VAL,

    AGGREGATE
);

Kernel<SYNAPSE_ARGS> IzhikevichAttributes::get_activator(
        ConnectionType type, bool second_order) {
    if (second_order)
        ErrorManager::get_instance()->log_error(
            "Unimplemented connection type!");

    switch (type) {
        case FULLY_CONNECTED:
            return get_activate_iz_fully_connected();
        case ONE_TO_ONE:
            return get_activate_iz_one_to_one();
        case CONVERGENT:
            return get_activate_iz_convergent();
        case DIVERGENT:
            return get_activate_iz_divergent();
        default:
            ErrorManager::get_instance()->log_error(
                "Unimplemented connection type!");
    }
}

/******************************************************************************/
/************************** TRACE UPDATER KERNELS *****************************/
/******************************************************************************/

// Ratio of positive to negative weight changes for excitatory connections
#define NEG_RATIO 0.5

#define UPDATE_EXTRACTIONS \
    float *pl_traces   = weights + (3*num_weights); \
\
    IzhikevichAttributes *att = \
        (IzhikevichAttributes*)synapse_data.to_attributes; \
    float *to_traces = att->neuron_trace.get(synapse_data.to_start_index); \
    float learning_rate = \
        att->learning_rate.get()[synapse_data.connection_index];

#define GET_DEST_ACTIVITY \
    float dest_trace = to_traces[to_index]; \
    float to_power = 0.0; \
    bool dest_spike = extractor(destination_outputs[to_index], 0);

#define UPDATE_WEIGHT \
    bool src_spike = extractor(outputs[from_index], 0); \
    float src_trace = pl_traces[weight_index]; \
    float weight = weights[weight_index]; \
    switch(opcode) { \
        case(ADD): { \
            float delta = (dest_spike) \
                ? (/* (max_weight - weight) * */ src_trace * learning_rate) : 0.0; \
            delta -= (src_spike) /* use ratio negative delta */ \
                ? (weight * dest_trace * learning_rate * NEG_RATIO) : 0.0; \
            weights[weight_index] = weight + delta; \
            } \
            break; \
    } \

CALC_ALL(update_iz,
    UPDATE_EXTRACTIONS,
    GET_DEST_ACTIVITY,
    UPDATE_WEIGHT,
    ; );

Kernel<SYNAPSE_ARGS> IzhikevichAttributes::get_updater(
        ConnectionType conn_type, bool second_order) {
    if (second_order)
        ErrorManager::get_instance()->log_error(
            "Unimplemented connection type!");

    switch (conn_type) {
        case FULLY_CONNECTED:
            return get_update_iz_fully_connected();
        case ONE_TO_ONE:
            return get_update_iz_one_to_one();
        case CONVERGENT:
            return get_update_iz_convergent();
        case DIVERGENT:
            return get_update_iz_divergent();
        default:
            ErrorManager::get_instance()->log_error(
                "Unimplemented connection type!");
    }
}

/******************************************************************************/
/************************** CLASS FUNCTIONS ***********************************/
/******************************************************************************/

static std::string extract_parameter(Layer *layer,
        std::string key, std::string default_val) {
    try {
        return layer->get_config()->get_property(key);
    } catch (...) {
        ErrorManager::get_instance()->log_warning(
            "Unspecified parameter: " + key + " for layer \""
            + layer->name + "\" -- using " + default_val + ".");
        return default_val;
    }
}

static float extract_parameter(Connection *conn,
        std::string key, float default_val) {
    try {
        return std::stof(conn->get_config()->get_property(key));
    } catch (...) {
        ErrorManager::get_instance()->log_warning(
            "Unspecified parameter: " + key + " for conn \""
            + conn->from_layer->name + "\" -> \""
            + conn->to_layer->name + "\" -- using "
            + std::to_string(default_val) + ".");
        return default_val;
    }
}

IzhikevichAttributes::IzhikevichAttributes(LayerList &layers)
        : Attributes(layers, BIT) {
    // Count connections
    int num_connections = 0;
    for (auto& layer : layers)
        num_connections += layer->get_input_connections().size();

    // Baseline conductances
    this->baseline_conductance = Pointer<float>(num_connections);
    Attributes::register_variable(&this->baseline_conductance);

    // Learning rate
    this->learning_rate = Pointer<float>(num_connections);
    Attributes::register_variable(&this->learning_rate);

    // STP variables
    this->stp_p = Pointer<float>(num_connections);
    this->stp_tau = Pointer<float>(num_connections);
    Attributes::register_variable(&this->stp_p);
    Attributes::register_variable(&this->stp_tau);

    // Conductances
    this->ampa_conductance = Pointer<float>(total_neurons);
    this->nmda_conductance = Pointer<float>(total_neurons);
    this->gabaa_conductance = Pointer<float>(total_neurons);
    this->gabab_conductance = Pointer<float>(total_neurons);
    this->multiplicative_factor = Pointer<float>(total_neurons);
    Attributes::register_variable(&this->ampa_conductance);
    Attributes::register_variable(&this->nmda_conductance);
    Attributes::register_variable(&this->gabaa_conductance);
    Attributes::register_variable(&this->gabab_conductance);
    Attributes::register_variable(&this->multiplicative_factor);

    // Neuron variables
    this->voltage = Pointer<float>(total_neurons);
    this->recovery = Pointer<float>(total_neurons);
    this->neuron_trace = Pointer<float>(total_neurons);
    this->delta_t = Pointer<int>(total_neurons);
    Attributes::register_variable(&this->recovery);
    Attributes::register_variable(&this->voltage);
    Attributes::register_variable(&this->neuron_trace);
    Attributes::register_variable(&this->delta_t);

    // Neuron parameters
    this->neuron_parameters = Pointer<IzhikevichParameters>(total_neurons);
    Attributes::register_variable(&this->neuron_parameters);

    // Fill in table
    int start_index = 0;
    for (auto& layer : layers) {
        IzhikevichParameters params = create_parameters(
            extract_parameter(layer, "init", "regular"));
        for (int j = 0 ; j < layer->size ; ++j) {
            neuron_parameters[start_index+j] = params;
            voltage[start_index+j] = params.c;
            recovery[start_index+j] = params.b * params.c;
            neuron_trace[start_index+j] = 0.0;
            delta_t[start_index+j] = 0;
        }
        start_index += layer->size;

        // Connection properties
        for (auto& conn : layer->get_input_connections()) {
            // Retrieve baseline conductance
            baseline_conductance[connection_indices[conn->id]] =
                extract_parameter(conn, "conductance", 0.01);

            // Retrieve learning rate and STP parameters if plastic
            if (conn->plastic) {
                learning_rate[connection_indices[conn->id]] =
                    extract_parameter(conn, "learning rate", 0.1);
                stp_p[connection_indices[conn->id]] =
                    extract_parameter(conn, "stp p", 1.0);
                stp_tau[connection_indices[conn->id]] =
                    extract_parameter(conn, "stp tau", 0.01);
            }
        }
    }
}

void IzhikevichAttributes::process_weight_matrix(WeightMatrix* matrix) {
    Connection *conn = matrix->connection;
    Pointer<float> mData = matrix->get_data();

    int num_weights = conn->get_num_weights();

    // Clear traces
    int matrix_depth = this->get_matrix_depth(conn);
    for (int i = 1 ; i < matrix_depth - 1 ; ++i) {
        clear_weights(mData + i*num_weights, num_weights);
    }

    // Set STP
    set_weights(mData + (matrix_depth-1)*num_weights, num_weights, 1.0);
}
