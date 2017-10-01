#include "network/structure_config.h"
#include "network/layer_config.h"
#include "util/error_manager.h"

StructureConfig::StructureConfig(const PropertyConfig *config)
        : PropertyConfig(config),
          name(config->get("name", "")),
          cluster_type(
              get_cluster_type(config->get("cluster type", "parallel"))) {
    // Check name
    if (not config->has("name"))
        ErrorManager::get_instance()->log_error(
            "Unspecified name for structure!");

    // Add layers
    for (auto layer : config->get_array("layers"))
        this->add_layer_internal(new LayerConfig(layer));
}

StructureConfig::StructureConfig(std::string name, ClusterType cluster_type)
        : name(name), cluster_type(cluster_type) {
    this->set("name", name);
    this->set("cluster type", ClusterTypeStrings.at(cluster_type));
}

void StructureConfig::add_layer_internal(LayerConfig *config) {
    this->layers.push_back(config);
}

StructureConfig* StructureConfig::add_layer(LayerConfig* config) {
    this->add_layer_internal(config);
    this->add_to_array("layers", config);
    return this;
}

StructureConfig* StructureConfig::add_layer(const PropertyConfig* config) {
    return this->add_layer(new LayerConfig(config));
}

const std::vector<LayerConfig*> StructureConfig::get_layers() const
    { return layers; }