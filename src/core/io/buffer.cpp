#include <cstdlib>
#include <cstring>

#include "io/buffer.h"
#include "model/model.h"
#include "model/structure.h"

Buffer *build_buffer(DeviceID device_id, Model *model) {
    return build_buffer(device_id,
        model->get_input_layers(), model->get_output_layers());
}

Buffer *build_buffer(DeviceID device_id,
        LayerList input_layers, LayerList output_layers) {
    if (device_id == ResourceManager::get_instance()->get_host_id())
        return new HostBuffer(input_layers, output_layers);
    else
        return new DeviceBuffer(input_layers, output_layers, device_id);
}

Buffer::Buffer(LayerList input_layers, LayerList output_layers) {
    this->input_layers = input_layers;
    this->output_layers = output_layers;
    for (auto layer : input_layers) dirty_map[layer] = false;

    input_size = output_size = 0;
    for (auto layer : input_layers) input_size += layer->size;
    for (auto layer : output_layers) output_size += layer->size;
}

void Buffer::init() {
    // Set up maps
    int input_index = 0;
    int output_index = 0;
    for (auto& layer : input_layers) {
        input_map[layer] = input.slice(input_index, layer->size);
        input_index += layer->size;
    }
    for (auto& layer : output_layers) {
        output_map[layer] = output.slice(output_index, layer->size);
        output_index += layer->size;
    }
}

Buffer::~Buffer() {
    this->input.free();
    this->output.free();
}

void Buffer::set_input(Layer* layer, Pointer<float> source) {
    source.copy_to(this->get_input(layer));
    dirty_map[layer] = true;
}

void Buffer::set_output(Layer* layer, Pointer<Output> source) {
    source.copy_to(this->get_output(layer));
}

void HostBuffer::init() {
    // Allocate buffer memory
    if (input_size > 0) input = Pointer<float>::pinned_pointer(input_size, 0.0);
    if (output_size > 0) output = Pointer<Output>::pinned_pointer(output_size);
    Buffer::init();
}

void DeviceBuffer::init() {
    // Allocate buffer memory
    if (input_size > 0) {
        input = Pointer<float>(input_size, 0.0);
        input.transfer_to_device(device_id);
    }
    if (output_size > 0) {
        output = Pointer<Output>(output_size);
        output.transfer_to_device(device_id);
    }
    Buffer::init();
}
