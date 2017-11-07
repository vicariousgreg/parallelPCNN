#include <string>
#include <math.h>

#include "state/impl/sample_attributes.h"
#include "state/weight_matrix.h"
#include "engine/kernel/synapse_kernel.h"
#include "util/tools.h"

/* These macros register the Attributes and WeightMatrix subclasses, associating
 *   them with the neural model named "sample".
 */
REGISTER_ATTRIBUTES(SampleAttributes, "sample", FLOAT)
REGISTER_WEIGHT_MATRIX(SampleWeightMatrix, "sample")

/******************************************************************************/
/************************** CLASS FUNCTIONS ***********************************/
/******************************************************************************/

/* The constructor is responsible for constructing variable arrays that the
 *   kernels will need.  These variables can be initialized based on layer
 *   or connection properties.
 * The output type (ie BIT for spiking or FLOAT for rate encoding) is passed
 *   to the Attributes superclass constructor.
 */
SampleAttributes::SampleAttributes(LayerList &layers)
        : Attributes(layers, FLOAT) {  // FLOAT indicates the output type

    // Here's where you construct and register variables
    this->layer_variable = create_layer_variable<float>(0.0);
    register_layer_variable("layer_var", &layer_variable);

    this->neuron_variable = create_neuron_variable<float>(0.0);
    register_neuron_variable("neuron_var", &neuron_variable);

    // Here's where you initialize variables
    int start_index = 0;
    for (auto& layer : layers) {
        // Layer Variables
        layer_variable[layer_indices[layer->id]] = 0.0;

        // Neuron Variables
        for (int nid = 0 ; nid < layer->size ; ++nid) {
            neuron_variable[start_index + nid] = 0.0;
        }

        start_index += layer->size;
    }
}

void SampleWeightMatrix::register_variables() {
    this->var1 = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("var1", &var1);

    this->var2 = WeightMatrix::create_variable<float>();
    WeightMatrix::register_variable("var2", &var2);
}

/* WeightMatrices call this function once they're done initializing the first
 *   layer, which is specified by the layer's noise config.  This is where you
 *   initialize the additional layers.  The connection and its layers can be
 *   retrieved from the WeightMatrix.
 */
void SampleAttributes::process_weight_matrix(WeightMatrix* matrix) {
    SampleWeightMatrix *s_mat = (SampleWeightMatrix*)matrix;

    // Retrieve connection and matrix data pointer
    Connection *conn = matrix->connection;
    Pointer<float> mData = matrix->get_weights();
    int num_weights = conn->get_num_weights();

    // Connection variable
    s_mat->x = std::stof(conn->get_parameter("connection variable", "1.0"));

    // Accessing second and third layers
    Pointer<float> second_weights = s_mat->var1;
    Pointer<float> third_weights = s_mat->var2;

    // The first layer is initialized according to the noise config.
    // Sometimes it's useful to differentially initialize
    //   matrix layers based on Connection parameters.
    if (conn->get_parameter("second weights", "false") == "true") {
        float val = std::stof(conn->from_layer->get_parameter("val", "1.0"));
        if (val < 0)
            LOG_ERROR(
                "Error initializing weight matrix for " + conn->str() + " \n"
                "  Val cannot be negative!");

        for (int wid = 0 ; wid < num_weights ; ++wid)
            second_weights[wid] = mData[wid] * val;
    }

    // Or with more basic connection properties.
    switch(conn->opcode) {
        case MODULATE:
            set_weights(third_weights, num_weights, 0.0);
            break;
        case SUB:
            set_weights(third_weights, num_weights, 1.0);
            break;
    }

}

/* This function can be used to enforce specific types of engine cluster.
 * For example, restricting the use of an attributes subclass to only
 *   feedforward engines, to ensure that data propagates all the way through
 *   the network at each timestep */
bool SampleAttributes::check_compatibility(ClusterType cluster_type) {
    return cluster_type == FEEDFORWARD;
}

/******************************************************************************/
/******************************** KERNEL **************************************/
/******************************************************************************/

/* This is the kernel that iterates over neurons to update their states.
 * This macro has four inputs:
 *   - name of class
 *   - name of kernel function
 *   - initialization code
 *   - neuron state update code
 * The initialization code runs at the beginning of the kernel, and is useful
 *   for retrieving pointers from the attributes.  The neuron state update code
 *   specifies the operations that are performed on each neuron.
 * See src/core/engine/kernel/attribute_data.h for a specification of inputs.
 * See src/core/state/attributes.h for macro definition, including the algorithm
 *   skeletons (DEF_ATT_KERNEL) and variables that are automatically extracted
 *   by this macro (PREAMBLE_ATTRIBUTES).
 */
BUILD_ATTRIBUTE_KERNEL(SampleAttributes, sample_attribute_kernel,
    // Cast the attributes pointer to the subclass type
    SampleAttributes *sample_att = (SampleAttributes*)att;

    // Layer index can be used to index into layer variables
    float layer_var = *sample_att->layer_variable.get(layer_index);

    // other_start_index is used to index into neuron variables
    float* neuron_var = sample_att->neuron_variable.get(other_start_index);

    // input and output are automatically retrieved by the macro,
    //   but this casts the Output* to a float* for convenience
    float *f_outputs = (float*)outputs;

    ,

    // If you want to respect delays, this is the algorithm
    // If not, delayed output connections will get garbage data
    float next_value = f_outputs[nid];
    int index;
    for (index = 0 ; index < history_size-1 ; ++index) {
        float curr_value = next_value;
        next_value = f_outputs[size * (index + 1) + nid];
        f_outputs[size * index + nid] = next_value;
    }
    float input = inputs[nid];

    // This is the appropriate index to use for the most recent output
    f_outputs[size * index + nid] = layer_var * inputs[nid];
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
    SampleAttributes *sample_att = (SampleAttributes*)synapse_data.attributes; \
    SampleWeightMatrix *sample_mat = (SampleWeightMatrix*)synapse_data.matrix; \
    float conn_var = sample_mat->x;

/* This macro defines what happens for each neuron before weight iteration.
 * Here is where neuron specific data should be extracted or initialized */
#define NEURON_PRE \
    float sum = 0.0;

/* This macro defines what happens for each weight.  The provided extractor
 *   function will transform the Output values to floats.  The delay is
 *   provided as an argument (for spiking models it extracts the correct bit).
 */
#define WEIGHT_OP \
    Output from_out = outputs[from_index]; \
    float weight = weights[weight_index]; \
    float val = extractor(from_out, delay) * weight; \
    sum += val;

/* This macro defines what happens for each neuron after weight iteration.
 * The provided calc function will use the operation designated by the opcode */
#define NEURON_POST \
    calc(opcode, inputs[to_index], sum);

/* This macro puts it all together.  It takes the name of the function and four
 *   code blocks that correspond to the four macros defined above. */
CALC_ALL(activate_sample,
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
 *   out on each weight using calc() and the assigned opcode.  No sum needs to
 *   be maintained; the second_order_weights are modified directly
 */
#define SECOND_ORDER_WEIGHT_OP \
    Output from_out = outputs[from_index]; \
    float weight = weights[weight_index]; \
    float val = extractor(from_out, delay) * weight; \
    second_order_weights[weight_index] = \
        calc(opcode, second_order_weights[weight_index], val);

CALC_ALL(activate_sample_second_order,
    EXTRACT_UPDATE,

    // No NEURON_PRE since we don't use output neuron data
    ,

    SECOND_ORDER_WEIGHT_OP,

    // No NEURON_POST since we don't use output neuron data
);

/* Convolutional second order connections are an anomaly.  They involve one
 *   iteration over the weight matrix, as opposed to repeated iteration during
 *   typical activation computation (one pass per destination neuron). */
CALC_ONE_TO_ONE(activate_sample_second_order_convolutional,
    EXTRACT_UPDATE,

    NEURON_PRE,

    WEIGHT_OP,

    NEURON_POST
);

/* This function is used to retrieve the appropriate kernel for a connection.
 * This allows different connections to run on different kernels. */
Kernel<SYNAPSE_ARGS> SampleAttributes::get_activator(Connection *conn) {
    bool second_order = conn->second_order;

    if (conn->convolutional and conn->second_order) {
        // Typically convolutional connections just use the convergent kernel
        // Second order convolutional connections are special because they iterate
        //   once over the weights (see above)
        return get_activate_sample_second_order_convolutional();
    }

    // The functions are retrieved using functions named after the argument
    //   passed to CALC_ALL, with prefixes corresponding to the connection types
    switch (conn->type) {
        case FULLY_CONNECTED:
            if (second_order) return get_activate_sample_second_order_fully_connected();
            else              return get_activate_sample_fully_connected();
        case SUBSET:
            if (second_order) return get_activate_sample_second_order_subset();
            else              return get_activate_sample_subset();
        case ONE_TO_ONE:
            if (second_order) return get_activate_sample_second_order_one_to_one();
            else              return get_activate_sample_one_to_one();
        case CONVERGENT:
            return get_activate_sample_convergent();
        case DIVERGENT:
            return get_activate_sample_divergent();
    }

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
    float to_out = extractor(destination_outputs[to_index], 0);

/* Do Oja's rule for Hebbian learning */
#define UPDATE_WEIGHT \
    float from_out = extractor(outputs[from_index], delay); \
    float old_weight = weights[weight_index]; \
    weights[weight_index] = old_weight + \
        /* Use conn_var as a learning rate */ \
        (conn_var * from_out * \
            (to_out - (from_out * old_weight)));

CALC_ALL(update_sample,
    EXTRACT,

    NEURON_PRE_UPDATE,

    UPDATE_WEIGHT,

    // No NEURON_POST
);

/* As before, convolutional kernels are updated differently.
 * This time, convergent and divergent kernels are different.
 * This macro defines a special kernel that iterates over all weights, and then
 *   over all from_neurons associated with that weight in an inner loop. */
CALC_CONVERGENT_CONVOLUTIONAL_BY_WEIGHT(update_sample_convergent_convolutional,
    EXTRACT,

    /* For each weight, initialize a sum to aggregate the input
     * from neurons that use it. */
    float weight_delta = 0.0;
    float old_weight = weights[weight_index];,

    /* Aggregate */
    float from_out = extractor(outputs[from_index], delay); \
    float to_out = extractor(destination_outputs[to_index], 0);
    weight_delta += from_out * (to_out - (from_out * old_weight));,

    /* Update the weight by averaging and using conn_var as learning rate */
    weights[weight_index] = old_weight
        + (conn_var * weight_delta / num_weights);
);
CALC_DIVERGENT_CONVOLUTIONAL_BY_WEIGHT(update_sample_divergent_convolutional,
    EXTRACT,

    /* For each weight, initialize a sum to aggregate the input
     * from neurons that use it. */
    float weight_delta = 0.0;
    float old_weight = weights[weight_index];,

    /* Aggregate */
    float from_out = extractor(outputs[from_index], delay); \
    float to_out = extractor(destination_outputs[to_index], 0);
    weight_delta += from_out * (to_out - (from_out * old_weight));,

    /* Update the weight by averaging and using conn_var as learning rate */
    weights[weight_index] = old_weight
        + (conn_var * weight_delta / num_weights);
);

Kernel<SYNAPSE_ARGS> SampleAttributes::get_updater(Connection *conn) {
    std::map<ConnectionType, Kernel<SYNAPSE_ARGS>> funcs;
    if (conn->second_order)
        LOG_ERROR("Unimplemented connection type!");

    // The functions are retrieved using functions named after the argument
    //   passed to CALC_ALL, with prefixes corresponding to the connection types
    switch (conn->type) {
        case FULLY_CONNECTED:
            return get_update_sample_fully_connected();
        case SUBSET:
            return get_update_sample_subset();
        case ONE_TO_ONE:
            return get_update_sample_one_to_one();
        case CONVERGENT:
            return (conn->convolutional)
                ? get_update_sample_convergent_convolutional()
                : get_update_sample_convergent();
        case DIVERGENT:
            return (conn->convolutional)
                ? get_update_sample_divergent_convolutional()
                : get_update_sample_divergent();
    }

    // Log an error if the connection type is unimplemented
    LOG_ERROR("Unimplemented connection type!");
}
