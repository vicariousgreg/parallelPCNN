#include <algorithm>
#include <sstream>

#include "state/weight_matrix.h"
#include "network/layer.h"
#include "network/connection.h"
#include "util/error_manager.h"

/* Sets all values in an array to the given val */
void set_weights(float* arr, int size, float val, float fraction) {
    fSet(arr, size, val, fraction);
}

/* Clears an array */
void clear_weights(float* arr, int size) {
    set_weights(arr, size, 0.0);
}

/* Randomizes an array */
void randomize_weights(float* arr, int size, float max, float fraction) {
    fRand(arr, size, 0, max, fraction);
}
void randomize_weights_gaussian(float* arr, int size,
        float mean, float std_dev, float max, float fraction) {
    // If standard deviation is 0.0, just set the weights to the mean
    if (std_dev == 0.0) {
        set_weights(arr, size, mean, fraction);
    } else {
        std::normal_distribution<double> dist(mean, std_dev);

        if (fraction == 1.0) {
            for (int i = 0 ; i < size ; ++i)
                arr[i] = std::min((double)max, std::max(0.0, dist(generator)));
        } else {
            std::uniform_real_distribution<double> f_dist(0.0, 1.0);
            for (int i = 0 ; i < size ; ++i)
                arr[i] = (f_dist(generator) < fraction)
                    ? std::min((double)max, std::max(0.0, dist(generator)))
                    : 0.0;
        }
    }
}
void randomize_weights_lognormal(float* arr, int size,
        float mean, float std_dev, float max, float fraction) {
    // If standard deviation is 0.0, just set the weights to the mean
    if (std_dev == 0.0) {
        set_weights(arr, size, mean, fraction);
    } else {
        std::lognormal_distribution<double> dist(mean, std_dev);

        if (fraction == 1.0) {
            for (int i = 0 ; i < size ; ++i)
                arr[i] = std::min((double)max, std::max(0.0, dist(generator)));
        } else {
            std::uniform_real_distribution<double> f_dist(0.0, 1.0);
            for (int i = 0 ; i < size ; ++i)
                arr[i] = (f_dist(generator) < fraction)
                    ? std::min((double)max, std::max(0.0, dist(generator)))
                    : 0.0;
        }
    }
}
void randomize_weights_powerlaw(float* arr, int size,
        float exponent, float max, float fraction) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    float coeff = pow(max, exponent+1);
    float pow_exp = 1.0 / (exponent+1);

    if (fraction == 1.0) {
        for (int i = 0 ; i < size ; ++i)
            arr[i] = pow(coeff * dist(generator), pow_exp);
    } else {
        for (int i = 0 ; i < size ; ++i)
            arr[i] = (dist(generator) < fraction)
                ? pow(coeff * dist(generator), pow_exp)
                : 0.0;
    }
}

/* Transfers the values from one array to another */
void transfer_weights(float* from, float* to, int size) {
    for (int i = 0 ; i < size ; ++i) to[i] = from[i];
}

/* Clears the diagonal of a weight matrix */
void clear_diagonal(float *arr, int rows, int cols) {
    if (rows != cols)
        LOG_ERROR(
            "Attempted to clear diagonal of non-square weight matrix!");

    for (int i = 0 ; i < rows ; ++i)
        arr[i * rows + i] = 0.0;
}

static void initialize_weights(const PropertyConfig config,
    float* target_matrix, Connection* conn, bool is_host);

WeightMatrix::WeightMatrix(Connection* conn, int matrix_depth,
        DeviceID device_id)
    : connection(conn),
      depth(matrix_depth),
      num_weights(conn->get_num_weights()),
      device_id(device_id) {
    matrix_size = num_weights * matrix_depth;

    // Allocate matrix on host
    // If parallel, it will be copied below
    mData = Pointer<float>(matrix_size);
    if (mData.get() == nullptr)
        LOG_ERROR(
            "Failed to allocate space for weight matrices on host!");

    // If parameter is specified, interpret it for initialization
    // Otherwise, perform randomization
    initialize_weights(conn->get_config()->get_weight_config(),
        mData, conn, ResourceManager::get_instance()->is_host(device_id));
}

WeightMatrix::~WeightMatrix() {
    this->mData.free();
}

Pointer<float> WeightMatrix::get_layer(int index) const {
    if (index < 0 or index >= depth)
        LOG_ERROR(
            "Attempted to access out of bounds Weight Matrix layer : "
            + std::to_string(index) + " on connection: " + connection->str());
    return mData.slice(num_weights * index, num_weights);
}

BasePointer* WeightMatrix::get_pointer() {
    return &this->mData;
}

/******************************************************************************/
/**************************** WEIGHT INITIALIZATION ***************************/
/******************************************************************************/
static void flat_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    float weight = config.get_float("weight", 1.0);
    float fraction = config.get_float("fraction", 1.0);

    set_weights(target_matrix, conn->get_num_weights(), weight, fraction);
}

static void random_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    float max_weight = config.get_float("max weight", 1.0);
    float fraction = config.get_float("fraction", 1.0);

    randomize_weights(target_matrix, conn->get_num_weights(),
        max_weight, fraction);
}

static void gaussian_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    float mean = config.get_float("mean", 1.0);
    float std_dev = config.get_float("std dev", 0.3);
    float fraction = config.get_float("fraction", 1.0);

    if (std_dev < 0)
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Gaussian weight config std_dev must be positive!");

    randomize_weights_gaussian(target_matrix, conn->get_num_weights(),
        mean, std_dev, conn->max_weight, fraction);
}

static void log_normal_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    float mean = config.get_float("mean", 1.0);
    float std_dev = config.get_float("std dev", 0.3);
    float fraction = config.get_float("fraction", 1.0);

    if (std_dev < 0)
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Log normal weight config std_dev must be positive!");

    randomize_weights_lognormal(target_matrix, conn->get_num_weights(),
        mean, std_dev, conn->max_weight, fraction);
}

static void power_law_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    float exponent = config.get_float("exponent", 1.5);
    float fraction = config.get_float("fraction", 1.0);

    exponent = abs(exponent);

    if (conn->max_weight < 0)
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Power law config max must be positive!");

    randomize_weights_powerlaw(target_matrix, conn->get_num_weights(),
        exponent, conn->max_weight, fraction);
}

static void surround_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    switch (conn->type) {
        case CONVERGENT:
            break;
        default:
            LOG_ERROR(
                "Error in weight config for " + conn->str() + ":\n"
                "  SurroundWeightConfig can only be used "
                "on convergent/convolutional arborized connections!");
    }

    int rows = config.get_int("rows", 0);
    int cols = config.get_int("columns", 0);
    int size_param = config.get_int("size", -1);

    if (size_param >= 0)
        rows = cols = size_param;
    if (rows < 0 or cols < 0)
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Surround weight config rows/cols must be positive!");

    // Carve out center
    auto ac = conn->get_config()->get_arborized_config();
    int row_field_size = ac.column_field_size;
    int col_field_size = ac.column_field_size;
    int kernel_size = ac.get_total_field_size();

    int row_offset = (row_field_size - rows) / 2;
    int col_offset = (col_field_size - cols) / 2;

    // Convolutional connections are unique in that there is only one kernel.
    int size;
    switch (conn->type) {
        case CONVERGENT:
            if (conn->convolutional) size = 1;
            else size = conn->to_layer->size;
            break;
    }

    if (is_host) {
        for (int index = 0 ; index < size ; ++index) {
            int weight_offset = (conn->convolutional)
                ? 0 : (index * kernel_size);

            for (int k_row = row_offset ; k_row < row_offset + rows ; ++k_row) {
                for (int k_col = col_offset ;
                        k_col < col_offset + cols ; ++k_col) {
                    int weight_index = weight_offset +
                        (k_row * col_field_size) + k_col;
                    target_matrix[weight_index] = 0.0;
                }
            }
        }
    } else {
        for (int index = 0 ; index < size ; ++index) {
            int weight_col = (conn->convolutional) ? 0 : index;
            int kernel_row_size = (conn->convolutional) ? 1 : size;

            for (int k_row = row_offset ; k_row < row_offset + rows ; ++k_row) {
                for (int k_col = col_offset ;
                        k_col < col_offset + cols ; ++k_col) {
                    int weight_index = weight_col +
                        ((k_row * col_field_size) + k_col) * kernel_row_size;
                    target_matrix[weight_index] = 0.0;
                }
            }
        }
    }
}

static void specified_config(const PropertyConfig& config, float* target_matrix,
        Connection* conn, bool is_host) {
    std::string weight_string = config.get("weight string", "");
    if (weight_string == "")
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Missing weight string for specified weight config!");

    std::stringstream stream(weight_string);
    int num_weights = conn->get_num_weights();

    int rows = conn->to_layer->size;
    int cols = conn->from_layer->size;
    auto ac = conn->get_config()->get_arborized_config();
    int row_field_size = ac.row_field_size;
    int column_field_size = ac.column_field_size;
    switch (conn->type) {
        case CONVERGENT:
            if (conn->convolutional) {
                rows = 1;
                cols = row_field_size * column_field_size;
            } else {
                rows = conn->to_layer->size;
                cols = row_field_size * column_field_size;
            }
            break;
        case DIVERGENT:
            if (conn->convolutional) {
                rows = 1;
                cols = row_field_size * column_field_size;
            } else {
                rows = conn->from_layer->size;
                cols = row_field_size * column_field_size;
            }
            break;
        case ONE_TO_ONE:
            rows = 1;
            break;
    }

    float value;
    for (int row = 0 ; row < rows ; ++row) {
        for (int col = 0 ; col < cols ; ++col) {
            if (row != rows-1 and col != cols-1 and stream.eof())
                LOG_ERROR(
                    "Error in weight config for " + conn->str() + ":\n"
                    "  Insufficient number of weights specified!");
            else stream >> value;

            // If parallel, transpose the input (rows <-> cols)
            // Parallel convergent matrices are laid out such that each
            //   kernel is in a column
            if (is_host) target_matrix[row * cols + col] = value;
            else         target_matrix[col * rows + row] = value;
        }
    }
}

static void initialize_weights(const PropertyConfig config,
        float* target_matrix, Connection* conn, bool is_host) {
    if (config.has("fraction")) {
        float fraction = config.get_float("fraction", 1.0);
        if (fraction < 0 or fraction > 1.0)
            LOG_ERROR(
                "Error in weight config for " + conn->str() + ":\n"
                "  Weight config fraction must be between 0 and 1!");
    }

    auto type = config.get("type", "flat");

    // If surround, treat as child type and then process as surround after
    bool surround = type == "surround";
    if (surround) {
        if (not config.has("child type"))
            LOG_ERROR(
                "Error in weight config for " + conn->str() + ":\n"
                "  Missing child weight config for surround weight config!");
        type = config.get("child type");
    }

    if (type == "flat")
        flat_config(config, target_matrix, conn, is_host);
    else if (type == "random")
        random_config(config, target_matrix, conn, is_host);
    else if (type == "gaussian")
        gaussian_config(config, target_matrix, conn, is_host);
    else if (type == "log normal")
        log_normal_config(config, target_matrix, conn, is_host);
    else if (type == "power law")
        power_law_config(config, target_matrix, conn, is_host);
    else if (type == "specified")
        specified_config(config, target_matrix, conn, is_host);
    else if (type == "surround")
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Surround weight configs cannot be nested!");
    else
        LOG_ERROR(
            "Error in weight config for " + conn->str() + ":\n"
            "  Unrecognized weight config type: " + type);

    // Now do surround processing
    if (surround)
        surround_config(config, target_matrix, conn, is_host);

    if (not config.get_bool("diagonal", true)) {
        switch(conn->type) {
            case FULLY_CONNECTED:
                clear_diagonal(target_matrix,
                    conn->from_layer->size, conn->to_layer->size);
                break;
            case SUBSET: {
                auto subset_config = conn->get_config()->get_subset_config();
                clear_diagonal(target_matrix,
                    subset_config.from_size,
                    subset_config.to_size);
                break;
            }
            case CONVERGENT:
                break;
        }
    }
}

/******************************************************************************/
/**************************** DELAY INITIALIZATION ****************************/
/******************************************************************************/
void set_delays(DeviceID device_id, OutputType output_type, Connection *conn,
        int* delays, float velocity, bool cap_delay,
        float from_spacing, float to_spacing,
        float x_offset, float y_offset) {
    if (output_type != BIT)
        LOG_ERROR(
            "Only BIT output connections can have variable delays!");
    int base_delay = conn->delay;

    LOG_DEBUG(
        conn->from_layer->name + " -> " +
        conn->to_layer->name + "delay initialization...\n");

    bool is_host = ResourceManager::get_instance()->is_host(device_id);

    switch(conn->type) {
        case FULLY_CONNECTED:
        case SUBSET: {
            int from_row_start, from_col_start, to_row_start, to_col_start;
            int from_row_end, from_col_end, to_row_end, to_col_end;

            if (conn->type == FULLY_CONNECTED) {
                from_row_start = from_col_start = to_row_start = to_col_start = 0;
                from_row_end = conn->from_layer->rows;
                from_col_end = conn->from_layer->columns;
                to_row_end = conn->to_layer->rows;
                to_col_end = conn->to_layer->columns;
            } else if (conn->type == SUBSET) {
                auto sc = conn->get_config()->get_subset_config();
                from_row_start = sc.from_row_start;
                from_col_start = sc.from_col_start;
                to_row_start = sc.to_row_start;
                to_col_start = sc.to_col_start;
                from_row_end = sc.from_row_end;
                from_col_end = sc.from_col_end;
                to_row_end = sc.to_row_end;
                to_col_end = sc.to_col_end;
            }

            int from_columns = conn->from_layer->columns;
            int weight_index = 0;
            for (int f_row = from_row_start; f_row < from_row_end; ++f_row) {
                float f_y = f_row * from_spacing;

                for (int f_col = from_col_start; f_col < from_col_end; ++f_col) {
                    float f_x = f_col * from_spacing;

                    for (int t_row = to_row_start; t_row < to_row_end; ++t_row) {
                        float t_y = t_row * to_spacing + y_offset;

                        for (int t_col = to_col_start; t_col < to_col_end; ++t_col) {
                            float t_x = t_col * to_spacing + x_offset;

                            float distance = pow(
                                pow(t_x - f_x, 2) + pow(t_y - f_y, 2),
                                0.5);
                            int delay = base_delay + (distance / velocity);
                            if (delay > 31)
                                if (cap_delay) delay = 31;
                                else
                                    LOG_ERROR(
                                        "Error initializing delays for "
                                        + conn->str() + "\n"
                                        "  Unmyelinated axons cannot have delays "
                                        "greater than 31!");
                            delays[weight_index] = delay;
                            ++weight_index;
                        }
                    }
                }
            }
            break;
        }
        case ONE_TO_ONE:
            for (int i = 0 ; i < conn->get_num_weights() ; ++i)
                delays[i] = base_delay;
            break;
        case CONVERGENT: {
            auto ac = conn->get_config()->get_arborized_config();
            int to_size = conn->to_layer->size;
            int field_size = ac.row_field_size * ac.column_field_size;

            if (ac.row_stride != ac.column_stride
                or (int(to_spacing / from_spacing) != ac.row_stride))
                LOG_ERROR(
                    "Error initializing delays for " + conn->str() + "\n"
                    "  Spacing and strides must match up for "
                    "convergent connection (got " + std::to_string(to_spacing)
                    + ", " + std::to_string(from_spacing) + "!");

            for (int f_row = 0; f_row < ac.row_field_size; ++f_row) {
                float f_y = (f_row*ac.row_spacing + ac.row_offset) * to_spacing;

                for (int f_col = 0; f_col < ac.column_field_size; ++f_col) {
                    float f_x = ((f_col*ac.column_spacing) + ac.column_offset) * to_spacing;

                    float distance = pow(
                        pow(f_x, 2) + pow(f_y, 2),
                        0.5);
                    int delay = base_delay + (distance / velocity);
                    if (delay > 31)
                        if (cap_delay) delay = 31;
                        else
                            LOG_ERROR(
                                "Error initializing delays for " + conn->str()
                                + "\n  Unmyelinated axons cannot have delays "
                                "greater than 31!");

                    int f_index = (f_row * ac.column_field_size) + f_col;

                    if (is_host)
                        for (int i = 0 ; i < to_size ; ++i)
                            delays[(i * field_size) + f_index] = delay;
                    else
                        for (int i = 0 ; i < to_size ; ++i)
                            delays[(f_index * to_size) + i] = delay;
                }
            }
            break;
        }
        case DIVERGENT: {
            auto ac = conn->get_config()->get_arborized_config();
            int num_weights = conn->get_num_weights();
            int to_rows = conn->to_layer->rows;
            int to_columns = conn->to_layer->columns;
            int to_size = conn->to_layer->size;
            int from_rows = conn->from_layer->rows;
            int from_columns = conn->from_layer->columns;

            if (ac.row_stride != ac.column_stride
                or (int(from_spacing / to_spacing) != ac.row_stride))
                LOG_ERROR(
                    "Error initializing delays for " + conn->str() + "\n"
                    "  Spacing and strides must match up for "
                    "convergent connection (got " + std::to_string(to_spacing)
                    + ", " + std::to_string(from_spacing) + "!");


            int row_field_size = ac.row_field_size;
            int column_field_size = ac.column_field_size;
            int row_stride = ac.row_stride;
            int column_stride = ac.column_stride;
            int row_spacing = ac.row_spacing;
            int column_spacing = ac.column_spacing;
            int row_offset = ac.row_offset;
            int column_offset = ac.column_offset;
            int kernel_size = row_field_size * column_field_size;

            for (int d_row = 0 ; d_row < to_rows ; ++d_row) {
                for (int d_col = 0 ; d_col < to_columns ; ++d_col) {
                    int to_index = d_row*to_columns + d_col;

                    /* Determine range of source neurons for divergent kernel */
                    int start_s_row = (d_row - row_offset - row_field_size + row_stride) / row_stride;
                    int start_s_col = (d_col - column_offset - column_field_size + column_stride) / column_stride;
                    int end_s_row = start_s_row + (row_spacing * (row_field_size + row_stride) / row_stride);
                    int end_s_col = start_s_col + (column_spacing * (column_field_size + column_stride) / column_stride);

                    // SERIAL
                    int weight_offset = to_index * num_weights / to_size;
                    // PARALLEL
                    int kernel_row_size = num_weights / to_size;

                    /* Iterate over relevant source neurons... */
                    int k_index = 0;
                    for (int s_row = start_s_row ; s_row <= end_s_row ; (s_row += row_spacing)) {
                        for (int s_col = start_s_col ; s_col <= end_s_col ; (s_col += column_spacing, ++k_index)) {
                            /* If wrapping, adjust out of bounds indices accordingly */
                            if (not ac.wrap and
                                (s_row < 0 or s_row >= from_rows
                                or s_col < 0 or s_col >= from_columns)) {
                                continue;
                            }

                            int from_index = (s_row * from_columns) + s_col;

                            float d_x = abs(
                                ((d_col + ac.column_offset) * to_spacing)
                                - (s_col * from_spacing));
                            float d_y = abs(
                                ((d_row + ac.row_offset) * to_spacing)
                                - (s_row * from_spacing));

                            float distance = pow(
                                pow(d_x, 2) + pow(d_y, 2),
                                0.5);
                            int delay = base_delay + (distance / velocity);
                            if (delay > 31)
                                if (cap_delay) delay = 31;
                                else
                                    LOG_ERROR(
                                        "Error initializing delays for "
                                        + conn->str() + "\n"
                                        "  Unmyelinated axons cannot have "
                                        "delays greater than 31!");
                            int weight_index = (is_host)
                                ? weight_offset + k_index
                                : to_index + (k_index * kernel_row_size);
                            delays[weight_index] = delay;
                        }
                    }
                }
            }
            break;
        }
    }
}
