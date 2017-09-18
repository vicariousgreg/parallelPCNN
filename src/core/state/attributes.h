#ifndef attributes_h
#define attributes_h

#include <map>
#include <set>
#include <vector>

#include "network/layer.h"
#include "state/weight_matrix.h"
#include "engine/kernel/kernel.h"
#include "engine/kernel/synapse_data.h"
#include "engine/kernel/attribute_data.h"
#include "util/constants.h"
#include "util/pointer.h"
#include "util/error_manager.h"

class Attributes;

/* Typedef for attribute kernel functions */
typedef AttributeData ATTRIBUTE_ARGS;
typedef void(*ATTRIBUTE_KERNEL)(ATTRIBUTE_ARGS);

/* Typedef for subclass build method
 * This can't be virtual because it's a class method */
typedef Attributes* (*ATT_BUILD_PTR)(LayerList &layers);

class Attributes {
    public:
        Attributes(LayerList &layers, OutputType output_type);
        virtual ~Attributes();

        /* Assigns the attributes to a device
         * This must be done prior to use */
        void set_device_id(DeviceID device_id);

        /* Checks whether these attributes are compatible
         *   with the given cluster_type */
        virtual bool check_compatibility(ClusterType cluster_type) {
            return true;
        }

        // Schedule and conduct transfer to device
        std::vector<BasePointer*> get_pointers();
        std::map<PointerKey, BasePointer*> get_pointer_map();
        void transfer_to_device();

        /* Learning Rule functions */
        // Activator Kernel
        virtual Kernel<SYNAPSE_ARGS> get_activator(Connection *conn) {
            return get_base_activator_kernel(conn);
        }

        // Updater Kernel
        virtual Kernel<SYNAPSE_ARGS> get_updater(Connection *conn) {
            return Kernel<SYNAPSE_ARGS> ();
        }

        // Depth of weight matrices
        virtual int get_matrix_depth(Connection *conn) { return 1; }

        // Weight matrix processor
        virtual void process_weight_matrix(WeightMatrix* matrix) { }

        // Layer data retrieval
        int get_layer_index(size_t id) const;
        int get_other_start_index(size_t id) const;
        Pointer<float> get_input(size_t id, int register_index = 0) const;
        Pointer<float> get_second_order_weights(size_t id) const;
        Pointer<Output> get_output(size_t id, int word_index = 0) const;
        Pointer<Output> get_expected(size_t id) const;

        // Connection data retrieval
        int get_connection_index(size_t id) const;

        // Neuron IO data
        EXTRACTOR extractor;
        const OutputType output_type;
        Pointer<Output> output;
        Pointer<Output> expected;
        Pointer<float> input;
        Pointer<float> second_order_weights;

        // Pointer to this object
        // If parallel, this will point to the device copy
        Attributes *pointer;

        // Getter for attribute kernels
        virtual Kernel<ATTRIBUTE_ARGS> get_kernel() = 0;
        virtual Kernel<ATTRIBUTE_ARGS> get_learning_kernel() {
            return Kernel<ATTRIBUTE_ARGS>(nullptr, nullptr);
        }

        DeviceID get_device_id() { return device_id; }

        // Get the set of neural model strings
        static const std::set<std::string> get_neural_models();

        // Gets the output type of a layer based on its neural model
        OutputType get_output_type();
        static OutputType get_output_type(std::string neural_model);
        static OutputType get_output_type(Layer *layer);

        static Attributes *build_attributes(LayerList &layers,
            std::string neural_model, DeviceID device_id);

    protected:
        class NeuralModelBank {
            public:
                // Set of neural model implementations
                std::set<std::string> neural_models;
                std::map<std::string, ATT_BUILD_PTR> build_pointers;
                std::map<std::string, int> sizes;
                std::map<std::string, OutputType> output_types;
        };

        // Registers a subclass neural model name with the state
        static int register_neural_model(std::string neural_model,
            int object_size, OutputType output_type, ATT_BUILD_PTR build_ptr);

        // Set of neural model implementations
        static NeuralModelBank* get_neural_model_bank();

        // Gets the name of the neural model
        virtual std::string get_neural_model() = 0;

        // Registers a variable to be handled by the superclass
        void register_neuron_variable(std::string key, BasePointer *pointer);
        void register_connection_variable(std::string key, BasePointer *pointer);
        void register_layer_variable(std::string key, BasePointer *pointer);

        // Traverse the dendritic tree and find second order nodes
        int dendrite_DFS(DendriticNode *curr, int second_order_size);

        // Number of neurons and layers
        int total_neurons;
        int total_layers;

        DeviceID device_id;
        int object_size;

        // Managed pointers
        std::map<std::string, BasePointer*> neuron_variables;
        std::map<std::string, BasePointer*> connection_variables;
        std::map<std::string, BasePointer*> layer_variables;

        std::map<size_t, int> layer_indices;
        std::map<size_t, int> other_start_indices;
        std::map<size_t, int> input_start_indices;
        std::map<size_t, int> output_start_indices;
        std::map<size_t, int> layer_sizes;

        std::map<size_t, int> connection_indices;

        std::map<size_t, int> second_order_indices;
        std::map<size_t, int> second_order_sizes;
};

/* Macros for Attribute subclass Registry */
// Put this one in .cpp
#define REGISTER_ATTRIBUTES(CLASS_NAME, STRING, OUTPUT_TYPE) \
int CLASS_NAME::neural_model_id = \
    Attributes::register_neural_model(STRING, \
        sizeof(CLASS_NAME), OUTPUT_TYPE, CLASS_NAME::build); \
std::string CLASS_NAME::neural_model = STRING; \
\
Attributes *CLASS_NAME::build(LayerList &layers) { \
    return new CLASS_NAME(layers); \
}

// Put this one in .h at bottom of class definition
#define ATTRIBUTE_MEMBERS \
    protected: \
        static Attributes *build(LayerList &layers); \
        static int neural_model_id; \
        static std::string neural_model; \
        virtual std::string get_neural_model() { return neural_model; }

// Put this one in .h if the class implements the attribute kernel
#define GET_KERNEL_DEF \
    public: \
        virtual Kernel<ATTRIBUTE_ARGS> get_kernel(); \

// Put this one in .h if the class implements the attribute learning kernel
#define GET_LEARNING_KERNEL_DEF \
    public: \
        virtual Kernel<ATTRIBUTE_ARGS> get_learning_kernel();



/* Macros for Attribute kernels */
#define PREAMBLE_ATTRIBUTES \
    const Attributes *att = attribute_data.attributes; \
    float *inputs = attribute_data.input.get(); \
    Output *outputs = attribute_data.output.get(); \
    int layer_index = attribute_data.layer_index; \
    int other_start_index = attribute_data.other_start_index; \
    int size = attribute_data.size; \
    int history_size = attribute_data.history_size; \
    bool plastic = attribute_data.plastic;

#ifdef __CUDACC__

// Skeletons -- don't use this directly
#define DEF_ATT_KERNEL(FUNC_NAME, PREAMBLE, BODY) \
GLOBAL void FUNC_NAME##_SERIAL(AttributeData attribute_data) { \
    PREAMBLE_ATTRIBUTES \
    PREAMBLE \
    for (int nid = 0; nid < size; ++nid) { \
        BODY; \
    } \
} \
GLOBAL void FUNC_NAME##_PARALLEL(AttributeData attribute_data) { \
    PREAMBLE_ATTRIBUTES \
    PREAMBLE \
    int nid = blockIdx.x * blockDim.x + threadIdx.x; \
    if (nid < size) { \
        BODY; \
    } \
}

// Use this to set up attributes kernel
#define BUILD_ATTRIBUTE_KERNEL( \
    CLASS_NAME, FUNC_NAME, PREAMBLE, BODY) \
DEF_ATT_KERNEL(FUNC_NAME, PREAMBLE, BODY) \
Kernel<ATTRIBUTE_ARGS> CLASS_NAME::get_kernel() { \
    return Kernel<ATTRIBUTE_ARGS>(FUNC_NAME##_SERIAL, FUNC_NAME##_PARALLEL); \
}

// Use this to set up attributes learning kernel
#define BUILD_ATTRIBUTE_LEARNING_KERNEL( \
    CLASS_NAME, FUNC_NAME, PREAMBLE, BODY) \
DEF_ATT_KERNEL(FUNC_NAME, PREAMBLE, BODY) \
Kernel<ATTRIBUTE_ARGS> CLASS_NAME::get_learning_kernel() { \
    return Kernel<ATTRIBUTE_ARGS>(FUNC_NAME##_SERIAL, FUNC_NAME##_PARALLEL); \
}

#else

// Skeletons -- don't use this directly
#define DEF_ATT_KERNEL(FUNC_NAME, PREAMBLE, BODY) \
GLOBAL void FUNC_NAME##_SERIAL(AttributeData attribute_data) { \
    PREAMBLE_ATTRIBUTES \
    PREAMBLE \
    for (int nid = 0; nid < size; ++nid) { \
        BODY; \
    } \
}

// Use this to set up attributes kernel
#define BUILD_ATTRIBUTE_KERNEL( \
    CLASS_NAME, FUNC_NAME, PREAMBLE, BODY) \
DEF_ATT_KERNEL(FUNC_NAME, PREAMBLE, BODY) \
Kernel<ATTRIBUTE_ARGS> CLASS_NAME::get_kernel() { \
    return Kernel<ATTRIBUTE_ARGS>(FUNC_NAME##_SERIAL); \
}

// Use this to set up attributes learning kernel
#define BUILD_ATTRIBUTE_LEARNING_KERNEL( \
    CLASS_NAME, FUNC_NAME, PREAMBLE, BODY) \
DEF_ATT_KERNEL(FUNC_NAME, PREAMBLE, BODY) \
Kernel<ATTRIBUTE_ARGS> CLASS_NAME::get_learning_kernel() { \
    return Kernel<ATTRIBUTE_ARGS>(FUNC_NAME##_SERIAL); \
}

#endif

#endif
