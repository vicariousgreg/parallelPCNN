#include "io/output.h"
#include "io/spike_print_output.h"
#include "io/float_print_output.h"

Output* build_output(Layer *layer, std::string type, std::string params) {
    if (type == "print_spike")
        return new SpikePrintOutput(layer, params);
    else if (type == "print_float")
        return new FloatPrintOutput(layer, params);
    else
        throw "Unrecognized output type!";
}