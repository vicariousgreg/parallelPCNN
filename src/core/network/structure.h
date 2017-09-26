#ifndef structure_h
#define structure_h

#include <map>
#include <set>
#include <vector>
#include <string>

#include "util/constants.h"
#include "network/layer.h"
#include "network/connection.h"
#include "util/property_config.h"

/* Represents a neural structure in a network model.
 * Contains network graph data and parameters for layers/connections.
 *
 * Each structure is assigned a cluster in the driver that determines the
 *     sequence of events that happen to its state (ie whether learning happens
 *     netween layer activations or in a batch at the end of the timestep).
 *     The type of cluster is determined by the ClusterType passed into the
 *     constructor.
 * Note that not all neural models are supported by each cluster type.  This is
 *     checked for in Attributes::check_compatibility() when the cluster is
 *     constructed.
 *
 * Layers and connections can be created using a variety of functions.
 */
class Structure {
    public:
        Structure(std::string name, ClusterType cluster_type=PARALLEL);
        virtual ~Structure();

        /* Checks whether this structure contains a layer
         *     of the given neural_model */
        bool contains(std::string neural_model) {
            return neural_model_flags.count(neural_model);
        }

        /* Gets the total neuron count */
        int get_num_neurons() const { return total_neurons; }


        /*******************************/
        /************ LAYERS ***********/
        /*******************************/
        const LayerList& get_layers() const { return layers; }
        Layer* add_layer(LayerConfig *config);
        Layer* add_layer_from_image(std::string path, LayerConfig *config);

        /*******************************/
        /********* CONNECTIONS *********/
        /*******************************/
        const ConnectionList& get_connections() const { return connections; }

        /* Connects layers in two different structures */
        static Connection* connect(
            Structure *from_structure, std::string from_layer_name,
            Structure *to_structure, std::string to_layer_name,
            ConnectionConfig *config,
            std::string node="root");

        Connection* connect_layers(
            std::string from_layer_name,
            std::string to_layer_name,
            ConnectionConfig *config,
            std::string node="root");

        Connection* connect_layers_expected(
            std::string from_layer_name,
            LayerConfig *layer_config,
            ConnectionConfig *conn_config,
            std::string node="root");

        Connection* connect_layers_matching(
            std::string from_layer_name,
            LayerConfig *layer_config,
            ConnectionConfig *conn_config,
            std::string node="root");

        /*****************************/
        /********* DENDRITES *********/
        /*****************************/
        void set_second_order(std::string to_layer_name,
            std::string dendrite_name);

        void create_dendritic_node(std::string to_layer_name,
            std::string parent_node_name, std::string child_node_name);

        std::string get_parent_node_name(Connection *conn) const;

        // Structure name
        const std::string name;

        // Stream type for iteration computation order
        const ClusterType cluster_type;

        std::string str() const { return "[Structure: " + name + "]"; }

        /* Find a layer
         * If not found, logs an error or returns nullptr */
        Layer* get_layer(std::string name, bool log_error=true);

    private:
        /* Internal layer connection functions */
        Connection* connect_layers(
                Layer *from_layer, Layer *to_layer,
                ConnectionConfig *config,
                std::string node="root");

        // Layers
        LayerList layers;
        std::map<std::string, Layer*> layers_by_name;

        // Connections
        ConnectionList connections;

        // Number of neurons
        int total_neurons;

        std::set<std::string> neural_model_flags;
};

typedef std::vector<Structure*> StructureList;

#endif