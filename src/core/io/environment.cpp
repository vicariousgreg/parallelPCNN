#include "io/environment.h"
#include "builder.h"

Environment* Environment::load(std::string path) {
    return load_environment(path);
}

void Environment::save(std::string path) {
    save_environment(this, path);
}

void Environment::add_module(PropertyConfig* config) {
    add_to_child_array("modules", config);
}

void Environment::remove_modules() {
    if (has_child_array("modules"))
        for (auto config : remove_child_array("modules"))
            delete config;
}

void Environment::remove_modules(std::string structure,
        std::string layer, std::string type) {
    for (auto config : get_child_array("modules")) {
        if (config->get("structure", "") == structure and
            (layer == "" or config->get("layer", "") == layer) and
            (type == "" or config->get("type", "") == type))
            delete remove_from_child_array("modules", config);
    }
}
