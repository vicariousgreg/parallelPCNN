#ifndef pointer_cpp
#define pointer_cpp

#ifndef pointer_h
#include "util/pointer.h"
#endif

#include <cstdlib>
#include <cstring>

#include "util/error_manager.h"
#include "util/resource_manager.h"
#include "util/tools.h"

template<typename T>
Pointer<T>::Pointer()
    : BasePointer(
          nullptr,
          0,
          sizeof(T),
          ResourceManager::get_instance()->get_host_id(),
          true,
          false,
          false) { }

template<typename T>
Pointer<T>::Pointer(unsigned long size)
    : BasePointer(
          ResourceManager::get_instance()->allocate_host(size, sizeof(T)),
          size,
          sizeof(T),
          ResourceManager::get_instance()->get_host_id(),
          true,
          false,
          true) { }

template<typename T>
Pointer<T>::Pointer(unsigned long size, T val)
        : Pointer<T>(size) {
    this->set(val, false);
}

template<typename T>
Pointer<T>::Pointer(T* ptr, unsigned long size, bool local, DeviceID device_id)
    : BasePointer(
          ptr,
          size,
          sizeof(T),
          device_id,
          local,
          false,
          false) { }

template<typename T>
template<typename S>
Pointer<S> Pointer<T>::cast() const {
    return Pointer<S>((S*)ptr, size, local, device_id);
}

template<typename T>
Pointer<T> Pointer<T>::slice(int offset, int new_size) const {
    return Pointer<T>(((T*)ptr) + offset, new_size, local, device_id);
}

template<typename T>
Pointer<T> Pointer<T>::pinned_pointer(unsigned long size) {
#ifdef __CUDACC__
    T* ptr;
    cudaMallocHost((void**) &ptr, size * sizeof(T));
    auto pointer = Pointer<T>(ptr, size, true,
        ResourceManager::get_instance()->get_host_id());
    pointer.owner = true;
    pointer.pinned = true;
    return pointer;
#else
    return Pointer<T>(size);
#endif
}

template<typename T>
Pointer<T> Pointer<T>::pinned_pointer(unsigned long size, T val) {
    auto pointer = Pointer<T>::pinned_pointer(size);
    pointer.set(val, false);
    return pointer;
}

template<typename T>
HOST DEVICE T* Pointer<T>::get(int offset) const {
#ifdef __CUDA_ARCH__
    if (local) assert(false);
    return ((T*)ptr) + offset;
#else
    if (not local)
        ErrorManager::get_instance()->log_error(
            "Attempted to dereference device pointer from host!");
    return ((T*)ptr) + offset;
#endif
}

template<typename T>
void Pointer<T>::copy_to(Pointer<T> dst) const {
    if (dst.size != this->size)
        ErrorManager::get_instance()->log_error(
            "Attempted to copy memory between pointers of different sizes!");
    if (local and dst.local)
        memcpy(dst.ptr, ptr, size * sizeof(T));
    else
        ErrorManager::get_instance()->log_error(
            "Non-local transfers must be handled by a Stream!");
}

template<typename T>
void Pointer<T>::copy_to(Pointer<T> dst, Stream *stream) const {
    if (dst.size != this->size)
        ErrorManager::get_instance()->log_error(
            "Attempted to copy memory between pointers of different sizes!");
#ifdef __CUDACC__
    if (this->local and dst.local) memcpy(dst.ptr, this->ptr, this->size * sizeof(T));
    else {
        if (stream->is_host())
            ErrorManager::get_instance()->log_error(
                "Attempted to copy memory between devices using host stream!");
        cudaSetDevice(stream->get_device_id());

        if (not this->local and not dst.local) {
            cudaMemcpyPeerAsync(dst.ptr, dst.device_id,
                this->ptr, this->device_id, this->size * sizeof(T),
                stream->get_cuda_stream());
        } else {
            auto kind = cudaMemcpyDeviceToHost;
            if (this->local) kind = cudaMemcpyHostToDevice;

            cudaMemcpyAsync(dst.ptr, this->ptr, this->size * sizeof(T),
                kind, stream->get_cuda_stream());
        }
    }
#else
    memcpy(dst.ptr, this->ptr, this->size * sizeof(T));
#endif
}

template<typename T>
void Pointer<T>::set(T val, bool async) {
    T* t_ptr = (T*)ptr;
    if (local) {
        for (int i = 0 ; i < size ; ++i) t_ptr[i] = val;
#ifdef __CUDACC__
    } else if (sizeof(T) == 1) {
        if (async) cudaMemsetAsync(ptr,val,size);
        else cudaMemset(ptr,val,size);
    } else if (sizeof(T) == 4) {
        if (async) cuMemsetD32Async((CUdeviceptr)ptr,val,size, 0);
        else cuMemsetD32((CUdeviceptr)ptr,val,size);
    } else {
        ErrorManager::get_instance()->log_error(
            "Attempted to set memory of non-primitive device array!");
#endif
    }
}

#endif