#include "input.h"
#include "../implementation/random_input.h"
#include "../implementation/image_input.h"

Input* build_input(Layer *layer, std::string type, std::string params) {
    if (type == "random")
        return new RandomInput(layer, params);
    else if (type == "image")
        return new ImageInput(layer, params);
    else
        throw "Unrecognized input type!";
}