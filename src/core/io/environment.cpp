#include "io/environment.h"
#include "builder.h"

Environment* Environment::load(std::string path) {
    return load_environment(path);
}

void Environment::save(std::string path) {
    save_environment(this, path);
}

void Environment::add_module(ModuleConfig* config) {
    config_list.push_back(config);
    for (auto layer_conf : config->get_layers())
        config_map[layer_conf->get("structure")]
                  [layer_conf->get("layer")].push_back(config);
}

void Environment::remove_modules() {
    for (auto c : config_list) delete c;
    config_map.clear();
    config_list.clear();
}

void Environment::remove_modules(std::string structure,
        std::string layer, std::string type) {
    for (auto l : config_map[structure]) {
        if (layer == "" or l.first == layer) {
            // Filter out configs to keep and delete others
            std::vector<ModuleConfig*> filtered_configs;
            for (auto c : l.second)
                if (type == "" or c->get_type() == type)
                    delete c;
                else
                    filtered_configs.push_back(c);

            // Clear old list and replace configs to keep
            l.second.clear();
            for (auto c : filtered_configs)
                l.second.push_back(c);
        }
    }
}
