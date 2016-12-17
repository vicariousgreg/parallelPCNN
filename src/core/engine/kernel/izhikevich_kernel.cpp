#include "engine/kernel/izhikevich_kernel.h"

/* Voltage threshold for neuron spiking. */
#define SPIKE_THRESH 30

/* Euler resolution for voltage update. */
#define EULER_RES 2

GLOBAL void iz_update_attributes(IzhikevichAttributes *att,
        int start_index, int count, int num_neurons) {
#ifdef PARALLEL
    int nid = blockIdx.x * blockDim.x + threadIdx.x;
    if (nid < count) {
        nid += start_index;
#else
    for (int nid = start_index; nid < start_index+count; ++nid) {
#endif
        /**********************
         *** VOLTAGE UPDATE ***
         **********************/
        float voltage = att->voltage[nid];
        float recovery = att->recovery[nid];
        float current = att->current[nid];

        float a = att->neuron_parameters[nid].a;
        float b = att->neuron_parameters[nid].b;

        // Euler's method for voltage/recovery update
        // If the voltage exceeds the spiking threshold, break
        for (int i = 0 ; i < EULER_RES && voltage < SPIKE_THRESH ; ++i) {
            float delta_v = (0.04 * voltage * voltage) +
                            (5*voltage) + 140 - recovery + current;
            voltage += delta_v / EULER_RES;
            recovery += a * ((b * voltage) - recovery) / EULER_RES;
        }

        /********************
         *** SPIKE UPDATE ***
         ********************/
        // Determine if spike occurred
        int spike = voltage >= SPIKE_THRESH;

        // Reduce reads, chain values.
        int curr_value, new_value;
        int next_value = att->spikes[nid];

        // Shift all the bits.
        // Check if next word is negative (1 for MSB).
        int index;
        for (index = 0 ; index < HISTORY_SIZE-1 ; ++index) {
            curr_value = next_value;
            next_value = att->spikes[num_neurons * (index + 1) + nid];

            // Shift bits, carry over MSB from next value.
            new_value = (curr_value << 1) + (next_value < 0);
            att->spikes[num_neurons*index + nid] = new_value;
        }

        // Least significant value already loaded into next_value.
        // Index moved appropriately from loop.
        att->spikes[num_neurons*index + nid] = (next_value << 1) + spike;

        // Reset voltage if spiked.
        if (spike) {
            att->voltage[nid] = att->neuron_parameters[nid].c;
            att->recovery[nid] = recovery + att->neuron_parameters[nid].d;
        } else {
            att->voltage[nid] = voltage;
            att->recovery[nid] = recovery;
        }
    }
}
