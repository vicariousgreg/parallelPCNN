#ifndef weight_matrix_h
#define weight_matrix_h

#include "model/connection.h"
#include "util/constants.h"
#include "util/pointer.h"
#include "util/resource_manager.h"

class WeightMatrix {
    public:
        WeightMatrix(Connection *conn, int matrix_depth);
        virtual ~WeightMatrix();

        Pointer<float> get_data() const { return mData; }

        void schedule_transfer(DeviceID device_id);

        Connection* const connection;

    private:
        Pointer<float> mData;
        int matrix_size;
};

/* Sets all values in an array to the given val */
void set_weights(float* arr, int size, float val);

/* Clears an array */
void clear_weights(float* arr, int size);

/* Randomizes an array */
void randomize_weights(float* arr, int size, float max);

/* Transfers the values from one array to another */
void transfer_weights(float* from, float* to, int size);

#endif
