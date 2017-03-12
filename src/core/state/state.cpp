#include <cstring>

#include "state/state.h"
#include "util/tools.h"

State::State(Model *model)
        : model(model) {

    // Determine number of non-host devices
    this->num_devices = ResourceManager::get_instance()->get_num_devices();

    // Distribute layers
    // TODO: add algorithm for distributing
    for (auto layer : model->get_layers())
        layer_devices[layer] = 0;

    // Create a buffer for each
    for (DeviceID i = 0; i < num_devices; ++i) {
        LayerList input_layers, output_layers;

        // Extract layers assigned to this device
        for (auto layer : model->get_input_layers()) {
            if (layer_devices[layer] == i)
                input_layers.push_back(layer);
        }
        for (auto layer : model->get_output_layers()) {
            if (layer_devices[layer] == i)
                output_layers.push_back(layer);
        }

        // Identify any inter-device connections and make room for them
        for (auto layer : model->get_layers())
            if (layer_devices[layer] == i)
                for (auto conn : layer->get_input_connections())
                    if (is_inter_device(conn))
                        output_layers.push_back(conn->from_layer);
        buffers.push_back(build_buffer(i, input_layers, output_layers));
    }

    // Create attributes and weight matrices
    for (DeviceID device_id = 0 ; device_id < num_devices ; ++device_id) {
        attributes.push_back(std::vector<Attributes*>());
        for (auto neural_model : NeuralModels) {
            LayerList layers;
            for (auto layer : model->get_layers(neural_model))
                if (layer_devices[layer] == device_id)
                    layers.push_back(layer);

            if (layers.size() == 0) {
                attributes[device_id].push_back(nullptr);
            } else {
                auto att = build_attributes(layers, neural_model, device_id);
                attributes[device_id].push_back(att);

                /* Set up weight matrices */
                for (auto& layer : layers) {
                    for (auto& conn : layer->get_input_connections()) {
                        WeightMatrix* matrix = new WeightMatrix(conn,
                            att->get_matrix_depth(conn));
                        this->weight_matrices[conn] = matrix;
                        att->process_weight_matrix(matrix);
                        matrix->transfer_to_device(att->get_device_id());
                    }
                }
            }
        }
    }
}

State::~State() {
    for (int i = 0; i < num_devices; ++i)
        for (auto neural_model : NeuralModels)
            if (attributes[i][neural_model] != nullptr) delete attributes[i][neural_model];
    for (auto matrix : weight_matrices) delete matrix.second;
    for (auto buffer : buffers) delete buffer;
}

bool State::check_compatibility(Structure *structure) {
    // Retrieve represented neural models in the structure
    auto flags = structure->get_neural_model_flags();

    // Check relevant attributes for compatibility
    for (auto n : NeuralModels)
        for (int i = 0; i < num_devices; ++i)
            if (flags[n] and attributes[i][n] and not
                    attributes[i][n]->check_compatibility(structure->cluster_type))
                return false;
    return true;
}

Pointer<float> State::get_input(Layer *layer, int register_index) const {
    return attributes[layer_devices.at(layer)][layer->neural_model]
        ->get_input(layer->id, register_index);
}

Pointer<Output> State::get_output(Layer *layer, int word_index) const {
    return attributes[layer_devices.at(layer)][layer->neural_model]
        ->get_output(layer->id, word_index);
}

Pointer<float> State::get_buffer_input(Layer *layer) const {
    return buffers.at(layer_devices.at(layer))->get_input(layer);
}

Pointer<Output> State::get_buffer_output(Layer *layer) const {
    return buffers.at(layer_devices.at(layer))->get_output(layer);
}

OutputType State::get_output_type(Layer *layer) const {
    return attributes[layer_devices.at(layer)][layer->neural_model]->output_type;
}

DeviceID State::get_device_id(Layer *layer) const {
    return layer_devices.at(layer);
}

int State::get_other_start_index(Layer *layer) const {
    return attributes[layer_devices.at(layer)][layer->neural_model]
        ->get_other_start_index(layer->id);
}

const Attributes* State::get_attributes_pointer(Layer *layer) const {
    return attributes[layer_devices.at(layer)][layer->neural_model]->pointer;
}

Kernel<ATTRIBUTE_ARGS> const State::get_attribute_kernel(Layer *layer) const {
    return attributes[layer_devices.at(layer)][layer->neural_model]->kernel;
}

Pointer<float> State::get_matrix(Connection* conn) const {
    return weight_matrices.at(conn)->get_data();
}

EXTRACTOR State::get_extractor(Connection *conn) const {
    return attributes[layer_devices.at(conn->from_layer)]
                     [conn->from_layer->neural_model]->extractor;
}

Kernel<SYNAPSE_ARGS>State::get_activator(Connection *conn) const {
    return attributes[layer_devices.at(conn->to_layer)]
                     [conn->to_layer->neural_model]->get_activator(conn->type);
}

Kernel<SYNAPSE_ARGS>State::get_updater(Connection *conn) const {
    return attributes[layer_devices.at(conn->to_layer)]
                     [conn->to_layer->neural_model]->get_updater(conn->type);
}

Pointer<Output> State::get_device_output_buffer(Connection *conn) const {
    return buffers[layer_devices.at(conn->to_layer)]
        ->get_output(conn->from_layer);
}

bool State::is_inter_device(Connection *conn) const {
    DeviceID from_device = layer_devices.at(conn->from_layer);
    DeviceID to_device = layer_devices.at(conn->to_layer);
    return from_device != to_device;
}
