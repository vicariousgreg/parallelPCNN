#include <cstdlib>

#include "driver.h"
#include "model.h"
#include "tools.h"
#include "parallel.h"


float* Driver::get_input() {
#ifdef PARALLEL
    // Copy from GPU to local location
    cudaMemcpy(this->input, this->device_input,
        this->model->num_neurons * sizeof(float), cudaMemcpyDeviceToHost);
    cudaCheckError("Failed to copy input from device to host!");
#endif
    return this->input;
}

void* Driver::get_output() {
#ifdef PARALLEL
    // Copy from GPU to local location
    cudaMemcpy(this->output, this->device_output,
        this->model->num_neurons * sizeof(float), cudaMemcpyDeviceToHost);
    cudaCheckError("Failed to copy output from device to host!");
#endif
    return this->output;
}

void Driver::set_input(int layer_id, float* input) {
    int offset = this->model->layers[layer_id].index;
    int size = this->model->layers[layer_id].size;
#ifdef PARALLEL
    // Send to GPU
    cudaMemcpy(&this->device_input[offset], input,
        size * sizeof(float), cudaMemcpyHostToDevice);
    cudaCheckError("Failed to set input!");
#else
    for (int nid = 0 ; nid < size; ++nid) {
        this->input[nid+offset] = input[nid];
    }
#endif
}

void Driver::randomize_input(int layer_id, float max) {
    int size = this->model->layers[layer_id].size;
    int offset = this->model->layers[layer_id].index;

    for (int nid = 0 ; nid < size; ++nid) {
        this->input[nid+offset] = fRand(0, max);
    }
#ifdef PARALLEL
    // Send to GPU
    this->set_input(layer_id, &this->input[offset]);
#endif
}

void Driver::clear_input(int layer_id) {
    int size = this->model->layers[layer_id].size;
    int offset = this->model->layers[layer_id].index;

    for (int nid = 0 ; nid < size; ++nid) {
        this->input[nid+offset] = 0.0;
    }
#ifdef PARALLEL
    // Send to GPU
    this->set_input(layer_id, &this->input[offset]);
#endif
}

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

/* Randomizes matrices */
void randomize_matrices(Model* model, float** entry_points, int depth) {
    for (int i = 0 ; i < model->num_connections ; ++i) {
        Connection &conn = model->connections[i];
        // Skip shared connections
        if (conn.parent != -1) continue;
        float* mData = entry_points[i];

        // Multiply by depth if plastic
        int matrix_size = conn.num_weights;
        if (conn.plastic) matrix_size *= depth;

#ifdef PARALLEL
        float* temp_matrix = (float*)calloc(matrix_size, sizeof(float));
        if (temp_matrix == NULL)
            throw "Failed to allocate temporary matrix on host for randomization!";

#endif
        // Randomize the first layer of the matrix (weights)
        // Further layers are initialized to zero.
        for (int index = 0 ; index < conn.num_weights ; ++index) {
#ifdef PARALLEL
            temp_matrix[index] = fRand(0, conn.max_weight);
#else
            mData[index] = fRand(0, conn.max_weight);
#endif
        }

        // Set up further layers if necessary (initialize to zero)
        for (int index = conn.num_weights ; index < matrix_size ; ++index) {
#ifdef PARALLEL
            temp_matrix[index] = 0.0;
#else
            mData[index] = 0.0;
#endif
        }

#ifdef PARALLEL
        cudaMemcpy(mData, temp_matrix,
            matrix_size * sizeof(float), cudaMemcpyHostToDevice);
        cudaCheckError("Failed to randomize weight matrix!");
        free(temp_matrix);
#endif
    }
}

float** build_weight_matrices(Model* model, int depth) {
    // Allocate one big glob for weights
    // Skip shared weights because they don't take up extra space
    int total_size = 0;
    for (int i = 0 ; i < model->num_connections ; ++i) {
        int matrix_size = 0;
        if (model->connections[i].parent == -1)
            total_size += model->connections[i].num_weights;

        // If plastic, multiply by depth to make room.
        if (model->connections[i].plastic)
            matrix_size *= depth;
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
        (float**)malloc(model->num_connections * sizeof(float*));
    float* curr_point = matrix_datas;
    for (int i = 0 ; i < model->num_connections ; ++i) {
        Connection &conn = model->connections[i];
        if (conn.parent == -1) {
            entry_points[i] = curr_point;
            // TODO: factor in weight depth for LTP, LTD, etc
            curr_point += conn.num_weights;
        } else {
            entry_points[i] = entry_points[conn.parent];
        }
    }
    randomize_matrices(model, entry_points, depth);
    return entry_points;
}
