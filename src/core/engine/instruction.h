#ifndef instruction_h
#define instruction_h

#include <vector>

#include "model/connection.h"
#include "state/state.h"
#include "engine/kernel/kernel_data.h"
#include "engine/kernel/kernel.h"
#include "engine/kernel/activator_kernel.h"
#include "util/parallel.h"

class Instruction {
    public:
        Instruction(Layer *layer);

        virtual void activate();
        virtual void update() { }
        virtual bool is_plastic() const { return false; }

        Layer* const to_layer;

#ifdef PARALLEL
        void set_stream(cudaStream_t *stream) { this->stream = stream; }
        void add_event(cudaEvent_t *event) { this->events.push_back(event); }

    protected:
        dim3 activator_blocks, activator_threads;
        dim3 updater_blocks, updater_threads;
        cudaStream_t *stream;
        std::vector<cudaEvent_t* > events;
#endif
};

class InitializeInstruction : public Instruction {
    public:
        InitializeInstruction(Layer *layer, State *state);

    protected:
        float *dst;
};

class ClearInstruction : public InitializeInstruction {
    public:
        ClearInstruction(Layer *layer, State *state)
                : InitializeInstruction(layer, state) { }

        void activate();
};

class RandomizeInstruction : public InitializeInstruction {
    public:
        RandomizeInstruction(Layer *layer, State *state)
                : InitializeInstruction(layer, state),
                  clear(layer->get_input_module() == NULL) { }

        void activate();

    protected:
        bool clear;
};

class SynapseInstruction : public Instruction {
    public:
        SynapseInstruction(Connection *conn, State *state);

        void activate();
        void update();
        bool is_plastic() const { return kernel_data.plastic; }

        const ConnectionType type;
        Connection* const connection;

    protected:
        EXTRACTOR extractor;
        KERNEL activator;
        KERNEL updater;
        const KernelData kernel_data;
};

class DendriticInstruction : public Instruction {
    public:
        DendriticInstruction(DendriticNode *parent,
            DendriticNode *child, State *state);

        void activate();

    protected:
        float *src, *dst;
        bool clear;
};

typedef std::vector<Instruction*> InstructionList;

#endif
