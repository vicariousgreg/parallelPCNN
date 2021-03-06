#ifndef weight_matrix_h
#define weight_matrix_h

#include "util/constants.h"
#include "util/resources/pointer.h"

class Connection;

class WeightMatrix {
    public:
        WeightMatrix(Connection *conn);
        virtual ~WeightMatrix();

        // Build function
        static WeightMatrix *build(Connection *conn);

        // Getters
        Pointer<float> get_weights() const { return weights; }
        Pointer<float> get_weights_transposed() const
            { return weights_transposed; }
        Pointer<float> get_second_order_weights() const
            { return second_order_weights; }
        BasePointer* get_layer(std::string key);
        DeviceID get_device_id() { return device_id; }
        int get_rows() const { return (transposed) ? columns : rows; }
        int get_columns() const { return (transposed) ? rows : columns; }

        // Pointer sets and transfer functions
        std::vector<BasePointer*> get_pointers();
        std::map<PointerKey, BasePointer*> get_pointer_map();
        void transfer(DeviceID new_device);

        // Transposition
        void transpose();
        void set_transpose_flag(bool t);
        bool get_transpose_flag() { return transpose_flag; }

        // Subclasses implement this for variable registration
        virtual void register_variables() { }

        // Auto-resize for sparse matrices, which may need to be resized
        //   after a state save/load
        void resize();

        // Adjust sparse indices in case they are invalid because of wrapping
        void adjust_sparse_indices();

        // Purge auxiliary memory from intialization
        void purge_auxiliary();

        /* Stores to/from indices in arrays */
        void get_indices();

        /* Stores unit distances in arrays */
        void get_distances(float from_spacing, float to_spacing,
            float x_offset=0.0, float y_offset=0.0);

        /* Stores uniform delays */
        void get_delays(int base_delay);

        /* Stores delays in array according to spatial organization */
        void get_delays(OutputType output_type,
            float from_spacing, float to_spacing,
            float x_offset, float y_offset,
            float velocity, bool cap_delay);

        // Pointer to this object
        // If parallel, this will point to the device copy
        WeightMatrix *pointer;

        // Weights
        Pointer<float> weights;
        Pointer<float> weights_transposed;
        Pointer<float> second_order_weights;

        // Bit vector of which weights are in use
        Pointer<int> used;
        // Number of used weights per destination neuron
        Pointer<int> nonzero_counts;
        // From/to indices for each weight
        Pointer<int> from_row_indices;
        Pointer<int> from_column_indices;
        Pointer<int> from_indices;
        Pointer<int> to_row_indices;
        Pointer<int> to_column_indices;
        Pointer<int> to_indices;

        // Distances between nodes for each weight
        Pointer<float> distances;

        // Transmission delays for each weight
        Pointer<int> delays;

        Connection* const connection;

    protected:
        // Additional matrix layers (variables)
        template<class T> Pointer<T> create_variable();
        void register_variable(std::string key, BasePointer* ptr);
        std::map<std::string, BasePointer*> variables;

        DeviceID device_id;
        bool transposed;
        bool transpose_flag;
        int num_weights;
        int rows;
        int columns;
        bool sparse;

        // Initialization
        void init();

        // Randomizes spatial locations of connections
        void randomize_projection();

        // Sparsify functionality
        void sparsify();

        virtual int get_object_size() { return sizeof(WeightMatrix); }
};

/* Macros for WeightMatrix subclass Registry */
// Put this one in .cpp
#define REGISTER_WEIGHT_MATRIX(CLASS_NAME, STRING) \
CLASS_NAME::CLASS_NAME(Connection *conn) \
    : WeightMatrix(conn) { } \
static bool __mat_dummy = \
    NeuralModelBank::register_weight_matrix( \
        STRING, CLASS_NAME::build); \
int CLASS_NAME::get_object_size() { return sizeof(CLASS_NAME); } \
\
WeightMatrix *CLASS_NAME::build(Connection *conn) { \
    auto mat = new CLASS_NAME(conn); \
    mat->init(); \
    return mat; \
}

// Use this if you want to reuse a defined weight matrix class
#define USE_WEIGHT_MATRIX(CLASS_NAME, STRING) \
static bool __mat_dummy = \
    NeuralModelBank::register_weight_matrix( \
        STRING, CLASS_NAME::build);

// Put this one in .h at bottom of class definition
#define WEIGHT_MATRIX_MEMBERS(CLASS_NAME) \
    public: \
        CLASS_NAME(Connection *conn); \
        static WeightMatrix *build(Connection *conn); \
    protected: \
        virtual int get_object_size();

#endif
