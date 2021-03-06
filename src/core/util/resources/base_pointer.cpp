#include <cstdlib>
#include <cstring>

#include "util/resources/pointer.h"
#include "util/logger.h"
#include "util/resources/resource_manager.h"

BasePointer::BasePointer(PointerKey key)
        : type(std::type_index(typeid(char))),
          ptr(ResourceManager::get_instance()->allocate_host(key.bytes, 1)),
          size(key.bytes),
          unit_size(1),
          device_id(ResourceManager::get_instance()->get_host_id()),
          local(true),
          pinned(false),
          owner(true) {
    if (ptr != nullptr)
        ResourceManager::get_instance()
            ->increment_pointer_count(ptr, device_id);
}

void BasePointer::give_to(BasePointer* other) {
    if (not other->is_null())
        other->free();

    other->ptr = this->ptr;
    other->size = this->get_bytes() / other->unit_size;
    other->local = this->local;
    other->pinned = this->pinned;
    other->owner = this->owner;
    this->owner = false;
}

BasePointer::BasePointer(std::type_index type, void* ptr,
    size_t size, size_t unit_size,
    DeviceID device_id, bool owner)
        : type(type),
          ptr(ptr),
          size(size),
          unit_size(unit_size),
          device_id(device_id),
          pinned(false),
          owner(owner) {
    this->local =
        device_id == ResourceManager::get_instance()->get_host_id();
    if (ptr != nullptr)
        ResourceManager::get_instance()
            ->increment_pointer_count(ptr, device_id);
}

BasePointer::~BasePointer() {
    if (ptr != nullptr)
        ResourceManager::get_instance()
            ->decrement_pointer_count(ptr, device_id);
}

void BasePointer::free() {
    if (owner and size > 0)
        free_pointer(ptr, device_id, pinned);

    this->ptr = nullptr;
    this->size = 0;
    this->owner = false;
}

void free_pointer(void* ptr, DeviceID device_id, bool pinned) {
    auto res_man = ResourceManager::get_instance();
    bool local = device_id == res_man->get_host_id();

#ifdef __CUDACC__
    if (local) {
        if (pinned) cudaFreeHost(ptr); // cuda pinned memory
        else std::free(ptr);           // unpinned host memory
    } else {
        cudaSetDevice(device_id);
        cudaFree(ptr);           // cuda device memory
        device_check_error(nullptr);
    }
#else
    if (local) std::free(ptr);         // unpinned host memory (default)
#endif

    res_man->drop_pointer(ptr, device_id);
}

BasePointer* BasePointer::slice(size_t offset, size_t new_size) const {
    auto pointer = new BasePointer(
        type,
        ((char*)ptr) + (offset * unit_size),
        new_size,
        unit_size,
        device_id,
        false);
    pointer->pinned = this->pinned;
    return pointer;
}

void BasePointer::transfer(DeviceID new_device, void* destination,
        bool transfer_ownership) {
    // If nullptr, don't do anything
    if (ptr == nullptr or size == 0) return;

    // Transfer data
    destination = transfer_pointer(
        ptr, destination,
        size * unit_size,
        device_id, new_device);

    auto res_man = ResourceManager::get_instance();

    // Save size (it's reset in free())
    size_t new_size = this->size;

    // Free old data
    // This will decrement old pointer count
    this->free();

    // Update data
    this->ptr = destination;
    this->owner = transfer_ownership;
    this->device_id = new_device;
    this->local = (new_device == res_man->get_host_id());
    this->size = new_size;

    // Increment new pointer count
    if (destination != nullptr)
        res_man->increment_pointer_count(destination, device_id);
}

void* transfer_pointer(void* ptr, void* destination, size_t bytes,
        DeviceID old_device, DeviceID new_device) {
    DeviceID host_id = ResourceManager::get_instance()->get_host_id();
    bool host_dest = (new_device == host_id);
    bool local = (old_device == host_id);

    if (destination == nullptr) {
        if (host_dest)
            destination = (void*)ResourceManager::get_instance()
                ->allocate_host(1, bytes);
        else
            destination = (void*)ResourceManager::get_instance()
                ->allocate_device(1, bytes, nullptr, new_device);
    }

#ifdef __CUDACC__
    if (local) {
        if (host_dest)
            memcpy(destination, ptr, bytes);
        else {
            cudaSetDevice(new_device);
            cudaMemcpy(destination, ptr, bytes, cudaMemcpyHostToDevice);
        }
    } else {
        cudaSetDevice(old_device);
        if (host_dest)
            cudaMemcpy(destination, ptr, bytes, cudaMemcpyDeviceToHost);
        else
            cudaMemcpyPeer(destination, new_device, ptr, old_device, bytes);
    }
#else
    memcpy(destination, ptr, bytes);
#endif

    return destination;
}

void BasePointer::copy_to(BasePointer* other) {
    if (other->size != this->size or other->unit_size != this->unit_size)
        LOG_ERROR(
            "Attempted to copy memory between pointers of different sizes!");

    if (this->local and other->local)
        memcpy(other->ptr, this->ptr, this->size * this->unit_size);
    else
        LOG_ERROR(
            "Attempted to copy memory between base pointers "
            "that aren't on the host!");
}

void BasePointer::resize(size_t new_size) {
    // If new size is zero, decrement pointer count, free, and return
    if (new_size == 0) {
        if (ptr != nullptr)
            ResourceManager::get_instance()
                ->decrement_pointer_count(ptr, device_id);
        this->free();
        return;
    }

    // Allocate new space, copy as much as possible
    auto res_man = ResourceManager::get_instance();

    void* new_ptr;
    if (local) {
        new_ptr = res_man->allocate_host(new_size, unit_size);
        if (this->size > 0)
            memcpy(new_ptr, this->ptr,
                MIN(this->size, new_size) * this->unit_size);
    }
#ifdef __CUDACC__
    else {
        new_ptr = res_man->allocate_device(
            new_size, unit_size, nullptr, device_id);
        cudaSetDevice(device_id);
        if (this->size > 0)
            cudaMemcpy(new_ptr, this->ptr,
                MIN(this->size, new_size) * this->unit_size,
                cudaMemcpyDeviceToDevice);
    }
#endif

    // Decrement old pointer count
    if (ptr != nullptr)
        ResourceManager::get_instance()
            ->decrement_pointer_count(ptr, device_id);

    // Free old data
    this->free();

    // Increment new pointer count
    if (ptr != nullptr)
        ResourceManager::get_instance()
            ->increment_pointer_count(ptr, device_id);

    // Update data
    this->ptr = new_ptr;
    this->size = new_size;
    this->owner = true;
}
