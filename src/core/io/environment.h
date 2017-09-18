#ifndef environment_h
#define environment_h

#include <map>
#include <vector>

#include "io/module.h"

class Environment {
    public:
        Environment() { }
        virtual ~Environment() { remove_modules(); }

        /* Save or load environment to/from JSON file */
        static Environment* load(std::string path);
        void save(std::string path);

        /* Add or remove modules */
        void add_module(ModuleConfig* config);
        void remove_modules();
        void remove_modules(std::string structure,
            std::string layer="", std::string type="");

        /* Getters */
        const std::vector<ModuleConfig*>& get_modules() { return config_list; }

    private:
        std::map<std::string,
            std::map<std::string,
                std::vector<ModuleConfig*>>> config_map;
        std::vector<ModuleConfig*> config_list;
};

#endif
