#include "driver/driver.h"
#include "driver/scheduler.h"
#include "state/izhikevich_attributes.h"
#include "state/rate_encoding_attributes.h"

Driver::Driver(Model *model, State *state) : state(state) {
    OutputType output_type = state->attributes->get_output_type();
    // Build instructions
    int timesteps_per_output = get_timesteps_per_output(output_type);
    for (int i = 0; i < model->connections.size(); ++i) {
        Connection *conn = model->connections[i];
        Layer *from_layer = conn->from_layer;
        Layer *to_layer = conn->to_layer;

        // Set up word index
        int word_index = HISTORY_SIZE - 1 -
            (conn->delay / timesteps_per_output);
        if (word_index < 0) throw "Invalid delay in connection!";

        // Create instruction
        Output* out = this->state->attributes->get_output(word_index);
        Instruction *inst =
            new Instruction(conn,
                out,
                this->state->attributes->get_output_type(),
                this->state->attributes->get_input(),
                this->state->get_matrix(conn->id));
        this->all_instructions.push_back(inst);

        // Stream cluster
        stream_clusters[to_layer->type].add_instruction(to_layer, inst, from_layer->type);
    }

#ifdef PARALLEL
    // Build streams and events
    cudaStreamCreate(&this->io_stream);
    cudaStreamCreate(&this->kernel_stream);

    input_event = new cudaEvent_t;
    clear_event = new cudaEvent_t;
    output_calc_event = new cudaEvent_t;
    output_event = new cudaEvent_t;

    unsigned int flags = cudaEventDisableTiming;
    unsigned int io_flags = flags & cudaEventBlockingSync;
    cudaEventCreateWithFlags(input_event, io_flags);
    cudaEventCreateWithFlags(clear_event, flags);
    cudaEventCreateWithFlags(output_calc_event, flags);
    cudaEventCreateWithFlags(output_event, io_flags);
#endif
}

Driver::~Driver() {
    delete this->state;
    for (int i = 0; i < this->all_instructions.size(); ++i)
        delete this->all_instructions[i];
#ifdef PARALLEL
    cudaEventDestroy(*input_event);
    cudaEventDestroy(*clear_event);
    cudaEventDestroy(*output_calc_event);
    cudaEventDestroy(*output_event);
    delete input_event;
    delete clear_event;
    delete output_calc_event;
    delete output_event;
#endif
}

#ifdef PARALLEL
void Driver::wait_event(IOType to_type, cudaEvent_t *event) {
    stream_clusters[to_type].wait_event(event);
}
#endif

void Driver::schedule_from(IOType from_type) {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        stream_clusters[i].schedule_execution(from_type);
}

void Driver::schedule_to(IOType to_type) {
    stream_clusters[to_type].schedule_execution();
}

void Driver::stage_clear() {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        stream_clusters[i].reset();

#ifdef PARALLEL
    // Create events
    unsigned int flags = cudaEventDisableTiming;
    unsigned int io_flags = flags & cudaEventBlockingSync;
    cudaEventCreateWithFlags(input_event, io_flags);
    cudaEventCreateWithFlags(clear_event, flags);
    cudaEventCreateWithFlags(output_calc_event, flags);
    cudaEventCreateWithFlags(output_event, io_flags);

    // Start input clearing
    clear_input();
    cudaEventRecord(*this->clear_event, this->kernel_stream);

    // Ensure all layer streams wait for appropriate input
    wait_event(INPUT, this->input_event);
    wait_event(INPUT_OUTPUT, this->input_event);
    wait_event(OUTPUT, this->clear_event);
    wait_event(INTERNAL, this->clear_event);

    // Launch output relevant computations
    schedule_from(INPUT_OUTPUT);
    schedule_from(OUTPUT);
    schedule_to(OUTPUT);
    schedule_to(INPUT_OUTPUT);
    Scheduler::get_instance()->dispatch(this);

    // Wait for output computations to finish
    for (int i = 0; i < IO_TYPE_SIZE; ++i) {
        stream_clusters[i].block_stream(this->kernel_stream, OUTPUT);
        stream_clusters[i].block_stream(this->kernel_stream, INPUT_OUTPUT);
    }
    stream_clusters[INPUT_OUTPUT].block_stream(this->kernel_stream);
    stream_clusters[OUTPUT].block_stream(this->kernel_stream);

    // Output state computation
    step_states(INPUT_OUTPUT);
    step_states(OUTPUT);
    cudaEventRecord(*this->output_calc_event, this->kernel_stream);

    // Launch remaining calculations
    schedule_to(INPUT);
    schedule_to(INTERNAL);
    Scheduler::get_instance()->dispatch(this);
    // Block kernel stream until they are done
    stream_clusters[INPUT].block_stream(this->kernel_stream);
    stream_clusters[INTERNAL].block_stream(this->kernel_stream);

    // Launch final state computations
    step_states(INPUT);
    step_states(INTERNAL);

    // Wait for input stream to return
    cudaEventSynchronize(*this->input_event);
#else
    clear_input();
#endif
}

void Driver::stage_input() {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        stream_clusters[i].reset();

#ifdef PARALLEL
    // Start input streaming
    this->state->get_input(this->io_stream);
    cudaEventRecord(*this->input_event, this->io_stream);

    // Wait for input stream to return
    cudaEventSynchronize(*this->input_event);
#else
    this->state->get_input();
#endif
}

void Driver::stage_calc_output() {
#ifdef PARALLEL
#else
    schedule_from(OUTPUT);
    schedule_from(INPUT_OUTPUT);
    schedule_to(OUTPUT);
    schedule_to(INPUT_OUTPUT);
    Scheduler::get_instance()->dispatch(this);
    step_states(INPUT_OUTPUT);
    step_states(OUTPUT);
#endif
}

void Driver::stage_send_output() {
#ifdef PARALLEL
    // Wait for output calculation to complete
    cudaEventSynchronize(*this->output_calc_event);

    // Start stream, and wait for it to return
    this->state->send_output(this->io_stream);
    cudaEventSynchronize(*this->output_event);
#else
    this->state->send_output();
#endif
}

void Driver::stage_remaining() {
#ifdef PARALLEL
    // Synchronize and check for errors
    cudaSync();
    cudaCheckError(NULL);
#else
    schedule_to(INPUT);
    schedule_to(INTERNAL);
    Scheduler::get_instance()->dispatch(this);
    step_states(INPUT);
    step_states(INTERNAL);
    step_weights();
#endif
}


void Driver::step_all_states() {
#ifdef PARALLEL
    this->state->attributes->update(0, this->state->attributes->get_num_neurons(), this->kernel_stream);
#else
    this->state->attributes->update(0, this->state->attributes->get_num_neurons());
#endif
}

void Driver::step_states(IOType layer_type) {
    int start_index = this->state->attributes->get_start_index(layer_type);
    int count = this->state->attributes->get_num_neurons(layer_type);
    if (count > 0)
#ifdef PARALLEL
        this->state->attributes->update(start_index, count, this->kernel_stream);
#else
        this->state->attributes->update(start_index, count);
#endif
}

void Driver::step_weights() {
    for (int i = 0; i < IO_TYPE_SIZE; ++i)
        stream_clusters[i].schedule_weight_update();
    Scheduler::get_instance()->dispatch(this);
}

void Driver::clear_input() {
    float *input = this->state->attributes->get_input();
    int offset = this->state->attributes->get_start_index(OUTPUT);
    int count = this->state->attributes->get_num_neurons() - offset;
    if (count > 0) {
#ifdef PARALLEL
        int threads = calc_threads(count);
        int blocks = calc_blocks(count);
        // Use the kernel stream
        clear_data<<<blocks, threads, 0, this->kernel_stream>>>(
            input + offset, count);
        cudaCheckError("Failed to clear inputs!");
#else
        clear_data(input + offset, count);
#endif
    }
}

#ifdef PARALLEL
void Driver::step_connection(Instruction *inst, cudaStream_t *stream) {
#else
void Driver::step_connection(Instruction *inst) {
#endif
    void(*kernel)(Instruction);

    // Determine which kernel to use based on connection type
    switch (inst->type) {
        case (FULLY_CONNECTED):
            kernel = &calc_fully_connected;
            break;
        case (ONE_TO_ONE):
            kernel = &calc_one_to_one;
            break;
        case (DIVERGENT):
        case (DIVERGENT_CONVOLUTIONAL):
            kernel = &calc_divergent;
            break;
        case (CONVERGENT):
        case (CONVERGENT_CONVOLUTIONAL):
            kernel = &calc_convergent;
            break;
        default:
            throw "Unimplemented connection type!";
    }

#ifdef PARALLEL
    // Calculate grid and block sizes based on type
    dim3 blocks_per_grid;
    dim3 threads_per_block;
    int threads = calc_threads(inst->to_size);

    switch (inst->type) {
        case (FULLY_CONNECTED):
        case (ONE_TO_ONE):
            blocks_per_grid = dim3(calc_blocks(inst->to_size));
            threads_per_block = dim3(threads);
            break;
        case (DIVERGENT):
        case (DIVERGENT_CONVOLUTIONAL):
        case (CONVERGENT):
        case (CONVERGENT_CONVOLUTIONAL):
            blocks_per_grid = dim3(
                inst->to_rows,
                calc_blocks(inst->to_columns));
            threads_per_block = dim3(1, threads);
            break;
        default:
            throw "Unimplemented connection type!";
    }

    // Run the parallel kernel
    kernel<<<blocks_per_grid, threads_per_block, 0, *stream>>>(
        *inst);
    cudaCheckError("Failed to calculate connection activation!");

#else
    // Run the serial kernel
    kernel(*inst);
#endif
}


Driver* build_driver(Model* model) {
    State* state;
    if (model->driver_string == "izhikevich")
        state = new State(model, new IzhikevichAttributes(model), 1);
    else if (model->driver_string == "rate_encoding")
        state = new State(model, new RateEncodingAttributes(model), 1);
    else
        throw "Unrecognized driver type!";
    return new Driver(model, state);
}
