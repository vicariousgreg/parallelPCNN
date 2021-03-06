#ifndef csv_module_h
#define csv_module_h

#include "io/module.h"
#include "util/resources/pointer.h"

class CSVReaderModule : public Module {
    public:
        CSVReaderModule(LayerList layers, ModuleConfig *config);
        virtual ~CSVReaderModule();

        virtual size_t get_expected_iterations() const
            { return epochs * exposure * num_rows; }

        void cycle_impl();

    protected:
        std::string filename;
        int exposure;
        int epochs;
        int curr_row;
        int num_rows;

        Pointer<float> data;
        std::vector<Pointer<float>> pointers;

    MODULE_MEMBERS
};

class CSVInputModule : public CSVReaderModule {
    public:
        CSVInputModule(LayerList layers, ModuleConfig *config);

        void feed_input_impl(Buffer *buffer);

    MODULE_MEMBERS
};

class CSVExpectedModule : public CSVReaderModule {
    public:
        CSVExpectedModule(LayerList layers, ModuleConfig *config);

        void feed_input_impl(Buffer *buffer);

    MODULE_MEMBERS
};

class CSVOutputModule : public Module {
    public:
        CSVOutputModule(LayerList layers, ModuleConfig *config);
        virtual ~CSVOutputModule();

        void report_output_impl(Buffer *buffer);

    MODULE_MEMBERS
};

class CSVEvaluatorModule : public CSVExpectedModule {
    public:
        CSVEvaluatorModule(LayerList layers, ModuleConfig *config);

        void report_output_impl(Buffer *buffer);
        void report(Report *report);

    private:
        std::map<Layer*, int> correct;
        std::map<Layer*, float> total_SSE;

    MODULE_MEMBERS
};

#endif
