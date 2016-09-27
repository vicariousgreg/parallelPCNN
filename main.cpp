#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include "model.h"
#include "state.h"
#include "driver.h"
#include "tools.h"

static Timer timer = Timer();

Model* build_model() {
    /* Construct the model */
    Model *model = new Model("izhikevich");
    int size = 800 * 1;

    int pos = model->add_layer(size, "random positive");
    int neg = model->add_layer(size / 4, "random negative");
    model->connect_layers(pos, pos, true, 0, .5, FULLY_CONNECTED, ADD);
    model->connect_layers(pos, neg, true, 0, .5, FULLY_CONNECTED, ADD);
    model->connect_layers(neg, pos, true, 0, 1, FULLY_CONNECTED, SUB);
    model->connect_layers(neg, neg, true, 0, 1, FULLY_CONNECTED, SUB);

    return model;
}

int main(void) {
    // Seed random number generator
    srand(time(NULL));

    try {
        // Start timer
        timer.start();

        Model *model = build_model();
        printf("Built model.\n");
        printf("  - neurons     : %10d\n", model->num_neurons);
        printf("  - layers      : %10d\n", model->num_layers);
        printf("  - connections : %10d\n", model->num_connections);

        // Start timer
        timer.start();

        Driver *driver = build_driver(model);
        printf("Built state.\n");
        timer.stop("Initialization");

        timer.start();
        int iterations = 50;
        for (int i = 0 ; i < iterations ; ++i) {
            driver->state->randomize_input(0, 5);
            driver->state->randomize_input(1, 2);
            driver->timestep();
            driver->print_output();
        }

        float time = timer.stop("Total time");
        printf("Time averaged over %d iterations: %f\n", iterations, time/iterations);

    } catch (const char* msg) {
        printf("\n\nERROR: %s\n", msg);
        printf("Fatal error -- exiting...\n");
        return 1;
    }

    return 0;
}
