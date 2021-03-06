#include "engine/instruction.h"

InitializeInstruction* get_initialize_instruction(
        Layer *layer, State *state, Stream *stream, bool is_input) {
    auto init_config = layer->get_config()->get_child("init config", nullptr);
    if (init_config != nullptr) {
        auto type = init_config->get("type", "normal");
        if (type == "flat")
            return
                new SetInstruction(layer, state, stream, not is_input);
        else if (type == "uniform")
            return
                new UniformNoiseInstruction(layer, state, stream, not is_input);
        else if (type == "normal")
            return
                new NormalNoiseInstruction(layer, state, stream, not is_input);
        else if (type == "poisson")
            return
                new PoissonNoiseInstruction(layer, state, stream, not is_input);
        else
            LOG_ERROR(
                "Error building cluster node for " + layer->str() + ":\n"
                "  Unrecognized init type: " + type);
    } else if (not is_input) {
        // Clear instruction (set 0.0, overwrite=true)
        return new SetInstruction(layer, state, stream, 0.0, true);
    } else {
        return nullptr;
    }
}

