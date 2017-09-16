#ifndef csv_input_module_h
#define csv_input_module_h

#include "io/module/module.h"
#include "util/pointer.h"
#include "csvparser.h"

class CSVInputModule : public Module {
    public:
        CSVInputModule(Layer *layer, ModuleConfig *config);
        virtual ~CSVInputModule();

        void feed_input(Buffer *buffer);
        virtual IOTypeMask get_type() { return INPUT; }

    private:
        int age;
        int exposure;
        int curr_row;

        std::vector<Pointer<float> > data;

    MODULE_MEMBERS
};

#endif