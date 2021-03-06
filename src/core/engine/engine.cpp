#include <cfloat>
#include <csignal>

#include "engine/engine.h"
#include "engine/instruction.h"
#include "engine/cluster/cluster.h"
#include "network/network.h"
#include "io/buffer.h"
#include "io/environment.h"
#include "state/state.h"
#include "state/attributes.h"
#include "report.h"
#include "gui_controller.h"
#include "mpi_wrap.h"


/******************/
/* Interrupt code */
/******************/

/* Wrapper with the correct signature for signal() */
static void handle_interrupt(int param) {
    Engine::interrupt();
}

/* Keep thread-safe signal flag */
std::mutex Engine::global_engine_lock;
bool Engine::interrupt_signaled = false;
bool Engine::interrupt_from_gui = false;
bool Engine::running = false;

/* Signals interrupt to all active engines */
void Engine::interrupt(bool from_gui) {
    {
        std::unique_lock<std::mutex> lock(global_engine_lock);

        // Avoid double signalling
        if (Engine::interrupt_signaled or not Engine::running) return;
        Engine::interrupt_signaled = true;
        Engine::interrupt_from_gui = from_gui;
    }
}


Engine::Engine(Context context)
        : context(context),
          learning_flag(true),
          suppress_output(false),
          refresh_rate(FLT_MAX),
          time_limit(0),
          environment_rate(1),
          iterations(0),
          verbose(false),
          buffer(nullptr),
          report(nullptr),
          multithreaded(false) { }

void Engine::build_environment(PropertyConfig args) {
    if (context.environment == nullptr) return;

    /* Keep track of input modules by layer to check for coactivity conflicts */
    std::map<Layer*, std::vector<Module*>> input_modules;

    /* Build environmental buffer */
    LayerList input_layers, output_layers;

    // Process each module config...
    for (auto config : context.environment->get_modules()) {
        // Skip if necessary
        if (config->get_bool("skip", false)) continue;

        // Build module
        Module *module = Module::build_module(
            context.network, new ModuleConfig(config));

        // If module has no layers, it will not be built, so skip it
        if (module == nullptr) continue;

        modules.push_back(module);

        // Update io_types for all layers attached to the module
        for (auto layer : module->layers) {
            auto layer_io_type = io_types[layer];
            auto module_io_type = module->get_io_type(layer);

            if (module_io_type == 0)
                LOG_ERROR(
                    "Error in environment model:\n"
                    "  Error adding module " + config->get("type") +
                    "to: " + layer->str() + "\n" +
                    "    Module has zero IO type for layer!");

            // Check for conflicting input modules
            if (module_io_type & INPUT & layer_io_type) {
                for (auto other : input_modules[layer])
                    if (module->is_coactive(other))
                        LOG_ERROR(
                            "Error in environment model:\n"
                            "  Error adding module " + config->get("type") +
                            "to: " + layer->str() + "\n" +
                            "    Layer has conflicting input modules!");
                input_modules[layer].push_back(module);
            }

            this->io_types[layer] = layer_io_type | module_io_type;

            // Track any auxiliary data used as input or output
            for (auto key : module->get_input_keys(layer))
                input_keys[layer].insert(key);
            for (auto key : module->get_output_keys(layer))
                output_keys[layer].insert(key);
        }
    }

    // Put layers in appropriate lists
    for (auto pair : io_types) {
        if (pair.second & INPUT)    input_layers.push_back(pair.first);
        if (pair.second & OUTPUT)   output_layers.push_back(pair.first);
    }

    // Construct buffer
    buffer = build_buffer(
        ResourceManager::get_instance()->get_host_id(),
            input_layers, output_layers,
            input_keys, output_keys);
}

void Engine::build_clusters(PropertyConfig args) {
    /* Build clusters */
    auto state = context.state;

    // Create the clusters and gather their nodes
    for (auto& structure : state->network->get_structures()) {
        auto cluster = build_cluster(
            structure, state, this, args);
        clusters.push_back(cluster);
        for (auto& node : cluster->get_nodes())
            cluster_nodes[node->to_layer] = node;
    }

    // Add external dependencies to the nodes
    for (auto& cluster : clusters)
        cluster->add_external_dependencies(cluster_nodes);

    // Process inter-device instructions
    for (auto& cluster : clusters) {
        for (auto& node : cluster->get_nodes()) {
            for (auto& syn_inst : node->get_synapse_activate_instructions()) {
                auto conn = syn_inst->connection;

                // If inter-device, find or create
                //   corresponding transfer instruction
                if (state->is_inter_device(conn)) {
                    InterDeviceTransferInstruction *inst = nullptr;

                    // Search for existing instruction
                    for (auto inter_inst : this->inter_device_transfers)
                        if (inter_inst->matches(conn, state))
                            inst = inter_inst;

                    // Create if doesn't exist
                    // Clusters are responsible for handling these transfers,
                    //   since different types handle them differently
                    bool new_transfer = (inst == nullptr);
                    if (new_transfer) {
                        inst = new InterDeviceTransferInstruction(conn, state);
                        this->inter_device_transfers.push_back(inst);
                    }
                    cluster->add_inter_device_instruction(
                        conn, syn_inst, inst, new_transfer);
                }
            }
        }
    }
}

void Engine::rebuild(PropertyConfig args) {
    clear();
    build_environment(args);
    build_clusters(args);
}

void Engine::clear() {
    // Clear clusters
    for (auto cluster : clusters) delete cluster;
    clusters.clear();
    for (auto& inst : inter_device_transfers) delete inst;
    inter_device_transfers.clear();

    // Clear buffer
    if (buffer != nullptr) {
        delete buffer;
        buffer = nullptr;
    }

    // Clear modules and IO types
    for (auto module : modules) delete module;
    modules.clear();
    io_types.clear();
    input_keys.clear();
    output_keys.clear();

    // Clear resources
    ResourceManager::get_instance()->delete_streams();
    ResourceManager::get_instance()->delete_events();
}

Engine::~Engine() {
    clear();
}

size_t Engine::get_buffer_bytes() const {
    size_t size = 0;
    if (buffer != nullptr)
        for (auto ptr : buffer->get_pointers())
            size += ptr->get_bytes();
    return size;
}


/****************/
/* Thread Mains */
/****************/

/* Launches network and environment computations */
void Engine::single_thread_loop() {
    // Synchronize MPI processes, if MPI is enabled (no-op otherwise)
    mpi_wrap_barrier();
    run_timer.reset();

    for (size_t i = 0 ; iterations == 0 or i < iterations; ++i) {
        // Reset iteration timer
        if (time_limit > 0) iteration_timer.reset();

        /*************************************/
        /*** Launch pre-input computations ***/
        /*************************************/
        for (auto& c : clusters) c->launch_pre_input_calculations();

        /**************************/
        /*** Read sensory input ***/
        /**************************/
        for (auto& c : clusters) c->wait_for_input();
        for (auto& m : modules)  m->feed_input(buffer);
        for (auto& c : clusters) c->launch_input();

        /***************************************/
        /*** Perform post-input computations ***/
        /***************************************/
        for (auto& c : clusters) c->launch_post_input_calculations();
        for (auto& c : clusters) c->launch_state_update();

        if (learning_flag)
            for (auto& c : clusters) c->launch_weight_update();

        /**************************/
        /*** Write motor output ***/
        /**************************/
        if (i % environment_rate == 0) {
            // Stream output
            if (not suppress_output) {
                for (auto& c : clusters) c->launch_output();
                for (auto& c : clusters) c->wait_for_output();
                for (auto& m : modules)  m->report_output(buffer);
            }

            // Update UI
            GuiController::update();
        }

        // Cycle modules
        for (auto& m : modules) m->cycle();

        // Check for errors
        device_check_error(nullptr);

        // If engine gets interrupted, break
        if (Engine::interrupt_signaled) {
            iterations = i;
            break;
        }

        // Print refresh rate if verbose
        if (verbose and i == 999)
            printf("Measured refresh rate: %.2f fps\n",
                1000 / (run_timer.query(nullptr)));

        // Synchronize with the clock
        if (time_limit > 0) iteration_timer.wait(time_limit);
    }

    // Wait for scheduler to complete
    Scheduler::get_instance()->wait_for_completion();

    // Final device synchronize
    device_synchronize();

    // MPI synchronize
    mpi_wrap_barrier();

    // Create report
    this->report = new Report(this, this->context.state,
        iterations, run_timer.query(nullptr));

    // Allow modules to modify report
    for (auto& m : this->modules) m->report(report);

    // Report report if verbose
    if (verbose) report->print();

    // If iterrupted by GUI controls, main thread will handle
    // Otherwise, shut down GUI
    if (not Engine::interrupt_from_gui)
        GuiController::quit();
}

/* Launches network computations
 *   Exchanges thread-safe locks with environment loop */
void Engine::network_loop() {
    // Synchronize MPI processes, if MPI is enabled (no-op otherwise)
    mpi_wrap_barrier();
    run_timer.reset();

    for (size_t i = 0 ; iterations == 0 or i < iterations; ++i) {
        // Wait for timer, then start clearing inputs
        if (time_limit > 0) iteration_timer.reset();

        // Launch pre-input calculations
        for (auto& c : clusters) c->launch_pre_input_calculations();

        /**************************/
        /*** Read sensory input ***/
        /**************************/
        sensory_lock.wait(NETWORK_THREAD);

        for (auto& c : clusters) c->launch_input();

        sensory_lock.pass(ENVIRONMENT_THREAD);

        /****************************/
        /*** Perform computations ***/
        /****************************/
        for (auto& c : clusters) c->launch_post_input_calculations();
        for (auto& c : clusters) c->launch_state_update();

        if (learning_flag)
            for (auto& c : clusters) c->launch_weight_update();

        /**************************/
        /*** Write motor output ***/
        /**************************/
        motor_lock.wait(NETWORK_THREAD);

        if (i % environment_rate == 0)
            for (auto& c : clusters) c->launch_output();

        motor_lock.pass(ENVIRONMENT_THREAD);

        // Check for errors
        device_check_error(nullptr);

        // If engine gets interrupted, halt streams and break
        if (Engine::interrupt_signaled) {
            iterations = i;
            break;
        }

        // Print refresh rate if verbose
        if (verbose and i == 999)
            printf("Measured refresh rate: %.2f fps\n",
                1000 / (run_timer.query(nullptr)));


        // Synchronize with the clock
        if (time_limit > 0) iteration_timer.wait(time_limit);
    }

    // Wait for environment to terminate first
    term_lock.wait(NETWORK_THREAD);

    // Wait for scheduler to complete
    Scheduler::get_instance()->wait_for_completion();

    // Final device synchronize
    device_synchronize();

    // MPI synchronize
    mpi_wrap_barrier();

    // Create report
    this->report = new Report(this, this->context.state,
        iterations, run_timer.query(nullptr));

    // Allow modules to modify report
    for (auto& m : modules) m->report(report);

    // Report report if verbose
    if (verbose) report->print();

    // If iterrupted by GUI controls, main thread will handle
    // Otherwise, shut down GUI
    if (not Engine::interrupt_from_gui)
        GuiController::quit();
}

/* Launches environment computations
 *   Exchanges thread-safe locks with network loop */
void Engine::environment_loop() {
    for (size_t i = 0 ; iterations == 0 or i < iterations; ++i) {
        /****************************/
        /*** Write sensory buffer ***/
        /****************************/
        sensory_lock.wait(ENVIRONMENT_THREAD);

        for (auto& c : clusters) c->wait_for_input();
        for (auto& m : modules)  m->feed_input(buffer);

        sensory_lock.pass(NETWORK_THREAD);

        /*************************/
        /*** Read motor buffer ***/
        /*************************/
        motor_lock.wait(ENVIRONMENT_THREAD);

        // Stream output
        if (i % environment_rate == 0 and not suppress_output) {
            for (auto& c : clusters) c->wait_for_output();
            for (auto& m : modules)  m->report_output(buffer);
        }

        motor_lock.pass(NETWORK_THREAD);

        // Update environment
        if (i % environment_rate == 0)
            GuiController::update();

        // Cycle modules
        for (auto& m : modules) m->cycle();

        // If engine gets interrupted, pass the locks and break
        if (Engine::interrupt_signaled) {
            sensory_lock.pass(NETWORK_THREAD);
            motor_lock.pass(NETWORK_THREAD);
            break;
        }
    }

    // Pass the termination lock
    term_lock.pass(NETWORK_THREAD);
}

/* Runs the engine:
 *   Determines active devices
 *   Builds state and transfers to devices
 *   Starts computation thread pool
 *   Extracts parameters
 *   Launches network/environment thread(s)
 *   Launches GUI
 *   Waits for threads
 *   Shuts down computation thread pool
 *   Generates report */
Report* Engine::run(PropertyConfig args) {
    // Register signal interrupt
    signal(SIGINT, handle_interrupt);

    {
        std::unique_lock<std::mutex> lock(global_engine_lock);
        if (Engine::running)
            LOG_ERROR("Cannot run more than one engine at once!");
        Engine::running = true;
    }

    // Determine device(s) to use
    std::set<DeviceID> devices;
    try {
        if (args.has("devices"))
            devices.insert(std::stoi(args.get("devices")));
        if (args.has_array("devices"))
            for (auto dev : args.get_array("devices"))
                devices.insert(std::stoi(dev));
        if (devices.size() > 0)
            ResourceManager::get_instance()->check_device_ids(devices, true);
        else
            devices = ResourceManager::get_instance()->get_default_devices();
    } catch (...) {
        LOG_ERROR("Failed to extract devices from Engine args!");
    }

    // Build state and transfer
    // This renders the engine outdated, so the engine must be rebuilt as well
    context.state->build(devices);
    context.state->transfer_to_device();
    this->rebuild(args);

    // Launch Scheduler thread pool
    Scheduler::get_instance()->start_thread_pool(
        std::max(0, args.get_int("worker threads", 4)));

    // Initialize parallel random states
    init_rand(context.network->get_max_layer_size());

    // Extract parameters
    this->verbose = args.get_bool("verbose", false);
    this->learning_flag = args.get_bool("learning flag", true);
    this->suppress_output = args.get_bool("suppress output", false);
    this->environment_rate = args.get_int("environment rate", 1);
    this->refresh_rate = args.get_float("refresh rate", FLT_MAX);
    if (this->refresh_rate == 0) this->refresh_rate = FLT_MAX;
    this->time_limit = (refresh_rate == FLT_MAX)
        ? 0 : (1.0 / this->refresh_rate);

    // If iterations is explicitly provided, use it
    if (args.has("iterations"))
        this->iterations = args.get_int("iterations", 1);
    else {
        // Otherwise, use the max of the expected iterations
        this->iterations = 0;
        for (auto module : modules)
            this->iterations =
                std::max(this->iterations, module->get_expected_iterations());
        if (this->iterations == 0)
            LOG_WARNING(
                "Unspecified number of iterations -- running indefinitely.");
    }

    // Print network
    if (this->verbose) context.network->print();


    // Ensure device is synchronized without errors
    device_synchronize();
    device_check_error("Clock device synchronization failed!");

    // Capture memory usage
    auto mems = ResourceManager::get_instance()->get_memory_usage(verbose);

    // Launch threads
    if (not Engine::interrupt_signaled) {
        // Set locks
        sensory_lock.set_owner(ENVIRONMENT_THREAD);
        motor_lock.set_owner(NETWORK_THREAD);
        term_lock.set_owner(ENVIRONMENT_THREAD);

        std::vector<std::thread> threads;
        if (args.get_bool("multithreaded", true)) {
            // Separate engine & environment threads
            if (verbose) printf("\nLaunching multithreaded...\n\n");
            threads.push_back(std::thread(&Engine::network_loop, this));
            threads.push_back(std::thread(&Engine::environment_loop, this));
        } else {
            // Single thread
            if (verbose) printf("\nLaunching single threaded...\n\n");
            threads.push_back(std::thread(
                &Engine::single_thread_loop, this));
        }

        // Launch UI on main thread
        GuiController::launch();

        // Wait for threads
        for (auto& thread : threads)
            thread.join();
    }

    bool interrupted = Engine::interrupt_signaled;
    {
        std::unique_lock<std::mutex> lock(global_engine_lock);
        // If interrupted by GUI controls, the main thread must terminate GUI
        if (Engine::interrupt_from_gui) {
            // signal=false
            GuiController::quit(false);
            Engine::interrupt_from_gui = false;
        }
        Engine::interrupt_signaled = false;
        Engine::running = false;
    }

    // Shutdown the Scheduler thread pool
    Scheduler::get_instance()->shutdown_thread_pool(verbose);

    // Clean up
    free_rand();

    // Generate report
    Report* r = (interrupted and this->report == nullptr)
        ? new Report(this, this->context.state, 0, 0.0)
        : this->report;
    r->set_child("args", &args);
    r->set("interrupted", interrupted);
    for (auto mem : mems) r->add_to_child_array("memory usage", &mem);

    // Reset engine variables
    this->report = nullptr;

    return r;
}
