#include "state/attributes.h"
#include "state/izhikevich_attributes.h"
#include "state/hebbian_rate_encoding_attributes.h"
#include "state/backprop_rate_encoding_attributes.h"
#include "state/hodgkin_huxley_attributes.h"

Attributes *build_attributes(LayerList &layers,
        NeuralModel neural_model, DeviceID device_id) {
    Attributes *attributes;

    switch(neural_model) {
        case(IZHIKEVICH):
            attributes = new IzhikevichAttributes(layers);
            attributes->object_size = sizeof(IzhikevichAttributes);
            break;
        case(HODGKIN_HUXLEY):
            attributes = new HodgkinHuxleyAttributes(layers);
            attributes->object_size = sizeof(HodgkinHuxleyAttributes);
            break;
        case(HEBBIAN_RATE_ENCODING):
            attributes = new HebbianRateEncodingAttributes(layers);
            attributes->object_size = sizeof(HebbianRateEncodingAttributes);
            break;
        case(BACKPROP_RATE_ENCODING):
            attributes = new BackpropRateEncodingAttributes(layers);
            attributes->object_size = sizeof(BackpropRateEncodingAttributes);
            break;
        default:
            ErrorManager::get_instance()->log_error(
                "Unrecognized engine type!");
    }

    // Copy attributes to device and set the pointer
    attributes->set_device_id(device_id);
    attributes->schedule_transfer();
    return attributes;
}

Attributes::Attributes(LayerList &layers, OutputType output_type,
        Kernel<ATTRIBUTE_ARGS> kernel, Kernel<ATTRIBUTE_ARGS> learning_kernel)
        : output_type(output_type),
          kernel(kernel),
          learning_kernel(learning_kernel),
          device_id(0),
          pointer(this) {
    // Keep track of register sizes
    int input_size = 0;
    int output_size = 0;
    int expected_size = 0;
    int other_size = 0;
    int second_order_size = 0;

    int timesteps_per_output = get_timesteps_per_output(output_type);

    // Determine how many input cells are needed
    //   based on the dendritic trees of the layers
    for (auto& layer : layers) {
        layer_sizes[layer->id] = layer->size;
        int register_count = layer->dendritic_root->get_max_register_index() + 1;

        // Determine max delay for output connections
        int max_delay_registers = 0;
        for (auto& conn : layer->get_output_connections()) {
            int delay_registers = conn->delay / timesteps_per_output;
            if (delay_registers > max_delay_registers)
                max_delay_registers = delay_registers;
        }
        ++max_delay_registers;

        // Set start indices and update size
        input_start_indices[layer->id] = input_size;
        input_size += register_count * layer->size;

        output_start_indices[layer->id] = output_size;
        output_size += max_delay_registers * layer->size;

        if (layer->is_expected()) {
            expected_start_indices[layer->id] = expected_size;
            expected_size += layer->size;
        }

        other_start_indices[layer->id] = other_size;
        other_size += layer->size;

        // Find any second order dendritic nodes
        second_order_size =
            dendrite_DFS(layer->dendritic_root, second_order_size);
    }

    // Set layer indices
    int layer_index = 0;
    for (auto& layer : layers)
        layer_indices[layer->id] = layer_index++;

    // Allocate space for input and output
    this->input = Pointer<float>(input_size, 0.0);
    this->output = Pointer<Output>(output_size);
    this->expected = Pointer<Output>(expected_size);
    this->second_order_input = Pointer<float>(second_order_size, 0.0);
    this->total_neurons = other_size;
    this->total_layers = layers.size();;
}

Attributes::~Attributes() {
    this->input.free();
    this->output.free();
    this->expected.free();
    this->second_order_input.free();
#ifdef __CUDACC__
    cudaFree(this->pointer);
#endif
}

int Attributes::dendrite_DFS(DendriticNode *curr, int second_order_size) {
    auto res_man = ResourceManager::get_instance();

    if (curr->is_second_order()) {
        second_order_indices[curr->id] = second_order_size;
        int size = curr->get_second_order_size();
        second_order_sizes[curr->id] = size;
        second_order_size += size;
    } else {
        // Recurse on internal children
        for (auto& child : curr->get_children())
            if (not child->is_leaf())
                second_order_size =
                    this->dendrite_DFS(child, second_order_size);
    }
    return second_order_size;
}

void Attributes::set_device_id(DeviceID device_id) {
    this->device_id = device_id;

    // Retrieve extractor
    // This has to wait until device_id is set
    get_extractor(&this->extractor, output_type, device_id);
}

void Attributes::transfer_to_device() {
    // Copy attributes to device and set the pointer
    if (not ResourceManager::get_instance()->is_host(device_id))
        this->pointer = (Attributes*)
            ResourceManager::get_instance()->allocate_device(
                1, object_size, this, device_id);
}

void Attributes::schedule_transfer() {
    // Transfer data
    this->input.schedule_transfer(device_id);
    this->output.schedule_transfer(device_id);
    this->expected.schedule_transfer(device_id);
    this->second_order_input.schedule_transfer(device_id);
}

int Attributes::get_other_start_index(int id) const {
    return other_start_indices.at(id);
}

Pointer<float> Attributes::get_input(int id, int register_index) const {
    int size = layer_sizes.at(id);
    return input.slice(input_start_indices.at(id) + (register_index * size), size);
}

Pointer<float> Attributes::get_second_order_input(int id) const {
    int size = second_order_sizes.at(id);
    return second_order_input.slice(second_order_indices.at(id), size);
}

Pointer<Output> Attributes::get_expected(int id) const {
    int size = layer_sizes.at(id);
    return expected.slice(expected_start_indices.at(id), size);
}

Pointer<Output> Attributes::get_output(int id, int word_index) const {
    int size = layer_sizes.at(id);
    return output.slice(output_start_indices.at(id) + (word_index * size), size);
}
