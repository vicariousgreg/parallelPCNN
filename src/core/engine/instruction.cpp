#include "engine/instruction.h"
#include "util/error_manager.h"

ConnectionData::ConnectionData(Connection *conn, State *state) :
        convolutional(conn->convolutional),
        opcode(conn->opcode),
        plastic(conn->plastic),
        max_weight(conn->max_weight),
        overlap(conn->overlap),
        stride(conn->stride),
        delay(conn->delay),
        from_size(conn->from_layer->size),
        from_rows(conn->from_layer->rows),
        from_columns(conn->from_layer->columns),
        to_size(conn->to_layer->size),
        to_rows(conn->to_layer->rows),
        to_columns(conn->to_layer->columns),
        num_weights(conn->num_weights),
        output_type(state->get_attributes()->get_output_type()),
        weights(state->get_matrix(conn->id)) {
    this->fray =
        (to_rows == from_rows and to_columns == from_columns)
            ? overlap / 2 : 0;

    // Set up word index
    int timesteps_per_output = get_timesteps_per_output(output_type);
    int word_index = HISTORY_SIZE - 1 -
        (conn->delay / timesteps_per_output);
    if (word_index < 0)
        ErrorManager::get_instance()->log_error(
            "Invalid delay in connection!");

    outputs = state->get_attributes()->get_output(word_index)
                + conn->from_layer->index;
    inputs = state->get_attributes()->get_input()
                + conn->to_layer->index;

    get_extractor(&this->extractor, output_type);
}

Instruction::Instruction(Connection *conn, State *state) :
        connection_data(conn, state),
        type(conn->type) {
    get_activator(&this->activator, type);
    if (conn->plastic)
        get_updater(&this->updater, type);
    else
        this->updater = NULL;

#ifdef PARALLEL
    // Calculate grid and block sizes based on type
    int threads = calc_threads(conn->to_layer->size);

    switch (type) {
        case (FULLY_CONNECTED):
        case (ONE_TO_ONE):
            this->blocks_per_grid = dim3(calc_blocks(conn->to_layer->size));
            this->threads_per_block = dim3(threads);
            break;
        case (CONVERGENT):
        case (CONVOLUTIONAL):
            this->blocks_per_grid = dim3(
                conn->to_layer->rows,
                calc_blocks(conn->to_layer->columns));
            this->threads_per_block = dim3(1, threads);
            break;
        default:
            ErrorManager::get_instance()->log_error(
                "Unimplemented connection type!");
    }
#endif
}

void Instruction::disable_learning() {
    this->connection_data.plastic = false;
}

#ifdef PARALLEL
void Instruction::execute(cudaStream_t *stream) {
    activator<<<blocks_per_grid, threads_per_block, 0, *stream>>>(this->connection_data);
}

void Instruction::update(cudaStream_t *stream) {
    if (this->connection_data.plastic)
        updater<<<blocks_per_grid, threads_per_block, 0, *stream>>>(this->connection_data);
}
#else
void Instruction::execute() {
    activator(this->connection_data);
}

void Instruction::update() {
    if (this->connection_data.plastic)
        updater(this->connection_data);
}

#endif
