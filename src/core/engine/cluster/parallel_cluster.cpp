#include <queue>

#include "engine/cluster/cluster.h"

ParallelCluster::ParallelCluster(Structure *structure,
        State *state, Environment *environment)
        : Cluster(state, environment) {
    // Build instructions
    for (auto& layer : structure->get_layers()) {
        auto device_id = state->get_device_id(layer);
        auto node = new ClusterNode(
            layer, state, environment, io_streams[device_id],
            ResourceManager::get_instance()->create_stream(device_id));
        nodes.push_back(node);
    }

    /* Schedule instructions */
    // Find all instructions with INPUT flag
    pre_input_instructions = sort_instructions(0, INPUT);
    // Find all instructions without INPUT flag
    post_input_instructions = sort_instructions(INPUT, 0);
}

void ParallelCluster::add_external_dependencies(
        std::map<Layer*, ClusterNode*> all_nodes) {
    // Crawl through the nodes and add dependencies for state updates
    // This prevents race conditions from output updates
    // Ensure that the output is not updated until it's been transferred
    // This is the opposite order of the sequential cluster
    for (auto& node : nodes)
        for (auto& pair : node->get_synapse_instructions())
            all_nodes[pair.first->from_layer]
                ->get_state_instruction()->add_dependency(pair.second);
}

InstructionList ParallelCluster::sort_instructions(
        IOTypeMask include, IOTypeMask exclude) {
    std::map<Layer*, std::queue<Instruction*> > schedules;
    InstructionList destination;

    // Extract instructions
    for (auto& node : nodes)
        if ((include == 0 or (node->to_layer->get_type() & include)) and
                not (node->to_layer->get_type() & exclude))
            for (auto& inst : node->get_instructions())
                // Add to schedule map for round robin
                schedules[node->to_layer].push(inst);

    // Perform round robin on nodes
    // Connections should be initialized this way to take advantage of
    //   stream overlap
    while (schedules.size() > 0) {
        for (auto it = schedules.begin(); it != schedules.end(); ) {
            destination.push_back(it->second.front());
            it->second.pop();
            if (it->second.size() == 0) it = schedules.erase(it);
            else ++it;
        }
    }
    return destination;
}

/******************************************************************************/
/****************************** LAUNCHERS *************************************/
/******************************************************************************/

void ParallelCluster::launch_pre_input_calculations() {
    for (auto& inst : this->pre_input_instructions) inst->activate();
}

void ParallelCluster::launch_post_input_calculations() {
    for (auto& inst : this->post_input_instructions) inst->activate();
}

void ParallelCluster::launch_state_update() {
    for (auto& node : nodes)
        node->activate_state();
}

void ParallelCluster::launch_weight_update() {
    for (auto& node : nodes)
        for (auto& inst : node->get_instructions())
            if (inst->is_plastic()) inst->update();
}
