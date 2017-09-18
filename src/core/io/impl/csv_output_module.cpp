#include <cstdlib>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

#include "io/impl/csv_output_module.h"
#include "util/tools.h"
#include "util/error_manager.h"

REGISTER_MODULE(CSVOutputModule, "csv_output", OUTPUT);

CSVOutputModule::CSVOutputModule(Layer *layer, ModuleConfig *config)
        : Module(layer) { }

CSVOutputModule::~CSVOutputModule() { }

void CSVOutputModule::report_output(Buffer *buffer) {
    Output* output = buffer->get_output(this->layer);

    for (int row = 0 ; row < this->layer->rows; ++row) {
        for (int col = 0 ; col < this->layer->columns; ++col) {
            int index = (row * this->layer->columns) + col;

            float value;
            Output out_value = output[index];
            switch (output_type) {
                case FLOAT:
                    value = out_value.f;
                    break;
                case INT:
                    value = out_value.i;
                    break;
                case BIT:
                    value = out_value.i >> 31;
                    break;
            }
            if (row != 0 or col != 0) std::cout << ",";
            std::cout << value;
        }
    }
    std::cout << "\n";
}
