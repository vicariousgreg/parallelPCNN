#ifndef resource_manager.h
#define resource_manager.h

#include <thread>
#include <vector>
#include <set>
#include <map>

#include "util/parallel.h"
#include "util/constants.h"
#include "util/stream.h"
#include "util/event.h"

class BasePointer;

class ResourceManager {
    public:
        static ResourceManager *get_instance();
        virtual ~ResourceManager();

        /* Delete managed pointers */
        void flush();
        void flush_device();
        void flush_host();
        void flush(DeviceID device_id);

        /* Delete resources */
        void delete_streams();
        void delete_events();

        unsigned int get_num_cores() { return num_cores; }
        unsigned int get_num_devices() { return devices.size(); }
        const std::vector<DeviceID> get_active_devices();
        DeviceID get_host_id() { return devices.size()-1; }
        bool is_host(DeviceID device_id) { return device_id == get_host_id(); }

        void* allocate_host(size_t count, size_t size);
        void* allocate_device(size_t count, size_t size,
            void* source_data, DeviceID device_id=0);
        void drop_pointer(void* ptr, DeviceID device_id);

        BasePointer* transfer(DeviceID device_id,
            std::vector<BasePointer*> ptrs);

        Stream *get_default_stream(DeviceID id);
        Stream *get_inter_device_stream(DeviceID id);
        Stream *create_stream(DeviceID id);
        Event *create_event(DeviceID id);

        void halt_streams();

    private:
        ResourceManager();
        static ResourceManager *instance;

        class Device {
            public:
                Device(DeviceID device_id, bool host_flag, bool solo);
                virtual ~Device();

                bool is_host() { return host_flag; }
                Stream *create_stream();
                Event *create_event();

                void delete_streams();
                void delete_events();

                const DeviceID device_id;
                const bool host_flag;
                Stream* const default_stream;
                Stream* const inter_device_stream;
                std::vector<Stream*> streams;
                std::vector<Event*> events;
        };

        int num_cores;
        std::vector<Device*> devices;
        std::map<DeviceID, std::set<void*>> managed_pointers;
};

#endif
