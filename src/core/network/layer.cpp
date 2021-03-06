#include <queue>

#include "network/layer.h"
#include "network/structure.h"
#include "network/connection.h"
#include "util/logger.h"

/* Copy constructor is meant for synapse_data, which can be sent to devices.
 * This will render pointers invalid, so they are set to nullptr when copied.
 * In addition, this ensures there are no double frees of pointers. */
Layer::Layer(const Layer& other)
    : config(nullptr),
      name(other.name),
      id(other.id),
      neural_model(other.neural_model),
      structure(nullptr),
      rows(other.rows),
      columns(other.columns),
      size(other.size),
      plastic(other.plastic),
      is_ghost(other.is_ghost),
      dendritic_root(nullptr) { }

Layer::Layer(Structure *structure, const LayerConfig *config)
        : config(config),
          name(config->name),
          id(std::hash<std::string>()(structure->name + "/" + name)),
          neural_model(config->neural_model),
          structure(structure),
          rows(config->rows),
          columns(config->columns),
          size(rows * columns),
          plastic(config->plastic),
          is_ghost(config->get_bool("ghost", false)),
          dendritic_root(new DendriticNode(this)) {
    add_dendrites("root", config->get_child_array("dendrites"));
}

Layer::~Layer() {
    if (dendritic_root != nullptr)
        delete dendritic_root;
    if (config != nullptr)
        delete config;
}

bool Layer::is_structure_input() const {
    for (auto conn : get_input_connections())
        if (conn->from_layer->structure == this->structure
                and not conn->recurrent)
            return false;
    return true;
}

bool Layer::is_structure_output() const {
    for (auto conn : get_output_connections())
        if (conn->to_layer->structure == this->structure
                and not conn->recurrent)
            return false;
    return true;
}

DendriticNodeList Layer::get_dendritic_nodes() const {
    DendriticNodeList nodes;
    std::queue<DendriticNode*> q;
    q.push(this->dendritic_root);

    // Breadth first search
    while (not q.empty()) {
        auto curr = q.front();
        q.pop();
        nodes.push_back(curr);
        for (auto child : curr->get_children())
            q.push(child);
    }

    return nodes;
}

DendriticNode* Layer::get_dendritic_node(std::string name,
        bool log_error) const {
    for (auto node: get_dendritic_nodes())
        if (node->name == name) return node;

    if (log_error)
        LOG_ERROR(
            "Error in " + this->str() + ":\n"
            "  Cannot find Dendritic Node " + name + "!");

    return nullptr;
}

std::string Layer::get_parameter(std::string key, std::string def_val) const {
    if (not this->get_config()->has(key))
        LOG_WARNING(
            "Error in " + this->str() + ":\n"
            "  Unspecified parameter: " + key
            + " -- using " + def_val + ".");
    return this->get_config()->get(key, def_val);
}

int Layer::get_max_delay() const {
    int max_delay = 0;
    for (auto& conn : get_output_connections())
        if (conn->delay > max_delay)
            max_delay = conn->delay;
    return max_delay;
}

int Layer::get_num_weights() const {
    int num_weights = 0;
    for (auto& conn : get_input_connections())
        num_weights += conn->get_num_weights();
    return num_weights;
}

int Layer::get_num_compute_weights() const {
    int num_weights = 0;
    for (auto& conn : get_input_connections())
        num_weights += conn->get_compute_weights();
    return num_weights;
}

void Layer::add_input_connection(Connection* connection) {
    this->input_connections.push_back(connection);
}

void Layer::add_output_connection(Connection* connection) {
    this->output_connections.push_back(connection);
}

std::string Layer::str() const {
    return "[Layer: " + name + " (" + structure->name + ")]";
}

void Layer::add_dendrites(std::string parent_name,
        const ConfigArray& dendrites) {
    auto parent = get_dendritic_node(parent_name);
    for (auto dendrite : dendrites) {
        if (not dendrite->has("name"))
            LOG_ERROR(
                "Attempted to dendrite without name to layer!");

        bool second_order = dendrite->get_bool("second order", false);
        auto child = parent->add_child(
            dendrite->get("name"),
            get_opcode(dendrite->get("opcode", "add")),
            dendrite->get_bool("second order", false),
            dendrite->get_float("init", 0.0));

        add_dendrites(child->name,
            dendrite->get_child_array("children"));
    }
}

int get_num_connections(const LayerList& layers) {
    int num_connections = 0;
    for (auto& layer : layers)
        num_connections += layer->get_input_connections().size();
    return num_connections;
}

bool check_equal_sizes(const LayerList& layers) {
    if (layers.size() == 0) return true;
    int size = layers.at(0)->size;
    for (auto& layer : layers)
        if (layer->size != size) return false;
    return true;
}
