#ifndef kernel_h
#define kernel_h

#include <functional>

#include "engine/kernel/synapse_data.h"
#include "engine/kernel/aggregator.h"
#include "util/resources/stream.h"
#include "util/resources/pointer.h"

class Connection;

template<typename... ARGS>
class Kernel {
    public:
        /* Null kernel */
        Kernel();

        /* Use this constructor to create kernels that run on the host,
         *   regardless of the stream device.  This is useful for transfer
         *   instructions, which are initiated by the host. */
        Kernel(void(*serial_kernel)(ARGS...), bool run_all_serial);

        /* Typical constructor.  Provide serial and parallel versions. */
        Kernel(void(*serial_kernel)(ARGS...),
               void (*parallel_kernel)(ARGS...)=nullptr);

        /* Checks if kernel is null (no serial kernel */
        bool is_null() { return serial_kernel == nullptr; }


        /* Serial/Parallel agnostic functions */
        void run(Stream *stream, dim3 blocks, dim3 threads, ARGS... args);
        void schedule(Stream *stream, dim3 blocks, dim3 threads, ARGS... args);


        /* Serial Functions */
        void run_serial(ARGS... args);
        void serial_wrapper(void(*f)(ARGS...), ARGS... args);
        void schedule_serial(Stream *stream, ARGS... args);


        /* Parallel Functions */
        void run_parallel(Stream *stream, dim3 blocks, dim3 threads,
                ARGS... args);
        void parallel_wrapper(Stream *stream, dim3 blocks, dim3 threads,
                void(*f)(ARGS...), ARGS... args);
        void schedule_parallel(Stream *stream, dim3 blocks, dim3 threads,
                ARGS... args);

    protected:
        bool run_all_serial;
        void (*serial_kernel)(ARGS...);
        void (*parallel_kernel)(ARGS...);
};

template<typename... ARGS>
using KernelList = std::vector<Kernel<ARGS...>>;


/* Sets input data (use val=0.0 for clear) */
Kernel<float, Pointer<float>, int, bool> get_set_data();

/* Randomizes input data */
Kernel<Pointer<float>, int, float, float, bool> get_randomize_data_uniform();
Kernel<Pointer<float>, int, float, float, bool> get_randomize_data_normal();
Kernel<Pointer<float>, int, float, float, bool, Pointer<float>>
    get_randomize_data_poisson();

/* Dendritic tree internal computation */
Kernel<int, Pointer<float>, Pointer<float>, AGGREGATOR, float>
    get_calc_internal();
Kernel<int, int, Pointer<float>, Pointer<float>, float>
    get_calc_internal_second_order();

/* Base activator kernel */
Kernel<SYNAPSE_ARGS> get_base_activator_kernel(Connection *conn);

/* Wraps a pointer copying operation
 * Sets run_all_serial to true,
 *   since device transfers are initiated by the host. */
template<typename T>
inline Kernel<Pointer<T>, Pointer<T>, Stream*> get_copy_pointer_kernel() {
    return Kernel<Pointer<T>, Pointer<T>, Stream*>(copy_pointer, true);
}

/* Transposition kernel */
Kernel<const Pointer<float>, Pointer<float>,
    const int, const int> get_transposer();

/******************************************************************************/
/************************* Template Implementations ***************************/
/******************************************************************************/

/* Null kernel */
template<typename... ARGS>
Kernel<ARGS...>::Kernel()
    : serial_kernel(nullptr),
      parallel_kernel(nullptr),
      run_all_serial(false) { }

/* Use this constructor to create kernels that run on the host,
 *   regardless of the stream device.  This is useful for transfer
 *   instructions, which are initiated by the host. */
template<typename... ARGS>
Kernel<ARGS...>::Kernel(void(*serial_kernel)(ARGS...), bool run_all_serial)
        : serial_kernel(serial_kernel),
          parallel_kernel(nullptr),
          run_all_serial(run_all_serial) { }

/* Typical constructor.  Provide serial and parallel versions. */
template<typename... ARGS>
Kernel<ARGS...>::Kernel(void(*serial_kernel)(ARGS...),
       void (*parallel_kernel)(ARGS...))
        : serial_kernel(serial_kernel),
          parallel_kernel(parallel_kernel),
          run_all_serial(false) { }

/* Serial/Parallel agnostic functions */
template<typename... ARGS>
void Kernel<ARGS...>::run(Stream *stream,
        dim3 blocks, dim3 threads, ARGS... args) {
    if (run_all_serial or stream->is_host())
        run_serial(stream, args...);
    else
        run_parallel(stream, blocks, threads, args...);
}

template<typename... ARGS>
void Kernel<ARGS...>::schedule(Stream *stream,
        dim3 blocks, dim3 threads, ARGS... args) {
    if (run_all_serial or stream->is_host())
        schedule_serial(stream, args...);
    else
        schedule_parallel(stream, blocks, threads, args...);
}


/* Serial Functions */
template<typename... ARGS>
void Kernel<ARGS...>::run_serial(ARGS... args) {
    serial_kernel(args...);
}

template<typename... ARGS>
void Kernel<ARGS...>::serial_wrapper(void(*f)(ARGS...), ARGS... args) {
    f(args...);
}

template<typename... ARGS>
void Kernel<ARGS...>::schedule_serial(Stream *stream, ARGS... args) {
    if (serial_kernel == nullptr)
        LOG_ERROR(
            "Attempted to run nullptr kernel!");
    else
        stream->schedule(
            std::bind(&Kernel<ARGS...>::serial_wrapper,
                this, serial_kernel, args...));
}


#ifdef __CUDACC__
/* Implemented Parallel Functions */
template<typename... ARGS>
void Kernel<ARGS...>::run_parallel(Stream *stream, dim3 blocks, dim3 threads,
        ARGS... args) {
    cudaSetDevice(stream->get_device_id());
    parallel_kernel<<<blocks, threads, 0, stream->get_cuda_stream()>>>(args...);
}

template<typename... ARGS>
void Kernel<ARGS...>::parallel_wrapper(Stream *stream,
        dim3 blocks, dim3 threads, void(*f)(ARGS...), ARGS... args) {
    cudaSetDevice(stream->get_device_id());
    f<<<blocks, threads, 0, stream->get_cuda_stream()>>>(args...);
}

template<typename... ARGS>
void Kernel<ARGS...>::schedule_parallel(Stream *stream,
        dim3 blocks, dim3 threads, ARGS... args) {
    if (parallel_kernel == nullptr)
        LOG_ERROR(
            "Attempted to run nullptr kernel!");
    else
        stream->schedule(
            std::bind(&Kernel<ARGS...>::parallel_wrapper,
                this, stream, blocks, threads, parallel_kernel, args...));
}
#else
/* Dummy Parallel Functions (for serial builds) */
template<typename... ARGS>
void Kernel<ARGS...>::run_parallel(Stream *stream,
        dim3 blocks, dim3 threads, ARGS... args) {
    LOG_ERROR(
        "Attempted to run parallel kernel on serial build!");
}

template<typename... ARGS>
void Kernel<ARGS...>::schedule_parallel(Stream *stream,
        dim3 blocks, dim3 threads, ARGS... args) {
    LOG_ERROR(
        "Attempted to run parallel kernel on serial build!");
}
#endif

#endif
