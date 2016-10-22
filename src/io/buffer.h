#ifndef buffer_h
#define buffer_h

#include "constants.h"

class Buffer {
    public:
        Buffer(int input_start_index, int input_size,
            int output_start_index, int output_size);
        ~Buffer() {
#ifdef PARALLEL
            // Free pinned memory
            if (input_size > 0) cudaFreeHost(this->input);
            if (output_size > 0) cudaFreeHost(this->output);
#else
            // Free non-pinned memory
            if (input_size > 0) free(this->input);
            if (output_size > 0) free(this->output);
#endif
        }

        void clear_input();
        void set_input(int offset, int size, float* source);
        void set_output(int offset, int size, Output* source);

        float* get_input();
        Output* get_output();


    private:
        int input_index;
        int input_size;
        int output_index;
        int output_size;
        float *input;
        Output *output;
};

#endif
