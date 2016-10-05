#ifndef constants_h
#define constants_h

/* Size (in integers) of spike history bit vector.
 * A size of 1 indicates the usage of 1 integer, which on most systems is
 *     4 bytes, or 32 bits.  This means that a history of 32 timesteps will
 *     be kept for neuron spikes.
 * The history size limits the longest connection delay possible, and also
 *     affects learning for STDP rules that use more than just the most
 *     recent spike.
 */
#define HISTORY_SIZE 8

/* Matrix Type enumeration.
 * Fully connected represents an n x m matrix.
 * One-to-one represents an n size vector connecting two layers
 *   of idential sizes.
 * Convergent connections converge from a larger layer to a smaller one.
 * Divergent connections diverge from a larger layer to a smaller one.
 * Convolutional layers are Convergent layers that share weights.
 */
enum ConnectionType {
    FULLY_CONNECTED,
    ONE_TO_ONE,
    DIVERGENT,
    DIVERGENT_CONVOLUTIONAL,
    CONVERGENT,
    CONVERGENT_CONVOLUTIONAL
};

/* Synaptic operation opcode */
enum Opcode {
    ADD,
    SUB,
    MULT,
    DIV
};


/* Synaptic operations */
#ifdef PARALLEL
#include "parallel.h"

inline __device__ float calc(Opcode opcode, float input, float sum) {
    switch (opcode) {
        case ADD:  return input + sum;
        case SUB:  return input - sum;
        case MULT: return input * (1+sum);
        case DIV:  return input / (1+sum);
    }
    assert(false);
    return 0.0;
}
#else
inline float calc(Opcode opcode, float input, float sum) {
    switch (opcode) {
        case ADD:  return input + sum;
        case SUB:  return input - sum;
        case MULT: return input * (1+sum);
        case DIV:  return input / (1+sum);
        default: throw "Unrecognized connection operation!";
    }
}
#endif

#endif
