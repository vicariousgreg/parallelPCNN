#ifdef __GUI__

#include "io/impl/visualizer_module.h"

#include "visualizer_window.h"

/******************************************************************************/
/***************************** VISUALIZER *************************************/
/******************************************************************************/

REGISTER_MODULE(VisualizerModule, "visualizer");

VisualizerModule::VisualizerModule(LayerList layers, ModuleConfig *config)
        : Module(layers, config),
          window(VisualizerWindow::build_visualizer(config)) {
    for (auto layer : layers) {
        auto layer_config = config->get_layer(layer);

        if (layer_config->get_bool("input", false))
            set_io_type(layer, get_io_type(layer) | INPUT);

        if (layer_config->get_bool("expected", false))
            set_io_type(layer, get_io_type(layer) | EXPECTED);

        if (layer_config->get_bool("output", false))
            set_io_type(layer, get_io_type(layer) | OUTPUT);

        // Use output as default
        if (get_io_type(layer) == 0)
            set_io_type(layer, OUTPUT);
        window->add_layer(layer, get_io_type(layer));
    }
}

void VisualizerModule::feed_input_impl(Buffer *buffer) {
    for (auto layer : layers)
        if (get_io_type(layer) & INPUT)
            window->feed_input(layer, buffer->get_input(layer));
}

void VisualizerModule::report_output_impl(Buffer *buffer) {
    for (auto layer : layers) {
        if (get_io_type(layer) & OUTPUT) {
            Output *output = buffer->get_output(layer);
            OutputType output_type = get_output_type(layer);
            window->report_output(layer, output, output_type);
        }
    }
}

/******************************************************************************/
/******************************* HEATMAP **************************************/
/******************************************************************************/

REGISTER_MODULE(HeatmapModule, "heatmap");

HeatmapModule::HeatmapModule(LayerList layers, ModuleConfig *config)
        : Module(layers, config),
          window(VisualizerWindow::build_heatmap(config)) {
    for (auto layer : layers) {
        auto layer_config = config->get_layer(layer);

        if (layer_config->get_bool("input", false))
            set_io_type(layer, get_io_type(layer) | INPUT);

        if (layer_config->get_bool("expected", false))
            set_io_type(layer, get_io_type(layer) | EXPECTED);

        if (layer_config->get_bool("output", false))
            set_io_type(layer, get_io_type(layer) | OUTPUT);

        // Use output as default
        if (get_io_type(layer) == 0)
            set_io_type(layer, OUTPUT);
        window->add_layer(layer, get_io_type(layer));
    }
}

void HeatmapModule::report_output_impl(Buffer *buffer) {
    for (auto layer : layers) {
        if (get_io_type(layer) & OUTPUT) {
            Output *output = buffer->get_output(layer);
            OutputType output_type = get_output_type(layer);
            window->report_output(layer, output, output_type);
        }
    }
}

void HeatmapModule::cycle_impl() {
    window->cycle();
}

#endif
