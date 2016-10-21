#include <cstdlib>
#include <cstring>
#include <sstream>

#include "state/state.h"
#include "tools.h"
#include "parallel.h"

State::State(Model *model, int weight_depth) {
    // Sort model layers, to ensure memory continuity for I/O layers
    model->sort_layers();

    // Get neuron counts
    this->total_neurons = model->num_neurons;

    // Determine start indices and number of neurons for each type
    int curr_index = 0;
    for (int layer_type = 0; layer_type < IO_TYPE_SIZE; ++layer_type) {
        std::vector<Layer*> layers = model->layers[layer_type];
        int size = 0;
        for (int i = 0; i < layers.size(); ++i)
            size += layers[i]->size;

        this->start_index[layer_type] = curr_index;
        this->num_neurons[layer_type] = size;
        curr_index += size;
    }

    // Build weight matrices
    this->weight_matrices = build_weight_matrices(model, weight_depth);

    // Allocate space for input and output
    float* local_input = (float*) allocate_host(
        this->total_neurons, sizeof(float));
    Output* local_output = (Output*) allocate_host(
        this->total_neurons * HISTORY_SIZE, sizeof(Output));

#ifdef PARALLEL
    // Copy data to device, then free from host
    this->input = (float*)
        allocate_device(this->total_neurons, sizeof(float), local_input);
    this->output = (Output*)
        allocate_device(this->total_neurons * HISTORY_SIZE, sizeof(Output), local_output);
    free(local_input);
    free(local_output);
#else
    // Simply set pointers
    this->input = local_input;
    this->output = local_output;
#endif
    // Create pointer to most recent word of output
    this->recent_output = this->output + ((HISTORY_SIZE-1) * this->total_neurons);
}

State::~State() {
#ifdef PARALLEL
    cudaFree(this->input);
    cudaFree(this->output);
    cudaFree(this->weight_matrices[0]);
    cudaFree(this->weight_matrices);
#else
    free(this->input);
    free(this->output);
    free(this->weight_matrices[0]);
    free(this->weight_matrices);
#endif
}

#ifdef PARALLEL
void State::get_input_from(Buffer *buffer, cudaStream_t stream) {
    int index = this->start_index[INPUT];
    int count = this->num_neurons[INPUT] + this->num_neurons[INPUT_OUTPUT];
    if (count != 0) {
        // Copy to GPU from local location
        cudaMemcpyAsync(this->input + index, buffer->get_input(),
            count * sizeof(float), cudaMemcpyHostToDevice, stream);
        cudaCheckError("Failed to copy input from host to device!");
    }
}
void State::send_output_to(Buffer *buffer, cudaStream_t stream) {
    int index = this->start_index[INPUT_OUTPUT];
    int count = this->num_neurons[INPUT_OUTPUT] + this->num_neurons[OUTPUT];
    if (count != 0) {
        // Copy from GPU to local location
        cudaMemcpyAsync(buffer->get_output(), this->recent_output + index,
            count * sizeof(Output), cudaMemcpyDeviceToHost, stream);
    }
}

#else
void State::get_input_from(Buffer *buffer) {
    int index = this->start_index[INPUT];
    int count = this->num_neurons[INPUT] + this->num_neurons[INPUT_OUTPUT];
    if (count != 0) {
        memcpy(this->input + index, buffer->get_input(),
            count * sizeof(float));
    }
}

void State::send_output_to(Buffer *buffer) {
    int index = this->start_index[INPUT_OUTPUT];
    int count = this->num_neurons[INPUT_OUTPUT] + this->num_neurons[OUTPUT];
    if (count != 0) {
        memcpy(buffer->get_output(), this->recent_output + index,
            count * sizeof(Output));
    }
}
#endif

void* allocate_host(int count, int size) {
    void* ptr = calloc(count, size);
    if (ptr == NULL)
        throw "Failed to allocate space on host for neuron state!";
    return ptr;
}

#ifdef PARALLEL
void* allocate_device(int count, int size, void* source_data) {
    void* ptr;
    cudaMalloc(&ptr, count * size);
    cudaCheckError("Failed to allocate memory on device for neuron state!");
    if (source_data != NULL)
        cudaMemcpy(ptr, source_data, count * size, cudaMemcpyHostToDevice);
    cudaCheckError("Failed to initialize memory on device for neuron state!");
    return ptr;
}
#endif

/* Initializes matrix */
void initialize_matrix(Connection* conn,
        float* mData, int depth) {
    // Multiply by depth if plastic
    int matrix_size = conn->num_weights;
    if (conn->plastic) matrix_size *= depth;

    // Copy data over to a target matrix
    // Parallel requires a temporary matrix be created and copied
    // Serial accesses mData directly
#ifdef PARALLEL
    float *target_matrix = (float*)calloc(matrix_size, sizeof(float));
    if (target_matrix == NULL)
        throw "Failed to allocate temporary matrix on host for randomization!";
#else
    float *target_matrix = mData;
#endif

    // If parameter is specified, interpret it for initialization
    // Otherwise, perform randomization
    if (conn->init_params.size() > 0) {
        std::stringstream stream(conn->init_params);

        // Extract first value
        float value;
        stream >> value;

        // If there are no more values, set all weights to first value
        // Otherwise, read values and initialize
        if (stream.eof()) {
            for (int index = 0 ; index < conn->num_weights ; ++index)
                target_matrix[index] = value;
        } else {
            // If parallel, transpose the input...
            // Parallel convergent matrices are laid out such that each
            //   kernel is in a column
#ifdef PARALLEL
            int rows, cols;
            switch (conn->type) {
                case (FULLY_CONNECTED):
                    throw "Cannot specify all weights for fully connected matrix!";
                case (CONVERGENT_CONVOLUTIONAL):
                case (DIVERGENT_CONVOLUTIONAL):
                    rows = conn->overlap * conn->overlap;
                    cols = 1;
                    break;
                case (DIVERGENT):
                    rows = conn->overlap * conn->overlap;
                    cols = conn->from_layer->size;
                case (CONVERGENT):
                    rows = conn->overlap * conn->overlap;
                    cols = conn->to_layer->size;
                    break;
                case (ONE_TO_ONE):
                    rows = conn->from_layer->size;
                    cols = conn->to_layer->size;
                    break;
            }
            for (int col = 0 ; col < cols ; ++col) {
                for (int row = 0 ; row < rows ; ++row) {
                    target_matrix[row * cols + col] = value;
                    if (row != rows-1 and col != cols-1 and stream.eof())
                        throw "Insufficient number of weights specified!";
                    stream >> value;
                }
            }
#else
            for (int index = 0 ; index < conn->num_weights ; ++index) {
                target_matrix[index] = value;
                if (index != conn->num_weights-1 and stream.eof())
                    throw "Insufficient number of weights specified!";
                stream >> value;
            }
#endif
        }
    } else {
        for (int index = 0 ; index < conn->num_weights ; ++index)
            target_matrix[index] = fRand(0, conn->max_weight);
    }

    // Set up further layers if necessary (initialize to zero)
    for (int index = conn->num_weights ; index < matrix_size ; ++index) {
        target_matrix[index] = 0.0;
    }

#ifdef PARALLEL
    cudaMemcpy(mData, target_matrix,
        matrix_size * sizeof(float), cudaMemcpyHostToDevice);
    cudaCheckError("Failed to initialize weight matrix!");
    free(target_matrix);
#endif
}

float** build_weight_matrices(Model* model, int depth) {
    // Allocate one big glob for weights
    // Skip shared weights because they don't take up extra space
    int total_size = 0;
    for (int i = 0 ; i < model->connections.size() ; ++i) {
        int matrix_size = model->connections[i]->num_weights;
        // If plastic, multiply by depth to make room.
        if (model->connections[i]->plastic)
            matrix_size *= depth;

        if (model->connections[i]->parent == NULL)
            total_size += matrix_size;
    }

    float* matrix_datas;
#ifdef PARALLEL
    cudaMalloc((&matrix_datas), total_size * sizeof(float));
    cudaCheckError("Failed to allocate space for weight matrices!");
#else
    matrix_datas = (float*)malloc(total_size * sizeof(float));
    if (matrix_datas == NULL)
        throw "Failed to allocate space for weight matrices!";
#endif

    // Allocate double pointer for indexing purposes
    float** entry_points = 
        (float**)malloc(model->connections.size() * sizeof(float*));
    float* curr_point = matrix_datas;
    for (int i = 0 ; i < model->connections.size() ; ++i) {
        Connection *conn = model->connections[i];
        if (conn->parent == NULL) {
            entry_points[i] = curr_point;
            // Skip over appropriate amount of memory
            // Plastic matrices might have additional layers
            if (conn->plastic)
                curr_point += (conn->num_weights * depth);
            else
                curr_point += conn->num_weights;
        } else {
            entry_points[i] = entry_points[conn->parent->id];
        }
    }

    // Initialize weights
    for (int i = 0 ; i < model->connections.size() ; ++i) {
        Connection *conn = model->connections[i];

        // Skip shared connections
        if (conn->parent == NULL)
            initialize_matrix(conn, entry_points[i], depth);
    }

    return entry_points;
}
