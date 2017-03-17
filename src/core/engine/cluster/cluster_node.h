#ifndef cluster_node_h
#define cluster_node_h

#include <map>
#include <vector>

#include "model/layer.h"
#include "io/environment.h"
#include "engine/instruction.h"

class ClusterNode {
    public:
        ClusterNode(Layer *layer, State *state, Environment *environment,
            Stream *io_stream, Stream *compute_stream);
        virtual ~ClusterNode();

        void activate_input();
        void activate_state();
        void activate_output();

        void synchronize_input();
        void synchronize_output();

        const InstructionList get_instructions() const;
        Instruction* get_input_instruction() const;
        Instruction* get_state_instruction() const;
        Instruction* get_output_instruction() const;
        const std::map<Connection*, Instruction*>
            get_synapse_instructions() const;
        const std::map<Connection*, Instruction*>
            get_external_transfer_instructions();

        Layer* const to_layer;
        const DeviceID device_id;
        Stream* const io_stream;
        Stream* const compute_stream;

        State* const state;
        Environment* const environment;

    private:
        void dendrite_DFS(DendriticNode *curr);

        std::map<Connection*, Instruction*> synapse_instructions;
        std::map<Connection*, Instruction*> external_transfer_instructions;

        InstructionList instructions;
        Instruction *state_instruction;

        bool is_input;
        Instruction *input_instruction;
        Instruction *input_copy_instruction;

        bool is_expected;
        Instruction *expected_instruction;
        Instruction *expected_copy_instruction;

        bool is_output;
        Instruction *output_instruction;
        Instruction *output_copy_instruction;

        Event *input_event;
        Event *expected_event;
        Event *output_event;
};

#endif
