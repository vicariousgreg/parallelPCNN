#ifndef dsst_output_module_h
#define dsst_output_module_h

#include "io/module/module.h"
#include "dsst.h"

class DSSTOutputModule : public Module {
    public:
        DSSTOutputModule(Layer *layer, ModuleConfig *config);

        void report_output(Buffer *buffer, OutputType output_type);
        virtual IOTypeMask get_type() { return OUTPUT; }

    private:
        DSST* dsst;

    MODULE_MEMBERS
};

#endif