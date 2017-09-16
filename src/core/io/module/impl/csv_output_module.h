#ifndef csv_output_module_h
#define csv_output_module_h

#include "io/module/module.h"
#include "util/pointer.h"

class CSVOutputModule : public Module {
    public:
        CSVOutputModule(Layer *layer, ModuleConfig *config);
        virtual ~CSVOutputModule();

        void report_output(Buffer *buffer, OutputType output_type);
        virtual IOTypeMask get_type() { return OUTPUT; }

    MODULE_MEMBERS
};

#endif