#include "engine/cluster/cluster.h"
#include "engine/cluster/cluster_node.h"
#include "engine/instruction.h"
#include "network/structure.h"
#include "state/state.h"
#include "engine/engine.h"
#include "util/resource_manager.h"

Cluster::Cluster(State *state, Engine *engine, PropertyConfig args)
        : state(state),
          engine(engine) {
    this->multithreaded = args.get_bool("multithreaded", true);

    auto active_devices = state->get_active_devices();
    if (not this->multithreaded and active_devices.size() > 1)
        LOG_ERROR(
            "Attempted to initialize single threaded cluster for state"
            " with multiple active devices!");

    auto res_man = ResourceManager::get_instance();
    for (auto device_id : active_devices)
        io_streams.push_back(
            (multithreaded)
                ? res_man->create_stream(device_id)
                : res_man->get_default_stream(device_id));
}

Cluster::~Cluster() {
    for (auto& node : nodes) delete node;
}

void Cluster::add_external_dependencies(
        std::map<Layer*, ClusterNode*> all_nodes) {
    // Crawl through the nodes and add dependencies for state updates
    // This prevents race conditions from output updates
    // Ensure that the output is not updated until it's been transferred
    for (auto& node : nodes) {
        for (auto& syn_inst : node->get_synapse_activate_instructions()) {
            auto conn = syn_inst->connection;
            all_nodes[conn->from_layer]
                ->get_state_update_instruction()->add_dependency(syn_inst);
            syn_inst->add_dependency(
                all_nodes[conn->from_layer]->get_state_update_instruction());
        }
    }
}

void Cluster::launch_input(Buffer* buffer) {
    for (auto& node : nodes)
        node->activate_input(buffer);
}

void Cluster::launch_output() {
    for (auto& node : nodes)
        node->activate_output();
}

void Cluster::wait_for_input() {
    if (this->multithreaded)
        for (auto& node : nodes)
            node->synchronize_input();
}

void Cluster::wait_for_output() {
    if (this->multithreaded)
        for (auto& node : nodes)
            node->synchronize_output();
}

Cluster *build_cluster(Structure *structure,
        State *state, Engine *engine, PropertyConfig args) {
    if (not state->check_compatibility(structure))
        LOG_ERROR(
            "Error building cluster for " + structure->str() + ":\n"
            "  Cluster compatibility conflict detected!");

    switch (structure->cluster_type) {
        case PARALLEL:
            return new ParallelCluster(structure, state, engine, args);
        case SEQUENTIAL:
            return new SequentialCluster(structure, state, engine, args);
        case FEEDFORWARD:
            return new FeedforwardCluster(structure, state, engine, args);
        default:
            LOG_ERROR(
                "Error building cluster for " + structure->str() + ":\n"
                "  Unrecognized stream cluster type!");
    }
    return nullptr;
}
