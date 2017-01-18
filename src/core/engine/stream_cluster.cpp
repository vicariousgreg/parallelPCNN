#include "engine/stream_cluster.h"

StreamCluster::StreamCluster(Model *model, State *state)
        : state(state) {
    // Build instructions
    for (int i = 0; i < model->connections.size(); ++i) {
        Connection *conn = model->connections[i];
        Layer *from_layer = conn->from_layer;
        Layer *to_layer = conn->to_layer;

        // Create instruction
        Instruction *inst = new Instruction(conn, state);

        // Add instruction to appropriate stream
        std::map<Layer*, Stream*>::iterator it =
            streams[to_layer->type].find(to_layer);

        Stream *stream =
            (it != streams[to_layer->type].end())
            ? it->second : new Stream(to_layer);

        stream->add_instruction(inst, from_layer->type);
        streams[to_layer->type][to_layer] = stream;

        // Add to global list
        all_instructions.push_back(inst);
    }

    // Finalize all streams
    // This sets up cuda information
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        for (auto& stream : streams[i]) stream.second->finalize();
}

StreamCluster::~StreamCluster() {
    // Delete streams
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        for (auto& stream : streams[i]) delete stream.second;

    // Delete instructions
    for (auto& inst : this->all_instructions) delete inst;
}

void StreamCluster::disable_learning() {
    for (int i = 0; i < all_instructions.size(); ++i)
        all_instructions[i]->disable_learning();
}

void StreamCluster::schedule_clear_output_calculations() {
#ifdef PARALLEL
    // Ensure all layer streams wait for clear event
    wait_event(OUTPUT, this->state->clear_event);
    wait_event(INTERNAL, this->state->clear_event);
#endif

    // Launch clear output relevant computations
    schedule_to(OUTPUT);
    this->dispatch_activate();
}

void StreamCluster::schedule_input_output_calculations() {
#ifdef PARALLEL
    // Ensure all layer streams wait for input event
    // This function should not be called until this event has been
    //   scheduled to be recorded
    wait_event(INPUT, this->state->input_event);
    wait_event(INPUT_OUTPUT, this->state->input_event);
#endif

    // Launch input output relevant computations
    schedule_from(INPUT_OUTPUT);
    schedule_from(OUTPUT);
    schedule_to(INPUT_OUTPUT);
    this->dispatch_activate();

#ifdef PARALLEL
    block_stream_from(OUTPUT, state->state_stream);
    block_stream_from(INPUT_OUTPUT, state->state_stream);
    block_stream_to(INPUT_OUTPUT, state->state_stream);
    block_stream_to(OUTPUT, state->state_stream);
#endif
}

void StreamCluster::schedule_non_output_calculations() {
    // Launch output relevant computations
    schedule_to(INPUT);
    schedule_to(INTERNAL);
    this->dispatch_activate();

#ifdef PARALLEL
    block_stream_to(INPUT, state->state_stream);
    block_stream_to(INTERNAL, state->state_stream);
#endif
}

void StreamCluster::reset() {
    // Reset streams
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        for (auto& stream : streams[i]) stream.second->reset();
}

void StreamCluster::schedule_from(IOType from_type) {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        for (auto& stream : streams[i])
            stream.second->schedule(from_type, this);
}

void StreamCluster::schedule_to(IOType to_type) {
    for (auto& stream : streams[to_type])
        stream.second->schedule(this);
}

void StreamCluster::schedule_weight_update() {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        for (auto& stream : streams[i])
            stream.second->schedule_plastic(this);
    this->dispatch_update();
}

#ifdef PARALLEL
void StreamCluster::wait_event(IOType to_type, cudaEvent_t *event) {
    for (auto& stream : streams[to_type])
        stream.second->wait_event(event);
}

void StreamCluster::block_stream_to(IOType to_type, cudaStream_t cuda_stream) {
    for (auto& stream : streams[to_type])
        cudaStreamWaitEvent(cuda_stream, *stream.second->finished_event, 0);
}

void StreamCluster::block_stream_from(IOType from_type, cudaStream_t cuda_stream) {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        for (auto& stream : streams[i])
            cudaStreamWaitEvent(cuda_stream, *stream.second->events[from_type], 0);
}
#endif

void StreamCluster::schedule(Instruction *inst) {
    schedules[inst->connection->to_layer].push_back(inst);
}

void StreamCluster::dispatch_activate() {
    bool done = false;
    for (int i = 0; not done; ++i) {
        done = true;
        for (auto& schedule : this->schedules)
            if (i < schedule.second.size()) {
                done = false;
                schedule.second[i]->activate();
            }
    }
    for (auto& schedule : this->schedules)
        schedule.second.clear();
}

void StreamCluster::dispatch_update() {
    bool done = false;
    for (int i = 0; not done; ++i) {
        done = true;
        for (auto& schedule : this->schedules)
            if (i < schedule.second.size()) {
                done = false;
                schedule.second[i]->update();
            }
    }
    for (auto& schedule : this->schedules)
        schedule.second.clear();
}
