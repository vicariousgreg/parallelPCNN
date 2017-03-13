#ifndef resource_manager.h
#define resource_manager.h

#include <thread>
#include <vector>

#include "util/parallel.h"

class Stream;
class Event;
class BasePointer;

typedef unsigned int DeviceID;

class ResourceManager {
        class Device;

    public:
        static ResourceManager *get_instance();
        virtual ~ResourceManager();

        unsigned int get_num_cores() { return num_cores; }
        unsigned int get_num_devices() { return devices.size(); }
        DeviceID get_host_id() { return devices.size()-1; }

        void* allocate_host(unsigned long count, int size);
        void* allocate_device(unsigned long count, int size,
            void* source_data, DeviceID device_id=0);

        void schedule_transfer(BasePointer* ptr, DeviceID device_id);
        void transfer();

        Stream *get_default_stream(DeviceID id);
        Stream *create_stream(DeviceID id);
        Event *create_event(DeviceID id);

    private:
        ResourceManager();
        static ResourceManager *instance;

        int num_cores;
        std::vector<Device*> devices;

        std::vector<unsigned long> scheduled_transfer_size;
        std::vector<std::vector<BasePointer*> > scheduled_transfers;

        class Device {
            public:
                Device(DeviceID device_id, bool host_flag);
                virtual ~Device();

                bool is_host() { return host_flag; }
                Stream *create_stream();
                Event *create_event();

                const DeviceID device_id;
                const bool host_flag;
                Stream* const default_stream;
                std::vector<Stream*> streams;
                std::vector<Event*> events;
        };
};

#endif