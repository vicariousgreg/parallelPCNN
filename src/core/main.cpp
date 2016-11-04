#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include "model/model.h"
#include "state/state.h"
#include "util/tools.h"
#include "clock.h"

static Timer timer = Timer();

void print_model(Model *model) {
    printf("Built model.\n");
    printf("  - neurons     : %10d\n", model->num_neurons);
    printf("  - layers      : %10d\n", model->all_layers.size());
    printf("  - connections : %10d\n", model->connections.size());
    int num_weights = 0;
    for (int i = 0; i < model->connections.size() ; ++i)
        if (model->connections[i]->parent == NULL)
            num_weights += model->connections[i]->num_weights;
    printf("  - weights     : %10d\n", num_weights);
}

Model* build_self_connected_model(std::string engine_name) {
    Model *model = new Model(engine_name);
    Structure *structure = new Structure("Self-connected");

    int rows = 1000;
    int cols = 1000;
    structure->add_layer("a", rows, cols, "random positive");
    structure->connect_layers("a", "a", true, 0, .5, CONVOLUTIONAL, ADD, "7 1");

    structure->add_layer("b", rows, cols, "random positive");
    structure->connect_layers("b", "b", true, 0, .5, CONVOLUTIONAL, ADD, "7 1");

    // Modules
    structure->add_module("a", "random_input", "5");
    //structure->add_module("a", "dummy_output", "5");
    structure->add_module("b", "random_input", "5");
    //structure->add_module("b", "dummy_output", "5");

    model->add_structure(structure);
    return model;
}

Model* build_arborized_model(std::string engine_name, ConnectionType type) {
    Model *model = new Model(engine_name);
    Structure *structure = new Structure("Arborized");

    int rows = 1000;
    int cols = 1000;
    structure->add_layer("a", rows, cols, "random positive");
    structure->connect_layers_expected("a", "b", "random positive" , true, 0, .5, type, ADD, "7 1");

    // Modules
    structure->add_module("a", "random_input", "5");
    //structure->add_module("b", "dummy_output", "5");

    model->add_structure(structure);
    return model;
}

Model* build_stress_model(std::string engine_name) {
    Model *model = new Model(engine_name);
    Structure *structure = new Structure("Self-connected");

    int size = 800 * 19;
    structure->add_layer("pos", 1, size, "random positive");
    structure->add_layer("neg", 1, size / 4, "random negative");
    structure->connect_layers("pos", "pos", true, 0, .5, FULLY_CONNECTED, ADD, "");
    structure->connect_layers("pos", "neg", true, 0, .5, FULLY_CONNECTED, ADD, "");
    structure->connect_layers("neg", "pos", true, 0, 1, FULLY_CONNECTED, SUB, "");
    structure->connect_layers("neg", "neg", true, 0, 1, FULLY_CONNECTED, SUB, "");

    // Modules
    structure->add_module("pos", "random_input", "5");
    structure->add_module("neg", "random_input", "2");

    model->add_structure(structure);
    return model;
}

Model* build_layers_model(std::string engine_name) {
    Model *model = new Model(engine_name);
    Structure *structure = new Structure("Self-connected");

    int size = 100;
    structure->add_layer("a", size, size, "random positive");

    structure->add_layer("c", size, size, "random positive");
    structure->connect_layers("a", "c", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");
    structure->add_layer("d", size, size, "random positive");
    structure->connect_layers("a", "d", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");
    structure->add_layer("e", size, size, "random positive");
    structure->connect_layers("a", "e", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");

    structure->add_layer("f", size, size, "random positive");
    structure->connect_layers("c", "f", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");

    structure->add_layer("g", size, size, "random positive");
    structure->connect_layers("d", "g", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");
    structure->connect_layers("f", "g", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");

    structure->add_layer("b", size, size, "random positive");
    structure->connect_layers("f", "b", true, 0, 5, CONVOLUTIONAL, ADD, "5 1");

    // Modules
    structure->add_module("a", "random_input", "10");
    structure->add_module("f", "dummy_output", "");
    structure->add_module("b", "random_input", "10");
    //structure->add_module("b", "dummy_output", "");

    std::string output_name = "visualizer_output";
    structure->add_module("a", output_name, "");
    structure->add_module("b", output_name, "");
    structure->add_module("c", output_name, "");
    structure->add_module("d", output_name, "");
    structure->add_module("e", output_name, "");
    structure->add_module("f", output_name, "");
    structure->add_module("g", output_name, "");

    model->add_structure(structure);
    return model;
}

Model* build_image_model(std::string engine_name) {
    /* Determine output type */
    //std::string output_name = "print_output";
    std::string output_name = "visualizer_output";

    /* Construct the model */
    Model *model = new Model(engine_name);
    Structure *structure = new Structure("Self-connected");

    //const char* image_path = "resources/bird.jpg";
    const char* image_path = "resources/bird-head.jpg";
    //const char* image_path = "resources/pattern.jpg";
    //const char* image_path = "resources/bird-head-small.jpg";
    //const char* image_path = "resources/grid.png";
    structure->add_layer_from_image("photoreceptor", image_path, "default");

    // Vertical line detection
    structure->connect_layers_expected("photoreceptor", "vertical", "default",
        true, 0, 5, CONVOLUTIONAL, ADD,
        "11 1 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 "
        "-5 -5  0  0  5 10  5  0  0 -5 -5 ");
    structure->connect_layers("vertical", "vertical", true, 0, 5, CONVOLUTIONAL, ADD,
        "5 1 "
        "-5  0  5  0 -5 "
        "-5  0  5  0 -5 "
        "-5  0  5  0 -5 "
        "-5  0  5  0 -5 "
        "-5  0  5  0 -5 ");

    // Horizontal line detection
    structure->connect_layers_expected("photoreceptor", "horizontal", "default",
        true, 0, 5, CONVOLUTIONAL, ADD,
        "11 1 "
        "-5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 "
        "-5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 "
        " 0  0  0  0  0  0  0  0  0  0  0 "
        " 0  0  0  0  0  0  0  0  0  0  0 "
        " 5  5  5  5  5  5  5  5  5  5  5 "
        "10 10 10 10 10 10 10 10 10 10 10 "
        " 5  5  5  5  5  5  5  5  5  5  5 "
        " 0  0  0  0  0  0  0  0  0  0  0 "
        " 0  0  0  0  0  0  0  0  0  0  0 "
        "-5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 "
        "-5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 ");
    structure->connect_layers("horizontal", "horizontal", true, 0, 5, CONVOLUTIONAL, ADD,
        "5 1 "
        "-5 -5 -5 -5 -5 "
        " 0  0  0  0  0 "
        " 5  5  5  5  5 "
        " 0  0  0  0  0 "
        "-5 -5 -5 -5 -5 ");

    // Cross detection
    structure->connect_layers_expected("vertical", "cross", "default",
        true, 0, 5, CONVOLUTIONAL, ADD,
        "5 1 "
        "-5  0 10  0 -5 "
        "-5  0 10  0 -5 "
        "-5  0 10  0 -5 "
        "-5  0 10  0 -5 "
        "-5  0 10  0 -5 ");
    structure->connect_layers("horizontal", "cross", true, 0, 5, CONVOLUTIONAL, ADD,
        "5 1 "
        "-5 -5 -5 -5 -5 "
        " 0  0  0  0  0 "
        "10 10 10 10 10 "
        " 0  0  0  0  0 "
        "-5 -5 -5 -5 -5 ");

    // Forward slash
    structure->connect_layers_expected("photoreceptor", "forward_slash", "default",
        true, 0, 5, CONVOLUTIONAL, ADD,
        "9 1 "
        " 0  0  0  0 -5 -5  0  5 10 "
        " 0  0  0 -5 -5  0  5 10  5 "
        " 0  0 -5 -5  0  5 10  5  0 "
        " 0 -5 -5  0  5 10  5  0 -5 "
        "-5 -5  0  5 10  5  0 -5 -5 "
        "-5  0  5 10  5  0 -5 -5  0 "
        " 0  5 10  5  0 -5 -5  0  0 "
        " 5 10  5  0 -5 -5  0  0  0 "
        "10  5  0 -5 -5  0  0  0  0 ");
    structure->connect_layers("forward_slash", "forward_slash", true, 0, 5, CONVOLUTIONAL, ADD,
        "5 1 "
        " 0  0 -5  0  5 "
        " 0 -5  0  5  0 "
        "-5  0  5  0 -5 "
        " 0  5  0 -5  0 "
        " 5  0 -5  0  0 ");

    // Back slash
    structure->connect_layers_expected("photoreceptor", "back_slash", "default",
        true, 0, 5, CONVOLUTIONAL, ADD,
        "9 1 "
        "10  5  0 -5 -5  0  0  0  0 "
        " 5 10  5  0 -5 -5  0  0  0 "
        " 0  5 10  5  0 -5 -5  0  0 "
        "-5  0  5 10  5  0 -5 -5  0 "
        "-5 -5  0  5 10  5  0 -5 -5 "
        " 0 -5 -5  0  5 10  5  0 -5 "
        " 0  0 -5 -5  0  5 10  5  0 "
        " 0  0  0 -5 -5  0  5 10  5 "
        " 0  0  0  0 -5 -5  0  5 10 ");
    structure->connect_layers("back_slash", "back_slash", true, 0, 5, CONVOLUTIONAL, ADD,
        "5 1 "
        " 5  0 -5  0  0 "
        " 0  5  0 -5  0 "
        "-5  0  5  0 -5 "
        " 0 -5  0  5  0 "
        " 0  0 -5  0  5 ");

    // Modules
    structure->add_module("photoreceptor", "image_input", image_path);
    structure->add_module("photoreceptor", output_name, "8");
    structure->add_module("vertical", output_name, "8");
    structure->add_module("horizontal", output_name, "8");
    structure->add_module("forward_slash", output_name, "8");
    structure->add_module("back_slash", output_name, "8");
    //structure->add_module("cross", output_name, "8");

    model->add_structure(structure);
    return model;
}

Model* build_reentrant_image_model(std::string engine_name) {
    /* Determine output type */
    //std::string output_name = "print_output";
    std::string output_name = "visualizer_output";

    /* Construct the model */
    Model *model = new Model(engine_name);
    Structure *structure = new Structure("Self-connected");

    //const char* image_path = "resources/bird.jpg";
    const char* image_path = "resources/bird-head.jpg";
    //const char* image_path = "resources/pattern.jpg";
    //const char* image_path = "resources/bird-head-small.jpg";
    //const char* image_path = "resources/grid.png";
    structure->add_layer_from_image("photoreceptor", image_path, "default");

    // Connect first layer to receptor
    structure->connect_layers_matching("photoreceptor", "layer1", "default",
        true, 0, 1, CONVOLUTIONAL, ADD, "21 1 1");

    // Create reentrant pair
    structure->connect_layers_matching("layer1", "layer2", "default",
        true, 0, 1, CONVOLUTIONAL, ADD, "9 1 1");
    structure->connect_layers("layer2", "layer1",
        true, 0, 1, CONVOLUTIONAL, ADD, "9 1 1");

    // Inhibitory self connections
    structure->connect_layers("layer1", "layer1",
        true, 0, 1, CONVOLUTIONAL, SUB, "5 1 10");

    structure->connect_layers("layer2", "layer2",
        true, 0, 1, CONVOLUTIONAL, SUB, "5 1 10");

    // Modules
    structure->add_module("photoreceptor", "image_input", image_path);
    structure->add_module("photoreceptor", output_name, "8");
    structure->add_module("layer1", output_name, "8");
    structure->add_module("layer2", output_name, "8");
    // Add random driver to second layer
    structure->add_module("layer2", "random_input", "5");

    model->add_structure(structure);
    return model;
}

void run_simulation(Model *model, int iterations, bool verbose) {
    Clock clock(10);
    //Clock clock;  // No refresh rate synchronization
    //clock.run(model, iterations, 8, verbose);
    clock.run(model, iterations, 1, verbose);
}

void stress_test() {
    Model *model;

    std::cout << "Stress...\n";
    model = build_stress_model("izhikevich");
    print_model(model);
    run_simulation(model, 10, true);
    std::cout << "\n";

    delete model;
}

void layers_test() {
    Model *model;

    std::cout << "Layers...\n";
    model = build_layers_model("izhikevich");
    //model = build_layers_model("rate_encoding");
    print_model(model);
    run_simulation(model, 100000, true);
    std::cout << "\n";

    delete model;
}

void reentrant_image_test() {
    Model *model;

    std::cout << "Image...\n";
    model = build_reentrant_image_model("izhikevich");
    //model = build_image_model("rate_encoding", true);
    print_model(model);
    run_simulation(model, 10000, true);
    //run_simulation(model, 100, true);
    //run_simulation(model, 10, true);
    std::cout << "\n";

    delete model;
}

void image_test() {
    Model *model;

    std::cout << "Image...\n";
    model = build_image_model("izhikevich");
    //model = build_image_model("rate_encoding", true);
    print_model(model);
    run_simulation(model, 10000, true);
    //run_simulation(model, 100, true);
    //run_simulation(model, 10, true);
    std::cout << "\n";

    delete model;
}

void varied_test() {
    Model *model;

    std::cout << "Self connected...\n";
    model = build_self_connected_model("izhikevich");
    print_model(model);
    run_simulation(model, 50, true);
    std::cout << "\n";
    delete model;

    std::cout << "Convergent...\n";
    model = build_arborized_model("izhikevich", CONVERGENT);
    print_model(model);
    run_simulation(model, 50, true);
    std::cout << "\n";
    delete model;

    std::cout << "Convergent convolutional...\n";
    model = build_arborized_model("izhikevich", CONVOLUTIONAL);
    print_model(model);
    run_simulation(model, 50, true);
    std::cout << "\n";
    delete model;
}

int main(int argc, char *argv[]) {
    // Seed random number generator
    srand(time(NULL));

    try {
        //stress_test();
        //layers_test();
        //image_test();
        reentrant_image_test();
        //varied_test();

        return 0;
    } catch (const char* msg) {
        printf("\n\nERROR: %s\n", msg);
        printf("Fatal error -- exiting...\n");
        return 1;
    }
}