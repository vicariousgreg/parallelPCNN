#ifndef structure_h
#define structure_h

#include <map>
#include <vector>
#include <string>

#include "util/constants.h"
#include "model/layer.h"
#include "model/connection.h"
#include "io/module/module.h"

/* Represents a neural structure in a network model.
 * Contains network graph data and parameters for layers/connections.
 *
 * Layers and connections can be created using a variety of functions.
 * In addition, input and output modules can be attached to layers using
 *    add_module(). These modules contain hooks for driving input to a layer
 *    and extracting and using output of a layer.
 */
class Structure {
    public:
        Structure (std::string name, std::string engine_name)
                : name(name),
                  engine_name(engine_name) {
            for (auto type : IOTypes) this->num_neurons[type] = 0;
        }
        virtual ~Structure() { }

        /* Gets the total neuron count */
        int get_num_neurons() const { return total_neurons; }
        int get_num_neurons(IOType type) const { return num_neurons[type]; }

        /* Connects layers in two different structures */
        static Connection* connect(
            Structure *from_structure, std::string from_layer_name,
            Structure *to_structure, std::string to_layer_name,
            bool plastic, int delay, float max_weight, ConnectionType type,
            Opcode opcode, std::string params);


        /*******************************/
        /************ LAYERS ***********/
        /*******************************/
        void add_layer(std::string name, int rows, int columns, std::string params, float noise=0.0);
        void add_layer_from_image(std::string name, std::string path, std::string params, float noise=0.0);
        const LayerList& get_layers() const { return all_layers; }
        const LayerList& get_layers(IOType type) const { return layers[type]; }
        const ConnectionList& get_connections() const { return connections; }

        /*******************************/
        /********* CONNECTIONS *********/
        /*******************************/
        Connection* connect_layers(std::string from_layer_name, std::string to_layer_name,
            bool plastic, int delay, float max_weight, ConnectionType type,
            Opcode opcode, std::string params);

        Connection* connect_layers_expected(
            std::string from_layer_name, std::string to_layer_name,
            std::string new_layer_params,
            bool plastic, int delay, float max_weight,
            ConnectionType type, Opcode opcode, std::string params,
            float noise=0.0);

        Connection* connect_layers_matching(
            std::string from_layer_name, std::string to_layer_name,
            std::string new_layer_params,
            bool plastic, int delay, float max_weight,
            ConnectionType type, Opcode opcode, std::string params,
            float noise=0.0);


        /* Dendritic internal connection functions */
        DendriticNode *spawn_dendritic_node(std::string to_layer_name);

        Connection* connect_layers_internal(
            DendriticNode *node, std::string from_layer_name,
            bool plastic, int delay, float max_weight, ConnectionType type,
            Opcode opcode, std::string params);


        /* Adds a module of the given |type| for the given |layer| */
        void add_module(std::string layer, std::string type, std::string params);

        // Structure name
        const std::string name;

        // Engine name
        const std::string engine_name;

    private:
        /* Internal layer connection functions */
        Connection* connect_layers(
                Layer *from_layer, Layer *to_layer,
                bool plastic, int delay, float max_weight,
                ConnectionType type, Opcode opcode, std::string params);

        Layer* find_layer(std::string name) {
            if (layers_by_name.find(name) != layers_by_name.end())
                return layers_by_name.find(name)->second;
            else
                return NULL;
        }

        // Layers
        LayerList all_layers;
        LayerList layers[sizeof(IOTypes)];
        std::map<std::string, Layer*> layers_by_name;

        // Connections
        ConnectionList connections;

        // Number of neurons
        int total_neurons;
        int num_neurons[sizeof(IOTypes)];
};

typedef std::vector<Structure*> StructureList;

#endif
