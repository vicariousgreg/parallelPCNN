#include <string>
#include <math.h>

#include "state/impl/vp_max_attributes.h"
#include "state/weight_matrix.h"
#include "engine/kernel/synapse_kernel.h"
#include "util/tools.h"

REGISTER_ATTRIBUTES(VPMaxAttributes, "vp_max", FLOAT)
REGISTER_WEIGHT_MATRIX(VPMaxWeightMatrix, "vp_max")

/******************************************************************************/
/************************** CLASS FUNCTIONS ***********************************/
/******************************************************************************/

VPMaxAttributes::VPMaxAttributes(Layer *layer)
        : Attributes(layer, FLOAT) { }

void VPMaxWeightMatrix::register_variables() { }

void VPMaxAttributes::process_weight_matrix(WeightMatrix* matrix) {
    VPMaxWeightMatrix *s_mat = (VPMaxWeightMatrix*)matrix;

    // Retrieve connection and matrix data pointer
    Connection *conn = matrix->connection;
    Pointer<float> mData = matrix->get_weights();
    int num_weights = conn->get_num_weights();
}

bool VPMaxAttributes::check_compatibility(ClusterType cluster_type) {
    return true;
}

/******************************************************************************/
/******************************** KERNEL **************************************/
/******************************************************************************/

BUILD_ATTRIBUTE_KERNEL(VPMaxAttributes, vp_max_kernel,
    // input and output are automatically retrieved by the macro,
    //   but this casts the Output* to a float* for convenience
    float *f_outputs = (float*)outputs;

    ,

    SHIFT_FLOAT_OUTPUTS(f_outputs, inputs[nid]); // by default no nonlinearity
)

/******************************************************************************/
/************************* SAMPLE ACTIVATOR KERNELS ***************************/
/******************************************************************************/

/* This is where the activator kernel is specified, which is run over all the
 *   weights in a connection and aggregates them to the destination neurons.
 * The attributes object is available here for variable retrieval.
 *
 * It is convenient to define each piece of the process using a macro.
 * This one defines variable extraction, as in the attributes kernel above. */
#define EXTRACT \
    VPMaxAttributes *vp_max_att = (VPMaxAttributes*)synapse_data.attributes; \
    VPMaxWeightMatrix *vp_max_mat = (VPMaxWeightMatrix*)synapse_data.matrix;

/* This macro defines what happens for each neuron before weight iteration.
 * Here is where neuron specific data should be extracted or initialized */
#define NEURON_PRE \
    float max_val = 0.0;

/* This macro defines what happens for each weight.  The provided extractor
 *   function will transform the Output values to floats.  The delay is
 *   provided as an argument (for spiking models it extracts the correct bit).
 */
#define WEIGHT_OP \
    Output from_out = outputs[from_index]; \
    float weight = weights[weight_index]; \
    float val = extract(from_out, delay) * weight; \
    if(val > max_val) max_val = val;

/* This macro defines what happens for each neuron after weight iteration.
 * The provided calc function will use the operation designated by the opcode */
#define NEURON_POST \
    inputs[to_index] = aggregate(inputs[to_index], max_val);

/* This macro puts it all together.  It takes the name of the function and four
 *   code blocks that correspond to the four macros defined above. */
CALC_ALL(activate_vp_max,
    EXTRACT,

    NEURON_PRE,

    WEIGHT_OP,

    NEURON_POST
);

/* Second order connections involve operations between weights in different
 *   weight matrices, such as multiplicative weights used for gating. They must
 *   be specified separately because they modify an auxiliary matrix rather than
 *   the inputs to the to_layer.
 *
 * This macro extracts the second order auxiliary weight matrix.
 */
#define EXTRACT_UPDATE \
    EXTRACT \
    float * const second_order_weights = synapse_data.matrix->second_order_weights.get();

/* This macro shows updating of second order weights.  The operation is carried
 *   out on each weight using aggregate() and the assigned opcode.  No sum
 *   needs to be maintained; the second_order_weights are modified directly.
 */
#define SECOND_ORDER_WEIGHT_OP \
    Output from_out = outputs[from_index]; \
    float weight = weights[weight_index]; \
    float val = extract(from_out, delay) * weight; \
    second_order_weights[weight_index] = \
        aggregate(second_order_weights[weight_index], val);

CALC_ALL(activate_vp_max_second_order,
    EXTRACT_UPDATE,

    // No NEURON_PRE since we don't use output neuron data
    ,

    SECOND_ORDER_WEIGHT_OP,

    // No NEURON_POST since we don't use output neuron data
);

/* Convolutional second order connections are an anomaly.  They involve one
 *   iteration over the weight matrix, as opposed to repeated iteration during
 *   typical activation computation (one pass per destination neuron). */
CALC_ONE_TO_ONE(activate_vp_max_second_order_convolutional,
    EXTRACT_UPDATE,

    NEURON_PRE,

    WEIGHT_OP,

    NEURON_POST
);

/* This function is used to retrieve the appropriate kernel for a connection.
 * This allows different connections to run on different kernels. */
KernelList<SYNAPSE_ARGS> VPMaxAttributes::get_activators(Connection *conn) {
    bool second_order = conn->second_order;

    if (conn->convolutional and conn->second_order) {
        // Typically convolutional connections just use the convergent kernel
        // Second order convolutional connections are special because they iterate
        //   once over the weights (see above)
        return { get_activate_vp_max_second_order_convolutional() };
    }

    try {
        if (conn->second_order)
            return { activate_vp_max_second_order_map.at(conn->get_type()) };
        else
            return { activate_vp_max_map.at(conn->get_type()) };
    } catch(std::out_of_range) { }

    // Log an error if the connection type is unimplemented
    LOG_ERROR("Unimplemented connection type!");
}

/******************************************************************************/
/************************** SAMPLE UPDATER KERNELS ****************************/
/******************************************************************************/

/* This is where the updater kernel is specified, which is run over all the
 *   weights in a connection to update their values.
 * This is where the learning rule is implemented.
 */

/* Retrieve output of the destination layer, not the input layer */
#define NEURON_PRE_UPDATE \
    float to_out = extract(destination_outputs[to_index], 0);

/* Do Oja's rule for Hebbian learning */
#define UPDATE_WEIGHT \
    float from_out = extract(outputs[from_index], delay); \
    float old_weight = weights[weight_index]; \
    weights[weight_index] = old_weight + \
        (from_out * (to_out - (from_out * old_weight)));

CALC_ALL(update_vp_max,
    EXTRACT,

    NEURON_PRE_UPDATE,

    UPDATE_WEIGHT,

    // No NEURON_POST
);

/* As before, convolutional kernels are updated differently.
 * This time, convergent and divergent kernels are different.
 * This macro defines a special kernel that iterates over all weights, and then
 *   over all from_neurons associated with that weight in an inner loop. */
CALC_CONVERGENT_CONVOLUTIONAL_BY_WEIGHT(update_vp_max_convergent_convolutional,
    EXTRACT,

    /* For each weight, initialize a sum to aggregate the input
     * from neurons that use it. */
    float weight_delta = 0.0;
    float old_weight = weights[weight_index];,

    /* Aggregate */
    float from_out = extract(outputs[from_index], delay); \
    float to_out = extract(destination_outputs[to_index], 0);
    weight_delta += from_out * (to_out - (from_out * old_weight));,

    /* Update the weight by averaging */
    weights[weight_index] = old_weight
        + (weight_delta / num_weights);
);
CALC_DIVERGENT_CONVOLUTIONAL_BY_WEIGHT(update_vp_max_divergent_convolutional,
    EXTRACT,

    /* For each weight, initialize a sum to aggregate the input
     * from neurons that use it. */
    float weight_delta = 0.0;
    float old_weight = weights[weight_index];,

    /* Aggregate */
    float from_out = extract(outputs[from_index], delay); \
    float to_out = extract(destination_outputs[to_index], 0);
    weight_delta += from_out * (to_out - (from_out * old_weight));,

    /* Update the weight by averaging */
    weights[weight_index] = old_weight
        + (weight_delta / num_weights);
);

KernelList<SYNAPSE_ARGS> VPMaxAttributes::get_updaters(Connection *conn) {
    if (conn->second_order)
        LOG_ERROR("Unimplemented connection type!");

    // The functions are retrieved using functions and a map created by CALC_ALL
    try {
        if (conn->convolutional) {
            if (conn->get_type() == CONVERGENT)
                return { get_update_vp_max_convergent_convolutional() };
            else if (conn->get_type() == DIVERGENT)
                return { get_update_vp_max_divergent_convolutional() };
        }
        return { update_vp_max_map.at(conn->get_type()) };
    } catch(std::out_of_range) { }

    // Log an error if the connection type is unimplemented
    LOG_ERROR("Unimplemented connection type!");
}
