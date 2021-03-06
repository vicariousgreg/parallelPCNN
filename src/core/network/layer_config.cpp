#include "network/layer_config.h"
#include "network/layer.h"
#include "util/logger.h"

#define cimg_display 0
#include "CImg.h"

#define GET_IMAGE(name) \
cimg_library::CImg<unsigned char>(name.c_str())

LayerConfig::LayerConfig(const PropertyConfig *config)
        : PropertyConfig(config),
          name(config->get("name", "")),
          neural_model(config->get("neural model", "")),
          rows((config->has("image"))
              ? GET_IMAGE(config->get("image")).height()
              : config->get_int("rows", 0)),
          columns((config->has("image"))
              ? GET_IMAGE(config->get("image")).width()
              : config->get_int("columns", 0)),
          plastic(config->get_bool("plastic", false)) {
    if (not config->has("name"))
        LOG_ERROR(
            "Attempted to construct LayerConfig without name!");
    if (not config->has("neural model"))
        LOG_ERROR(
            "Attempted to construct LayerConfig without neural model!");

    if (rows <= 0 or columns <= 0)
        LOG_ERROR(
            "Invalid dimensions for " + this->str() +
            "[" + std::to_string(rows) + "," + std::to_string(columns) + "]!");
}

LayerConfig::LayerConfig(
        std::string name,
        std::string neural_model,
        int rows,
        int columns,
        PropertyConfig* init_config,
        bool plastic)
            : name(name),
              neural_model(neural_model),
              rows(rows),
              columns(columns),
              plastic(plastic) {
    if (init_config != nullptr)
        this->set_child("init config", init_config);
    this->set("name", name);
    this->set("neural model", neural_model);
    this->set("rows", std::to_string(rows));
    this->set("columns", std::to_string(columns));
    if (init_config != nullptr)
        this->set_child("init config", init_config);
    this->set("plastic", (plastic) ? "true" : "false");
}

LayerConfig::LayerConfig(
    std::string name,
    std::string neural_model,
    std::string image,
    PropertyConfig* init_config,
    bool plastic)
        : LayerConfig(
            name,
            neural_model,
            GET_IMAGE(image).height(),
            GET_IMAGE(image).width(),
            init_config,
            plastic) { }

static PropertyConfig* get_dendrite(const ConfigArray& arr, std::string name) {
    for (auto node : arr)
        if (node->get("name") == name)
            return node;
        else {
            auto props = get_dendrite(node->get_child_array("children"), name);
            if (props != nullptr) return props;
        }
    return nullptr;
}

LayerConfig* LayerConfig::add_dendrite(std::string parent,
        PropertyConfig *config) {
    if (parent == "root") {
        this->add_to_child_array("dendrites", config);
    } else {
        auto node = get_dendrite(this->get_child_array("dendrites"), parent);
        if (node == nullptr)
            LOG_ERROR(
                "Could not find dendrite " + parent
                + "in layer " + name + "!");
        node->add_to_child_array("children", config);
    }
    return this;
}

LayerConfig* LayerConfig::add_dendrite(std::string parent,
            std::string name, bool second_order, float init_val) {
    auto props = new PropertyConfig();
    props->set("name", name);
    props->set("second order", second_order ? "true" : "false");
    props->set("init val", init_val);
    this->add_dendrite(parent, props);
    delete props;
    return this;
}
