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
        Structure (std::string name, ClusterType cluster_type)
                : name(name), cluster_type(cluster_type) {
            for (auto type : IOTypes) this->num_neurons[type] = 0;
            for (auto neural_model : NeuralModels)
                this->neural_model_flags.push_back(false);
        }
        virtual ~Structure() {
            for (auto layer : layers) delete layer;
            for (auto conn : connections) delete conn;
        }

        // Returns the flags for which models are represented by this structure
        const std::vector<bool> get_neural_model_flags() {
            return neural_model_flags;
        }

        /* Gets the total neuron count */
        int get_num_neurons() const { return total_neurons; }
        int get_num_neurons(IOType type) const { return num_neurons[type]; }

        /* Connects layers in two different structures */
        static Connection* connect(
            Structure *from_structure, std::string from_layer_name,
            Structure *to_structure, std::string to_layer_name,
            ConnectionConfig config);


        /*******************************/
        /************ LAYERS ***********/
        /*******************************/
        void add_layer(LayerConfig config);
        void add_layer_from_image(std::string path, LayerConfig config);
        const LayerList& get_layers() const { return layers; }
        const ConnectionList& get_connections() const { return connections; }

        /*******************************/
        /********* CONNECTIONS *********/
        /*******************************/
        Connection* connect_layers(std::string from_layer_name,
            std::string to_layer_name, ConnectionConfig config);

        Connection* connect_layers_expected(
            std::string from_layer_name,
            LayerConfig layer_config, ConnectionConfig conn_config);

        Connection* connect_layers_matching(
            std::string from_layer_name,
            LayerConfig layer_config, ConnectionConfig conn_config);


        /* Dendritic internal connection functions */
        DendriticNode *spawn_dendritic_node(std::string to_layer_name);

        Connection* connect_layers_internal(
            DendriticNode *node, std::string from_layer_name,
            ConnectionConfig config);


        /* Adds a module of the given |type| for the given |layer| */
        void add_module(std::string layer, std::string type, std::string params);

        // Structure name
        const std::string name;

        // Stream type for iteration computation order
        const ClusterType cluster_type;

    private:
        /* Internal layer connection functions */
        Connection* connect_layers(
                Layer *from_layer, Layer *to_layer,
                ConnectionConfig config);

        Layer* find_layer(std::string name) {
            if (layers_by_name.find(name) != layers_by_name.end())
                return layers_by_name.find(name)->second;
            else return nullptr;
        }

        // Layers
        LayerList layers;
        std::map<std::string, Layer*> layers_by_name;

        // Connections
        ConnectionList connections;

        // Number of neurons
        int total_neurons;
        int num_neurons[sizeof(IOTypes)];

        // Flags for whether this contains layers of a given neural model
        std::vector<bool> neural_model_flags;
};

typedef std::vector<Structure*> StructureList;

#endif
