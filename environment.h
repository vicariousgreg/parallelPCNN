#ifndef environment_h
#define environment_h

#include <vector>
#include <ctime>
#include <cstdlib>

#include "neuron_parameters.h"
#include "state.h"
#include "layer.h"
#include "weight_matrix.h"

#define SPIKE_THRESH 30
#define EULER_RES 10
#define HISTORY_SIZE 8

using namespace std;

class Environment {
    public:
        Environment ();

        virtual ~Environment () {}

        /* Builds the environment.
         * Allocates memory for various arrays of data.
         * Calls build() on all weight matrices.
         * Returns whether allocation was successful.
         */
        bool build();

        /* Adds a layer to the environment with the given parameters */
        int add_layer(int size, int sign,
            float a, float b, float c, float d);

        /* Adds a randomized layer to the environment */
        int add_randomized_layer(int size, int sign);

        /* Connects two layers, creating a weight matrix with the given 
         *   parameters */
        int connect_layers(int from_layer, int to_layer,
            bool plastic, float max_weight);

        /* Injects current from the given pointer to the given layer */
        void inject_current(int layer_id, float* input);

        /* Injects randomized current to the given layer.
         * Input is bounded by |max| */
        void inject_random_current(int layer_id, float max);

        /* Zeroes current to the given layer */
        void clear_current(int layer_id);

        /* Returns a pointer to an array containing the most recent integer
         *   of neuron spikes.
         * If parallel, this will copy the values from the device. */
        int* get_spikes();

        /* Returns a pointer to an array containing the neuron currents.
         * If parallel, this will copy the values from the device. */
        float* get_current();

        /* Cycles the environment:
         * 1. Activate neural connections, which updates currents.
         * 2. Update neuron voltages from currents.
         * 3. Timestep the spikes.
         * 4. Update connection weights for plastic matrices */
        void cycle();

        /* Activates neural connections, triggering updates of currents */
        void activate();

        /* Updates neuron voltages from the currents using the Izhikevich model */
        void update_voltages();

        /* Timesteps the spikes, shifting spike bit vectors */
        void timestep();

        /* Updates weights for plastic neural connections.
         * TODO: implement.  This should use STDB variant Hebbian learning */
        void update_weights();

        // Neurons
        int num_neurons;

        // Environment state
        State state;

    private:
        /* Adds a single neuron.
         * This is called from add_layer() */
        int add_neuron(float a, float b, float c, float d);

        // Layers
        int num_layers;
        vector<Layer> layers;

        // Connection matrices.
        int num_connections;
        vector<WeightMatrix> connections;

        // Parameter Vector
        vector<NeuronParameters> parameters_vector;
        NeuronParameters *neuron_parameters;
};

#endif
