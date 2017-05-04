#include <climits>

#include "heatmap.h"
#include "visualizer_window.h"
#include "gui.h"
#include "model/layer.h"
#include "io/environment.h"

int Heatmap::instance_id = -1;

Heatmap *Heatmap::get_instance(bool init) {
    int id = Heatmap::instance_id;
    if ((id == -1 or id >= Frontend::instances.size())) {
        if (init) {
            new Heatmap();
            Heatmap::instance_id = Frontend::instances.size()-1;
        } else {
            Heatmap::instance_id = -1;
            return nullptr;
        }
    }
    return (Heatmap*)Frontend::instances[Heatmap::instance_id];
}

static guint8 convert(Output out, OutputType type) {
    switch (type) {
        case FLOAT:
            return 255 * out.f;
        case INT:
            return 255 * float(out.i) / INT_MAX;
        case BIT:
            return (out.i > INT_MAX) ? 255 : (out.i >> 23);
    }
}

Heatmap::Heatmap() : iterations(0) { }

Heatmap::~Heatmap() {
    for (auto pair : spike_count_map) free(pair.second);
}

bool Heatmap::add_output_layer(Layer *layer, std::string params) {
    LayerInfo* info;
    try {
        info = layer_map.at(layer);
        return false;
    } catch (...) {
        info = new LayerInfo(layer);
        this->add_layer(layer, info);
    }
    info->set_output();

    this->spike_count_map[layer->id] = (float*) calloc(layer->size, sizeof(float));
    return true;
}

void Heatmap::update(Environment *environment) {
    ++iterations;
    int reset_iteration = 2500;
    bool verbose = false;

    // Copy data over
    for (int i = 0; i < visualizer_window->layers.size(); ++i) {
        LayerInfo *info = visualizer_window->layers[i];
        if (info->get_output()) {
            guint8* data = visualizer_window->pixbufs[i]->get_pixels();
            Buffer *buffer = environment->buffer;
            Output *output = buffer->get_output(info->layer);
            OutputType output_type = environment->get_output_type(info->layer);

            float* spike_count = spike_count_map[info->layer->id];
            int max = 0;
            int min = -1;
            float avg = 0;
            int num_spiked = 0;

            if (iterations % reset_iteration == 0)
                for (int j = 0; j < info->layer->size; ++j)
                    spike_count[j] = 0;

            for (int j = 0; j < info->layer->size; ++j) {
                if (output[j].i & (1 << 31)) ++spike_count[j];
                if (spike_count[j] > 0) {
                    if (spike_count[j] > max) max = spike_count[j];
                    if ((min == -1) or (spike_count[j] < min)) min = spike_count[j];
                    ++num_spiked;
                }
                avg += spike_count[j];
            }
            avg /= info->layer->size;

            float std_dev = 0.0;
            for (int j = 0; j < info->layer->size; ++j)
                std_dev += pow((spike_count[j] - avg), 2);
            std_dev = pow((std_dev / info->layer->size), 0.5);

            if (verbose and max != min)
                printf("%-10s -> %-10s: max %6d  min %6d  num_spiked %6d  "
                       "avg %10.4f  std_dev %10.4f\n",
                    info->layer->structure->name.c_str(),
                    info->layer->name.c_str(),
                    max, min, num_spiked, avg, std_dev / avg);

            for (int j = 0; j < info->layer->size; ++j) {
                if (spike_count[j] == 0) {
                    data[j*4 + 0] = 0.0;
                    data[j*4 + 1] = 0.0;
                    data[j*4 + 2] = 0.0;
                } else {
                    float val = (max == min) ? 0.5 : (spike_count[j]-min) / (max-min);
                    //val = 255.0 * val * val;  // nonlinear heatmap
                    val = 255.0 * val;
                    data[j*4 + 0] = val;
                    data[j*4 + 1] = 0.0;
                    data[j*4 + 2] = 255.0 - val;
                }
            }

            if (info->layer->rows == 1)
                for (int j = 1; j < 50; ++j) {
                    int index = j*info->layer->size * 4;
                    for (int k = 0; k < info->layer->size; ++k) {
                        data[index + (k*4 + 0)] = data[k*4 + 0];
                        data[index + (k*4 + 1)] = data[k*4 + 1];
                        data[index + (k*4 + 2)] = data[k*4 + 2];
                    }
                }
        }
    }

    // Signal GUI to update
    this->gui->dispatcher.emit();
}