#ifndef layer_config_h
#define layer_config_h

#include <string>
#include "util/constants.h"

class LayerConfig {
    public:
        LayerConfig(
            std::string name,
            NeuralModel neural_model,
            int rows=0,
            int columns=0,
            std::string params="",
            float noise=0.0)
                : name(name),
                  neural_model(neural_model),
                  rows(rows),
                  columns(columns),
                  params(params),
                  noise(noise) { }

        LayerConfig(
            std::string name,
            NeuralModel neural_model,
            std::string params="",
            float noise=0.0)
                : LayerConfig(name, neural_model, 0, 0, params, noise) { }
        
        std::string name;
        NeuralModel neural_model;
        int rows, columns;
        std::string params;
        float noise;
};

#endif