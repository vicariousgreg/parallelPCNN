#ifndef attributes_h
#define attributes_h

#include "engine/kernel/attribute_kernel.h"
#include "model/model.h"
#include "io/buffer.h"
#include "util/constants.h"
#include "util/parallel.h"

class Attributes {
    public:
        Attributes(Model *model, OutputType output_type);
        virtual ~Attributes();

        /* Getters for IO data */
        float* get_input() { return input; }
        OutputType get_output_type() { return output_type; }
        Output* get_recent_output() { return recent_output; }
        Output* get_output(int word_index = 0) {
            return output + (total_neurons * word_index);
        }

        /* Updates neuron attributes using engine-specific kernel */
        void update(int start_index, int count);

        /* Getters for neuron count related information */
        int get_num_neurons() { return total_neurons; }
        int get_num_neurons(IOType type) { return num_neurons[type]; }
        int get_start_index(IOType type) { return start_index[type]; }

    protected:
        friend class State;

        ATTRIBUTE_KERNEL attribute_kernel;

        /* Primary attribute update function */

        /* IO functions */
        void get_input_from(Buffer *buffer);
        void send_output_to(Buffer *buffer);

#ifdef PARALLEL
        // Pointer to device copy of this object
        Attributes *device_pointer;

        void set_streams(cudaStream_t *io_stream, cudaStream_t *state_stream) {
            this->io_stream = io_stream;
            this->state_stream = state_stream;
        }

        cudaStream_t *io_stream;
        cudaStream_t *state_stream;
#endif

        // Number of neurons
        int total_neurons;
        int num_neurons[IO_TYPE_SIZE];

        // Start indices by type
        int start_index[IO_TYPE_SIZE];

        // Neuron input
        float* input;

        // Neuron output
        OutputType output_type;
        Output* output;
        Output* recent_output;
};

Attributes *build_attributes(Model *model);

#endif
