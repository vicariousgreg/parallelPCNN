#include <cstdlib>
#include <cstring>
#include <sstream>

#include "state/state.h"
#include "tools.h"
#include "parallel.h"

static void initialize_matrix(Connection* conn,
        float* mData, int weight_depth);

WeightMatrices::WeightMatrices(Model *model, int weight_depth) {
    // Allocate one big glob for weights
    // Skip shared weights because they don't take up extra space
    int total_size = 0;
    for (int i = 0 ; i < model->connections.size() ; ++i) {
        int matrix_size = model->connections[i]->num_weights;
        // If plastic, multiply by depth to make room.
        if (model->connections[i]->plastic)
            matrix_size *= weight_depth;

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
                curr_point += (conn->num_weights * weight_depth);
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
            initialize_matrix(conn, entry_points[i], weight_depth);
    }

    this->matrices = entry_points;
}

/* Initializes matrix */
static void initialize_matrix(Connection* conn,
        float* mData, int weight_depth) {
    // Multiply by depth if plastic
    int matrix_size = conn->num_weights;
    if (conn->plastic) matrix_size *= weight_depth;

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