#ifndef attributes_h
#define attributes_h

#include <string>

#include "model/layer.h"
#include "state/weight_matrix.h"
#include "engine/kernel/extractor.h"
#include "engine/kernel/synapse_kernel.h"
#include "engine/kernel/attribute_data.h"
#include "engine/kernel/activator_kernel.h"
#include "io/environment.h"
#include "util/constants.h"
#include "util/error_manager.h"

/* Typedef for attribute kernel functions */
typedef void(*ATTRIBUTE_KERNEL)(const AttributeData attribute_data);
#define PREAMBLE_ATTRIBUTES \
    const Attributes *att = attribute_data.attributes; \
    float *inputs = attribute_data.input.get(); \
    Output *outputs = attribute_data.output.get(); \
    int other_start_index = attribute_data.other_start_index; \
    int size = attribute_data.size; \
    int history_size = attribute_data.history_size;

class Attributes {
    public:
        Attributes(LayerList &layers, OutputType output_type,
                   ATTRIBUTE_KERNEL kernel);
        virtual ~Attributes();

        /* Checks whether these attributes are compatible
         *   with the given cluster_type */
        virtual bool check_compatibility(ClusterType cluster_type) { return true; }

        virtual void transfer_to_device();

        /* Learning Rule functions */
        // Activator Kernel
        virtual SYNAPSE_KERNEL get_activator(ConnectionType type) {
            return get_base_activator_kernel(type);
        }

        // Updater Kernel
        virtual SYNAPSE_KERNEL get_updater(ConnectionType type) { return NULL; }

        // Depth of weight matrices
        virtual int get_matrix_depth(Connection *conn) { return 1; }

        // Weight matrix processor
        virtual void process_weight_matrix(WeightMatrix* matrix) { }

        // Layer data retrieval
        int get_other_start_index(int id) const;
        Pointer<float> get_input(int id, int register_index = 0) const;
        Pointer<Output> get_output(int id, int word_index = 0) const;

        // Neuron IO data
        EXTRACTOR extractor;
        const OutputType output_type;
        Pointer<Output> output;
        Pointer<float> input;

        // Pointer to this object
        // If parallel, this will point to the device copy
        Attributes *pointer;

        // Pointer to attribute kernel
        ATTRIBUTE_KERNEL kernel;

    protected:
        // Number of neurons
        int total_neurons;

        std::map<int, int> other_start_indices;
        std::map<int, int> input_start_indices;
        std::map<int, int> output_start_indices;
        std::map<int, int> sizes;
};

Attributes *build_attributes(LayerList &layers, NeuralModel neural_model);

#endif
