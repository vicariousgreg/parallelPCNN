#include <string>
#include <math.h>

#include "state/impl/izhikevich_attributes.h"
#include "engine/kernel/synapse_kernel.h"
#include "util/tools.h"

REGISTER_ATTRIBUTES(IzhikevichAttributes, "izhikevich", BIT)
REGISTER_WEIGHT_MATRIX(IzhikevichWeightMatrix, "izhikevich")

/******************************************************************************/
/******************************** PARAMS **************************************/
/******************************************************************************/

/* Neuron parameters class.
 * Contains a,b,c,d parameters for Izhikevich model */
class IzhikevichParameters {
    public:
        IzhikevichParameters(float a, float b, float c, float d) :
                a(a), b(b), c(c), d(d) {}
        float a, b, c, d;
};

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

static void create_parameters(std::string str,
        float* as, float* bs, float* cs, float* ds, int size) {
    if (str == "random positive") {
        // (ai; bi) = (0:02; 0:2) and (ci; di) = (-65; 8) + (15;-6)r2
        for (int i = 0 ; i < size ; ++i) {
            as[i] = 0.02;
            // increase for higher frequency oscillations
            bs[i] = 0.2;
            cs[i] = -65.0 + (15.0 * pow(fRand(), 2));
            ds[i] = 8.0 - (6.0 * pow(fRand(), 2));
        }
    } else if (str == "random negative") {
        //(ai; bi) = (0:02; 0:25) + (0:08;-0:05)ri and (ci; di)=(-65; 2).
        for (int i = 0 ; i < size ; ++i) {
            as[i] = 0.02 + (0.08 * fRand());
            bs[i] = 0.25 - (0.05 * fRand());
            cs[i] = -65.0;
            ds[i] = 2.0;
        }
    } else {
        IzhikevichParameters params(0,0,0,0);
        if (str == "default")                 params = DEFAULT;
        else if (str == "regular")            params = REGULAR;
        else if (str == "bursting")           params = BURSTING;
        else if (str == "chattering")         params = CHATTERING;
        else if (str == "fast")               params = FAST;
        else if (str == "low_threshold")      params = LOW_THRESHOLD;
        else if (str == "thalamo_cortical")   params = THALAMO_CORTICAL;
        else if (str == "resonator")          params = RESONATOR;
        else
            LOG_ERROR(
                "Unrecognized parameter string: " + str);


        for (int i = 0 ; i < size ; ++i) {
            as[i] = params.a;
            bs[i] = params.b;
            cs[i] = params.c;
            ds[i] = params.d;
        }
    }
}

/******************************************************************************/
/******************************** KERNEL **************************************/
/******************************************************************************/

/* Voltage threshold for neuron spiking. */
#define IZ_SPIKE_THRESH 30.0

/* Euler resolution for voltage update. */
#define IZ_EULER_RES 10
#define IZ_EULER_RES_INV 0.1

/* Time dynamics of spike traces */
#define STDP_TAU_POS       0.933  // tau = 15ms
#define STDP_TAU_NEG       0.95   // tau = 20ms

/* Time dynamics of dopamine */
#define DOPAMINE_CLEAR_TAU 0.95  // 20ms

/* STDP A constants */
#define STDP_A_POS 0.004
#define STDP_A_NEG 0.004

BUILD_ATTRIBUTE_KERNEL(IzhikevichAttributes, iz_attribute_kernel,
    float *ampa_conductances = att->ampa_conductance.get();
    float *nmda_conductances = att->nmda_conductance.get();
    float *gabaa_conductances = att->gabaa_conductance.get();
    float *gabab_conductances = att->gabab_conductance.get();
    float *multiplicative_factors = att->multiplicative_factor.get();
    float *dopamines = att->dopamine.get();

    float *voltages = att->voltage.get();
    float *recoveries = att->recovery.get();
    float *postsyn_exc_traces = att->postsyn_exc_trace.get();
    int *time_since_spikes = att->time_since_spike.get();
    unsigned int *spikes = (unsigned int*)outputs;
    float *as = att->as.get();
    float *bs = att->bs.get();
    float *cs = att->cs.get();
    float *ds = att->ds.get();

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

    float a = as[nid];
    float b = bs[nid];

    // Euler's method for voltage/recovery update
    // If the voltage exceeds the spiking threshold, break
    for (int i = 0 ; (i < IZ_EULER_RES) and (voltage < IZ_SPIKE_THRESH) ; ++i) {
        // Start with AMPA conductance
        float current = -ampa_conductance * voltage;

        // NMDA nonlinear voltage dependence
        float temp = powf((voltage + 80) / 60, 2);
        current -= nmda_conductance * (temp / (1+temp)) * voltage;

        // GABA conductances
        current -= gabaa_conductance * (voltage + 70);
        current -= gabab_conductance * (voltage + 90);

        // Multiplicative factor for synaptic currents
        current *= 1 + multiplicative_factor;

        // Add the base current after multiplicative factor
        current += base_current;

        // Update voltage
        // For numerical stability, use hybrid numerical method
        //   (see section 5b of "Hybrid Spiking Models" by Izhikevich)
        float delta_v = (0.04 * voltage * voltage) +
                        (5*voltage) + 140 - recovery + current;

        float sum_conductances =
            ampa_conductance + nmda_conductance +
            gabaa_conductance + gabab_conductance;

        voltage =
            (voltage + IZ_EULER_RES_INV *
                (delta_v
                    + (gabaa_conductance * -70)
                    + (gabab_conductance * -90)))
            / (1 + (IZ_EULER_RES_INV * sum_conductances));

        // If the voltage explodes (voltage == NaN -> voltage != voltage),
        //   set it to threshold before it corrupts the recovery variable
        voltage = (voltage != voltage) ? IZ_SPIKE_THRESH : voltage;

        float adjusted_tau = (voltage > IZ_SPIKE_THRESH)
            ? delta_v
                / (IZ_SPIKE_THRESH - voltage + delta_v)
                * IZ_EULER_RES_INV
            : IZ_EULER_RES_INV;

        // Update recovery variable
        recovery += a * adjusted_tau * ((b * voltage) - recovery);
    }

    ampa_conductances[nid] = 0.0;
    nmda_conductances[nid] = 0.0;
    gabaa_conductances[nid] = 0.0;
    gabab_conductances[nid] = 0.0;
    multiplicative_factors[nid] = 0.0;
    dopamines[nid] *= DOPAMINE_CLEAR_TAU;

    /********************
     *** SPIKE UPDATE ***
     ********************/
    // Determine if spike occurred
    unsigned int spike = voltage >= IZ_SPIKE_THRESH;

    SHIFT_BIT_OUTPUTS(spikes, spike);

    // Update trace, voltage, recovery
    postsyn_exc_traces[nid] = (prev_bit)
        ? STDP_A_NEG
        : (postsyn_exc_traces[nid] * STDP_TAU_NEG);
    time_since_spikes[nid] = (prev_bit)
        ? 0
        : MIN(32, time_since_spikes[nid] + 1);
    voltages[nid] = (spike) ? cs[nid] : voltage;
    recoveries[nid] = recovery + (spike * ds[nid]);
)

/******************************************************************************/
/************************* TRACE ACTIVATOR KERNELS ****************************/
/******************************************************************************/

#define AMPA_TAU          0.8    // tau = 5
#define GABAA_TAU         0.833  // tau = 6
#define NMDA_TAU          0.993  // tau = 150
#define GABAB_TAU         0.993  // tau = 150

#define MULT_TAU          0.95   // tau = 20
#define DOPAMINE_TAU      0.95   // tau = 20

// Extraction at start of kernel
#define ACTIV_EXTRACTIONS \
    IzhikevichAttributes *att = \
        (IzhikevichAttributes*)synapse_data.attributes; \
    IzhikevichWeightMatrix *matrix = \
        (IzhikevichWeightMatrix*)synapse_data.matrix; \
    float baseline_conductance = matrix->baseline_conductance; \
    bool stp_flag = matrix->stp_flag; \
\
    float *stp_rs = matrix->stp_rs.get(); \
    float *stp_ws = matrix->stp_ws.get(); \
    int   *delays = matrix->delays.get(); \
    int   *from_time_since_spike = matrix->time_since_spike.get();

#define ACTIV_EXTRACTIONS_SHORT(SHORT_NAME, SHORT_TAU) \
    float *short_conductances = att->SHORT_NAME.get(); \
    float short_tau = SHORT_TAU; \
    float *short_traces   = matrix->short_traces.get();

#define ACTIV_EXTRACTIONS_LONG(LONG_NAME, LONG_TAU) \
    float *long_conductances = att->LONG_NAME.get(); \
    float long_tau = LONG_TAU; \
    float *long_traces   = matrix->long_traces.get();

// Neuron Pre Operation
#define INIT_SUM \
    float short_sum = 0.0; \
    float long_sum = 0.0;

// Weight Operation
#define CALC_VAL_PREAMBLE \
    float spike = extract(outputs[from_index], delays[weight_index]); \
    from_time_since_spike[from_index] = \
        ((spike > 0.0) \
            ? 0 \
            : MIN(32, from_time_since_spike[from_index] + 1)); \
\
    float weight = weights[weight_index] \
        * (stp_rs[weight_index] * stp_ws[weight_index]);

#define CALC_VAL_SHORT \
    float short_trace = short_traces[weight_index] \
        + (spike * weight * baseline_conductance); \
    short_sum += short_trace; \
    short_traces[weight_index] = short_trace * short_tau;

#define CALC_VAL_LONG \
    float long_trace = long_traces[weight_index] \
        + (spike * weight * baseline_conductance); \
    long_sum += long_trace; \
    long_traces[weight_index] = long_trace * long_tau;

// Neuron Post Operation
#define AGGREGATE_SHORT \
    short_conductances[to_index] += short_sum;

#define AGGREGATE_LONG \
    long_conductances[to_index] += long_sum;


/* Trace versions of activator functions */
CALC_ALL(activate_iz_add,
    ACTIV_EXTRACTIONS
    ACTIV_EXTRACTIONS_SHORT(
        ampa_conductance,
        AMPA_TAU)
    ACTIV_EXTRACTIONS_LONG(
        nmda_conductance,
        NMDA_TAU),

    INIT_SUM,

    CALC_VAL_PREAMBLE
    CALC_VAL_SHORT
    CALC_VAL_LONG,

    AGGREGATE_SHORT
    AGGREGATE_LONG
);

CALC_ALL(activate_iz_sub,
    ACTIV_EXTRACTIONS
    ACTIV_EXTRACTIONS_SHORT(
        gabaa_conductance,
        GABAA_TAU)
    ACTIV_EXTRACTIONS_LONG(
        gabab_conductance,
        GABAB_TAU),

    INIT_SUM,

    CALC_VAL_PREAMBLE
    CALC_VAL_SHORT
    CALC_VAL_LONG,

    AGGREGATE_SHORT
    AGGREGATE_LONG
);

CALC_ALL(activate_iz_mult,
    ACTIV_EXTRACTIONS
    ACTIV_EXTRACTIONS_SHORT(
        multiplicative_factor,
        MULT_TAU),

    INIT_SUM,

    CALC_VAL_PREAMBLE
    CALC_VAL_SHORT,

    AGGREGATE_SHORT
);

CALC_ALL(activate_iz_reward,
    ACTIV_EXTRACTIONS
    ACTIV_EXTRACTIONS_SHORT(
        dopamine,
        DOPAMINE_TAU),

    INIT_SUM,

    CALC_VAL_PREAMBLE
    CALC_VAL_SHORT,

    AGGREGATE_SHORT
);

CALC_ALL(activate_iz_gap,
    IzhikevichAttributes *att =
        (IzhikevichAttributes*)synapse_data.attributes;
    float *voltage = att->voltage.get();,

    float v = voltage[to_index];
    float sum = 0.0;,

    float weight = weights[weight_index];
    sum += (voltage[from_index] - v) * weight;,

    inputs[to_index] += sum;
);

KernelList<SYNAPSE_ARGS> IzhikevichAttributes::get_activators(Connection *conn) {
    // Direct connections get a mainline into the input current
    if (conn->get_parameter("direct", "false") == "true")
        return { get_base_activator_kernel(conn) };

    // These are not supported because of the change of weight matrix pointer
    // Second order host connections require their weight matrices to be copied
    // Currently, this only copies the first matrix in the stack, and this
    //   attribute set uses auxiliary data
    if (conn->second_order)
        LOG_ERROR(
            "Unimplemented connection type!");

    try {
        switch (conn->opcode) {
            case(ADD):
                return { activate_iz_add_map.at(conn->get_type()) };
            case(SUB):
                return { activate_iz_sub_map.at(conn->get_type()) };
            case(MULT):
                return { activate_iz_mult_map.at(conn->get_type()) };
            case(REWARD):
                return { activate_iz_reward_map.at(conn->get_type()) };
            case(GAP):
                return { activate_iz_gap_map.at(conn->get_type()) };
        }
    } catch(std::out_of_range) { }

    LOG_ERROR(
        "Unimplemented connection type!");
}

/******************************************************************************/
/************************** TRACE UPDATER KERNELS *****************************/
/******************************************************************************/

#define UPDATE_EXTRACTIONS \
    IzhikevichWeightMatrix *matrix = \
        (IzhikevichWeightMatrix*)synapse_data.matrix; \
    float *presyn_traces   = matrix->presyn_traces.get(); \
    float *stp_rs = matrix->stp_rs.get(); \
    float *stp_ws = matrix->stp_ws.get(); \
    float *weight_derivatives = matrix->weight_derivatives.get(); \
    int   *delays = matrix->delays.get(); \
    int   *from_time_since_spike = matrix->time_since_spike.get(); \
    bool stp_flag = matrix->stp_flag; \
\
    IzhikevichAttributes *att = \
        (IzhikevichAttributes*)synapse_data.attributes; \
    float *to_exc_traces = att->postsyn_exc_trace.get(); \
    int   *to_time_since_spike = att->time_since_spike.get(); \
    float *dopamines = att->dopamine.get(); \
    float learning_rate = matrix->learning_rate; \

#define GET_DEST_ACTIVITY \
    float dest_exc_trace = to_exc_traces[to_index]; \
    int   dest_time_since_spike = to_time_since_spike[to_index]; \
    float dopamine = dopamines[to_index]; \
    float dest_spike = extract(destination_outputs[to_index], 0);

/* Minimum weight */
#define MIN_WEIGHT 0.0001f

/* Short term plasticity */
#define U_ADD 0.5
#define F_ADD 1000
#define D_ADD 800

#define U_SUB 0.2
#define F_SUB 20
#define D_SUB 700

/* Plasticity dynamics */
#define C_TAU 0.0001
#define C_A   0.000001

#define STDP_ADD \
    /* Update presynaptic trace */ \
    float src_trace = \
        (presyn_traces[weight_index] = \
            (src_spike) \
                ? STDP_A_POS \
                : presyn_traces[weight_index] * STDP_TAU_POS); \
\
    float dest_trace = dest_exc_trace;

#define STDP_SUB \
    /* Update presynaptic trace */ \
    float src_trace =  \
        ((dest_spike > 0.0 and src_time_since_spike > 0 and src_time_since_spike < 32) \
            /* positive iSTDP function of delta T */ \
            ? powf(src_time_since_spike, 10) \
              * 0.000001 \
              * 2.29 \
              * expf(-1.1 * src_time_since_spike) \
            : 0.0); \
\
    float dest_trace = \
        ((src_spike and dest_time_since_spike > 0 and dest_time_since_spike < 32) \
            /* negative iSTDP function of delta T */ \
            ? powf(dest_time_since_spike, 10) \
              * 0.0000001 \
              * 2.6 \
              * expf(-0.94 * dest_time_since_spike) \
            : 0.0);

#define UPDATE_WEIGHT(STDP, U, D, F) \
    float weight = weights[weight_index]; \
\
    if (weight >= MIN_WEIGHT) { \
        /* Extract postsynaptic trace */ \
        int src_time_since_spike = from_time_since_spike[from_index]; \
        bool src_spike = src_time_since_spike == 0; \
\
        /* Depends on ADD/SUB */ \
        STDP; \
\
        /* Compute delta from short term dynamics */ \
        float weight_delta = weight_derivatives[weight_index]; \
        weight_delta = \
            weight_derivatives[weight_index] -= \
                (weight_delta - C_A) * C_TAU + \
                    (dest_spike * src_trace) \
                    - (src_spike * dest_trace); \
\
        /* Calculate new weight */ \
        /* Reward (dopamine) should factor in here */ \
        weight += learning_rate * weight_delta; \
\
        /* Ensure weight stays within boundaries */ \
        weights[weight_index] = MAX(MIN_WEIGHT, MIN(max_weight, weight)); \
\
        /* Update short term plasticity if applicable */ \
        if (stp_flag) { \
            float r = stp_rs[weight_index]; \
            float w = stp_ws[weight_index]; \
\
            stp_rs[weight_index] += ((1 - r) / D) - (r * w * src_spike); \
            stp_ws[weight_index] += ((U - w) / F) + (U * (1 - w) * src_spike); \
        } \
    }

CALC_ALL(update_iz_add,
    UPDATE_EXTRACTIONS,
    GET_DEST_ACTIVITY,
    UPDATE_WEIGHT(STDP_ADD, U_ADD, D_ADD, F_ADD),
; );

CALC_ALL(update_iz_sub,
    UPDATE_EXTRACTIONS,
    GET_DEST_ACTIVITY,
    UPDATE_WEIGHT(STDP_SUB, U_SUB, D_SUB, F_SUB),
; );

KernelList<SYNAPSE_ARGS> IzhikevichAttributes::get_updaters(Connection *conn) {
    // Second order, convolutional, and direct updaters are not supported
    if (conn->second_order or conn->convolutional)
        LOG_ERROR(
            "Unimplemented connection type!");

    try {
        switch (conn->opcode) {
            case(ADD):
                return { update_iz_add_map.at(conn->get_type()) };
            case(SUB):
                return { update_iz_sub_map.at(conn->get_type()) };
        }
    } catch(std::out_of_range) { }

    LOG_ERROR(
        "Unimplemented connection type!");
}

/******************************************************************************/
/************************** CLASS FUNCTIONS ***********************************/
/******************************************************************************/

static void check_parameters(Layer *layer) {
    std::set<std::string> valid_params;
    valid_params.insert("name");
    valid_params.insert("neural model");
    valid_params.insert("rows");
    valid_params.insert("columns");

    valid_params.insert("params");
    valid_params.insert("neuron spacing");

    for (auto pair : layer->get_config()->get())
        if (valid_params.count(pair.first) == 0)
            LOG_WARNING(
                "Unrecognized layer parameter: " + pair.first);
}

static void check_parameters(Connection *conn) {
    std::set<std::string> valid_params;
    valid_params.insert("from layer");
    valid_params.insert("from structure");
    valid_params.insert("to layer");
    valid_params.insert("to structure");
    valid_params.insert("max");
    valid_params.insert("plastic");
    valid_params.insert("opcode");
    valid_params.insert("type");

    valid_params.insert("conductance");
    valid_params.insert("learning rate");
    valid_params.insert("myelinated");
    valid_params.insert("random delay");
    valid_params.insert("cap delay");
    valid_params.insert("x offset");
    valid_params.insert("y offset");
    valid_params.insert("short term plasticity");

    for (auto pair : conn->get_config()->get())
        if (valid_params.count(pair.first) == 0)
            LOG_WARNING(
                "Unrecognized connection parameter: " + pair.first);
}

IzhikevichAttributes::IzhikevichAttributes(Layer *layer)
        : Attributes(layer, BIT) {
    // Conductances
    this->ampa_conductance = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("ampa", &ampa_conductance);
    this->nmda_conductance = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("nmda", &nmda_conductance);
    this->gabaa_conductance = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("gabaa", &gabaa_conductance);
    this->gabab_conductance = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("gabab", &gabab_conductance);
    this->multiplicative_factor = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("mult", &multiplicative_factor);
    this->dopamine = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("dopamine", &dopamine);

    // Neuron variables
    this->voltage = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("voltage", &voltage);
    this->recovery = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("recovery", &recovery);
    this->postsyn_exc_trace = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("post trace", &postsyn_exc_trace);
    this->time_since_spike = Attributes::create_neuron_variable<int>();
    Attributes::register_neuron_variable("time since spike", &time_since_spike);

    // Neuron parameters
    this->as = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("a", &as);
    this->bs = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("b", &bs);
    this->cs = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("c", &cs);
    this->ds = Attributes::create_neuron_variable<float>();
    Attributes::register_neuron_variable("d", &ds);

    // Check layer parameters
    check_parameters(layer);

    create_parameters(layer->get_parameter("params", "regular"),
        this->as, this->bs, this->cs, this->ds, layer->size);
    for (int j = 0 ; j < layer->size ; ++j) {
        postsyn_exc_trace[j] = 0.0;
        time_since_spike[j] = 32;

        // Run simulation to stable point
        float v = this->cs[j];
        float r = this->bs[j] * this->cs[j];
        float delta_v;
        float delta_r;
        float a = this->as[j];
        float b = this->bs[j];
        do {
            delta_v = (0.04 * v * v) + (5*v) + 140 - r;
            v += delta_v;

            delta_r = a * ((b * v) - r);
            r += delta_r;
        } while (abs(delta_v) > 0.001 and abs(delta_r) > 0.001);

        voltage[j] = v;
        recovery[j] = r;
    }

    // Connection properties
    for (auto& conn : layer->get_input_connections()) {
        // Check connection parameters
        check_parameters(conn);

        // Ensure gap junctions are self-connections
        if (conn->get_type() == GAP and conn->from_layer != conn->to_layer)
            LOG_ERROR(
                "Error " + conn->str() + "\n"
                "Gap junctions must be between neurons of the same layer.");
    }
}

void IzhikevichWeightMatrix::register_variables() {
    // Short term (AMPA/GABAA) conductance trace
    this->short_traces = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("short trace", &short_traces);

    // Long term (NMDA/GABAA) conductance trace
    this->long_traces = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("long trace", &long_traces);

    // Presynaptic trace
    this->presyn_traces = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("presyn trace", &presyn_traces);

    // Short Term Plasticity
    this->stp_rs = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("short term R", &stp_rs);
    this->stp_ws = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("short term w", &stp_ws);

    // Weight derivatives
    this->weight_derivatives = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("weight derivatives", &weight_derivatives);

    // Time since presynaptic spike
    this->time_since_spike = WeightMatrix::create_variable<int>();
    WeightMatrix::register_variable("time since spike", &time_since_spike);
}

void IzhikevichAttributes::process_weight_matrix(WeightMatrix* matrix) {
    auto iz_mat = (IzhikevichWeightMatrix*)matrix;
    Connection *conn = matrix->connection;

    // Retrieve baseline conductance
    iz_mat->baseline_conductance =
        std::stof(conn->get_parameter("conductance", "1.0"));

    // Retrieve learning rate
    iz_mat->learning_rate =
        std::stof(conn->get_parameter("learning rate", "0.1"));

    // Retrieve short term plasticity flag
    iz_mat->stp_flag =
        conn->get_parameter("short term plasticity", "true") == "true";

    int num_weights = conn->get_num_weights();
    Pointer<float> mData = matrix->get_weights();

    // Short term trace
    fClear(iz_mat->short_traces, num_weights);

    // Long term trace
    fClear(iz_mat->long_traces, num_weights);

    // Plasticity trace
    fClear(iz_mat->presyn_traces, num_weights);

    // Short Term Plasticity
    fSet(iz_mat->stp_rs, num_weights, 1.0);

    if (conn->get_type() == ADD)
        fSet(iz_mat->stp_ws, num_weights, U_ADD);
    else if (conn->get_type() == SUB)
        fSet(iz_mat->stp_ws, num_weights, U_SUB);
    else
        fClear(iz_mat->stp_ws, num_weights);

    fSet(iz_mat->weight_derivatives, num_weights, C_A);

    // Delays
    // Myelinated connections use the base delay only
    if (conn->get_parameter("myelinated", "false") == "true") {
        iz_mat->get_delays(conn->delay);
    } else if (conn->get_config()->has("random delay")) {
        int max_delay = std::stoi(
            conn->get_parameter("random delay", "0"));
        if (max_delay > 31)
            LOG_ERROR(
                "Randomized axons cannot have delays greater than 31!");
        iRand(iz_mat->delays, num_weights, 0, max_delay);
    } else {
        iz_mat->get_delays(BIT,
            std::stof(conn->from_layer->get_parameter("neuron spacing", "0.1")),
            std::stof(conn->to_layer->get_parameter("neuron spacing", "0.1")),
            std::stof(conn->get_parameter("x offset", "0.0")),
            std::stof(conn->get_parameter("y offset", "0.0")),
            0.15,
            conn->get_parameter("cap delay", "false") == "true");
    }

    // Time since last spike
    int *time_since_spike = iz_mat->time_since_spike.get();
    for (int i = 0 ; i < num_weights; ++i)
        time_since_spike[i] = 32;
}
