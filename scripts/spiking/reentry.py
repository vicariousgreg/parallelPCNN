from syngen import Network, Environment, create_io_callback, FloatArray
from syngen import get_gpus, get_cpu
from syngen import set_suppress_output, set_warnings, set_debug

from os import path
import sys
import argparse

import numpy as np
import matplotlib.pyplot as plt

def build_network(dim=64):
    # Neuron parameters
    exc_init = "regular"
    inh_init = "fast"
    #exc_init = "random positive"
    #inh_init = "random negative"

    # Plasticity parameters
    plastic = True
    stp = True
    stp_tau = "5000"
    exc_learning_rate = 0.001
    inh_learning_rate = 0.01

    # Noise Parameters
    exc_noise_strength = 20.0
    exc_noise_rate = 0.01
    inh_noise_strength = 20.0
    inh_noise_rate = 0.01
    exc_random = False
    inh_random = False

    # Weight parameters
    init = "flat"
    myelinated = False

    exc_exc_weight_init = init
    exc_exc_exponent = 1.5
    exc_exc_base_weight = 0.001
    exc_exc_base_weight_min = 0.00011
    exc_exc_base_weight_max = 0.2
    exc_exc_fraction = 0.1

    exc_inh_weight_init = init
    exc_inh_exponent = 1.5
    exc_inh_base_weight = 0.001
    exc_inh_base_weight_min = 0.00011
    exc_inh_base_weight_max = 0.2
    exc_inh_fraction = 0.1

    inh_exc_weight_init = init
    inh_exc_exponent = 1.5
    inh_exc_base_weight = 0.1
    inh_exc_base_weight_min = 0.00011
    inh_exc_base_weight_max = 0.2
    inh_exc_fraction = 1.0

    # Create main structure
    structure = {"name" : "snn", "type" : "parallel"}

    exc_exc_spread = 25
    exc_inh_spread = 15
    inh_exc_spread = 15
    exc_inh_mask = 3

    # Sine wave envelope
    sine = {
        "name" : "sine",
        "neural model" : "sine generator",
        "frequency" : 25,
        "rows" : dim,
        "columns" : dim
    }

    # Poisson input layer
    poisson = {
        "name" : "poisson",
        "neural model" : "poisson generator",
        "rows" : dim,
        "columns" : dim
    }

    # Excitatory layer
    excitatory = {
        "name" : "exc",
        "neural model" : "izhikevich",
        "rows" : dim,
        "columns" : dim,
        "neuron spacing" : "0.1",
        "params" : exc_init
    }

    # Inhibitory layer
    inhibitory = {
        "name" : "inh",
        "neural model" : "izhikevich",
        "rows" : dim/2,
        "columns" : dim/2,
        "neuron spacing" : "0.2",
        "params" : inh_init
    }

    # Noise
    if exc_noise_rate > 0 and exc_noise_strength > 0:
        excitatory["init config"] = {
            "type" : "poisson",
            "value" : exc_noise_strength,
            "rate" : exc_noise_rate,
            "random" : exc_random
        }
    if inh_noise_rate > 0 and inh_noise_strength > 0:
        inhibitory["init config"] = {
            "type" : "poisson",
            "value" : inh_noise_strength,
            "rate" : inh_noise_rate,
            "random" : inh_random
        }


    # Add layers to structure
    structure["layers"] = [
        sine, poisson,
        excitatory, inhibitory]

    sine_poisson = {
        "from layer" : "sine",
        "to layer" : "poisson",
        "type" : "one to one",
        "opcode" : "add",
        "plastic" : False,
        "weight config" : {
            "type" : "flat",
            "weight" : 0.001,
            "fraction" : 1.0,
        },
        "direct" : True # direct connection into input current
    }

    poisson_exc = {
        "from layer" : "poisson",
        "to layer" : "exc",
        "type" : "one to one",
        "opcode" : "add",
        "plastic" : False,
        "weight config" : {
            "type" : "flat",
            "weight" : 20.0,
            "fraction" : 1.0,
        },
        "direct" : True # direct connection into input current
    }

    exc_exc = {
        "from layer" : "exc",
        "to layer" : "exc",
        "name" : "exc exc matrix",
        "type" : "convergent",
        "arborized config" : {
            "field size" : exc_exc_spread,
            "wrap" : True,
        },
        "opcode" : "add",
        "plastic" : plastic,
        "sparse" : True,
        "learning rate" : exc_learning_rate,
        "max weight" : "0.5",
        "myelinated" : myelinated,
        "short term plasticity" : stp,
        "short term plasticity tau" : stp_tau
    }

    exc_inh = {
        "from layer" : "exc",
        "to layer" : "inh",
        "name" : "exc inh matrix",
        "type" : "convergent",
        "arborized config" : {
            "field size" : exc_inh_spread,
            "stride" : 2,
            "wrap" : True,
        },
        "opcode" : "add",
        "plastic" : plastic,
        "sparse" : True,
        "learning rate" : exc_learning_rate,
        "max weight" : "0.5",
        "myelinated" : myelinated,
        "short term plasticity" : stp,
        "short term plasticity tau" : stp_tau
    }
    inh_exc = {
        "from layer" : "inh",
        "to layer" : "exc",
        "name" : "inh exc matrix",
        "type" : "divergent",
        "arborized config" : {
            "field size" : inh_exc_spread,
            "stride" : 2,
            "wrap" : True,
        },
        "opcode" : "sub",
        "plastic" : plastic,
        "sparse" : True,
        "learning rate" : inh_learning_rate,
        "max weight" : "0.5",
        "myelinated" : myelinated,
        "short term plasticity" : stp,
        "short term plasticity tau" : stp_tau
    }

    # Exc Exc init
    if exc_exc_weight_init == "zero":
        exc_exc["weight config"] = {
                "type" : "flat",
                "weight" : 0.00011,
                "fraction" : exc_exc_fraction,
                "diagonal" : False,
                "circular mask" : {}
            }
    elif exc_exc_weight_init == "random":
        exc_exc["weight config"] = {
                "type" : "random",
                "min weight" : exc_exc_base_weight_min,
                "max weight" : exc_exc_base_weight_max,
                "fraction" : exc_exc_fraction,
                "diagonal" : False,
                "circular mask" : {}
            }
    elif exc_exc_weight_init == "flat":
        exc_exc["weight config"] = {
                "type" : "flat",
                "weight" : exc_exc_base_weight,
                "fraction" : exc_exc_fraction,
                "diagonal" : False,
                "circular mask" : {}
            }
    elif exc_exc_weight_init == "power law":
        exc_exc["weight config"] = {
                "type" : "power law",
                "exponent" : exc_exc_exponent,
                "min weight" : exc_exc_base_weight_min,
                "max weight" : exc_exc_base_weight_max,
                "fraction" : exc_exc_fraction,
                "diagonal" : False,
                "circular mask" : {}
            }

    # Exc Inh init
    if exc_inh_weight_init == "zero":
        exc_inh["weight config"] = {
                "type" : "flat",
                "weight" : 0.00011,
                "fraction" : exc_inh_fraction,
                "circular mask" : [
                    {
                        "diameter" : exc_inh_mask,
                        "invert" : True
                    },
                    { }
                ]
            }
    elif exc_inh_weight_init == "random":
        exc_inh["weight config"] = {
                "type" : "random",
                "min weight" : exc_inh_base_weight_min,
                "max weight" : exc_inh_base_weight_max,
                "fraction" : exc_inh_fraction,
                "circular mask" : [
                    {
                        "diameter" : exc_inh_mask,
                        "invert" : True
                    },
                    { }
                ]
            }
    elif exc_inh_weight_init == "flat":
        exc_inh["weight config"] = {
                "type" : "flat",
                "weight" : exc_inh_base_weight,
                "fraction" : exc_inh_fraction,
                "circular mask" : [
                    {
                        "diameter" : exc_inh_mask,
                        "invert" : True
                    },
                    { }
                ]
            }
    elif exc_inh_weight_init == "power law":
        exc_inh["weight config"] = {
                "type" : "power law",
                "exponent" : exc_inh_exponent,
                "min weight" : exc_inh_base_weight_min,
                "max weight" : exc_inh_base_weight_max,
                "fraction" : exc_inh_fraction,
                "circular mask" : [
                    {
                        "diameter" : exc_inh_mask,
                        "invert" : True
                    },
                    { }
                ]
            }

    # Inh Exc init
    if inh_exc_weight_init == "zero":
        inh_exc["weight config"] = {
                "type" : "flat",
                "weight" : 0.00011,
                "fraction" : inh_exc_fraction,
                "distance callback" : "gaussian",
            }
    elif inh_exc_weight_init == "random":
        inh_exc["weight config"] = {
                "type" : "random",
                "min weight" : inh_exc_base_weight_min,
                "max weight" : inh_exc_base_weight_max,
                "fraction" : inh_exc_fraction,
                "distance callback" : "gaussian",
            }
    elif inh_exc_weight_init == "flat":
        inh_exc["weight config"] = {
                "type" : "flat",
                "weight" : inh_exc_base_weight,
                "fraction" : inh_exc_fraction,
                "distance callback" : "gaussian",
            }
    elif inh_exc_weight_init == "power law":
        inh_exc["weight config"] = {
                "type" : "power law",
                "exponent" : inh_exc_exponent,
                "min weight" : inh_exc_base_weight_min,
                "max weight" : inh_exc_base_weight_max,
                "fraction" : inh_exc_fraction,
                "distance callback" : "gaussian",
            }

    # Create connections
    connections = [
        sine_poisson,
        poisson_exc,
        exc_exc,
        exc_inh,
        inh_exc,
    ]

    # Create network
    return Network(
        {"structures" : [structure],
         "connections" : connections})

def build_environment(visualizer=False, peaks=False, std_dev=10):
    modules = []
    if visualizer:
        modules = [
            {
                "type" : "visualizer",
                "colored" : False,
                "layers" : [
                    { "structure" : "snn", "layer" : "sine" },
                    { "structure" : "snn", "layer" : "poisson" },
                    { "structure" : "snn", "layer" : "exc" },
                    { "structure" : "snn", "layer" : "inh" },
                ]
            },
            {
                "type" : "visualizer",
                "colored" : True,

                "decay" : False,
                "window" : 256,

                #"decay" : True,
                #"window" : 1024,
                #"bump" : 128,

                "layers" : [
                    { "structure" : "snn", "layer" : "exc" },
                    { "structure" : "snn", "layer" : "inh" },
                ]
            },
            {
                "type" : "heatmap",
                "window" : 1000000, # Long term
                "linear" : True,
                "layers" : [
                    { "structure" : "snn", "layer" : "exc" },
                    { "structure" : "snn", "layer" : "inh" },
                ]
            },
            {
                "type" : "heatmap",
                "window" : 1000, # Short term
                "linear" : False,
                "layers" : [
                    #{ "structure" : "snn", "layer" : "sine" },
                    #{ "structure" : "snn", "layer" : "poisson" },
                    { "structure" : "snn", "layer" : "exc" },
                    { "structure" : "snn", "layer" : "inh" },
                ]
            },
            {
                "type" : "gaussian_random_input",
                "rate" : "1000",
                "border" : 0,
                "std dev" : std_dev,
                "value" : 1.0,
                "normalize" : True,
                "peaks" : peaks,
                "random" : False,
                "layers" : [
                    {
                        "structure" : "snn",
                        "layer" : "sine"
                    }
                ]
            }
        ]

    return Environment({"modules" : modules})

def print_matrix_stats(matrix):
    non_zero = sum(1 for x in matrix if x >= 0.00001)
    fraction_non_zero = float(non_zero) / len(matrix)
    mat_sum = sum(matrix)
    mat_avg = (mat_sum / non_zero if non_zero > 0 else 0)
    mat_min = (min(x for x in matrix if x >= 0.00001)
                if non_zero > 0 else min(matrix))
    mat_max = max(matrix)

    print("  Non-zero: %-9d / %-9d (%7.3f%%)"
        % (non_zero, len(matrix), 100.0 * fraction_non_zero))
    print("  Sum:      %f" % mat_sum)
    print("  Average:  %9.7f" % mat_avg)
    print("  Min:      %9.7f" % mat_min)
    print("  Max:      %9.7f" % mat_max)
    print("")
    return (non_zero, mat_sum, mat_avg, mat_min, mat_max)

def compare_matrices(init_matrix, pre_matrix, post_matrix, to_size):
    print("Init:")
    (init_non_zero, init_sum, init_avg, init_min, init_max) \
        = print_matrix_stats(init_matrix)

    print("Pre:")
    (pre_non_zero, pre_sum, pre_avg, pre_min, pre_max) \
        = print_matrix_stats(pre_matrix)

    print("Post:")
    (post_non_zero, post_sum, post_avg, post_min, post_max) \
        = print_matrix_stats(post_matrix)


    num_diff = np.sum(np.mat(pre_matrix) != np.mat(post_matrix))
    diff_non_zero = post_non_zero - pre_non_zero
    diff_sum = post_sum - pre_sum
    diff_avg = post_avg - pre_avg
    diff_min = post_min - pre_min
    diff_max = post_max - pre_max

    print("Diff:")
    print("  Num diff:     %-9d" % num_diff)
    print("  Non-zero:     %-9d" % diff_non_zero)
    print("  Sum:          %f" % diff_sum)
    print("  Average:      %10.7f" % diff_avg)
    print("  Min:          %10.7f" % diff_min)
    print("  Max:          %10.7f" % diff_max)
    print("")
    print("")

    def plot_matrix(matrix, color):
        np_mat = np.mat(matrix)
        np_mat = np_mat.reshape(to_size, len(matrix) / to_size)
        np_mat = np_mat.sum(axis=1).reshape(to_size, 1)
        (hist, bins) = np.histogram(np_mat,
                 bins=np.logspace(np.log10(max(0.00001, np_mat.min()-0.00001)),
                             np.log10(np_mat.max()+0.00001), 100))
        x = [10 ** (0.5 * (np.log10(bins[i]) + np.log10(bins[i+1]))) for i in range(len(hist))]
        try:
            hist = [float(v) / hist.sum() for v in hist]
        except ZeroDivisionError:
            pass
        plt.loglog(x, hist, "o", color=color)

    def plot_array(matrix, color):
        np_mat = np.mat([x for x in matrix if x > 0.0])
        if np_mat.size == 0:
            x = 0.0
            hist = [len(matrix)]
        else:
            np_mat = np_mat.reshape(np_mat.size, 1)
            (hist, bins) = np.histogram(np_mat,
                     bins=np.logspace(np.log10(max(0.00001, np_mat.min()-0.00001)),
                         np.log10(np_mat.max()+0.00001), 100))
            x = [10 ** (0.5 * (np.log10(bins[i]) + np.log10(bins[i+1]))) for i in range(len(hist))]
            try:
                hist = [float(v) / hist.sum() for v in hist]
            except ZeroDivisionError:
                pass
        plt.loglog(x, hist, "o", color=color)

    plt.subplot(1,2,1)
    plot_matrix(init_matrix, 'g')
    plot_matrix(pre_matrix, 'b')
    plot_matrix(post_matrix, 'r')

    plt.subplot(1,2,2)
    plot_array(init_matrix, 'g')
    plot_array(pre_matrix, 'b')
    plot_array(post_matrix, 'r')

    plt.show()

def main(infile=None, outfile=None, do_training=True, print_stats=True,
        dim=128, peaks=1, visualizer=False, refresh_rate=0, device=None,
        iterations=1000000):
    std_dev = int(dim / 10)

    network = build_network(dim)
    env = build_environment(visualizer, peaks, std_dev)

    if print_stats:
        init_exc_exc_matrix = network.get_weight_matrix("exc exc matrix").to_list()
        init_exc_inh_matrix = network.get_weight_matrix("exc inh matrix").to_list()
        init_inh_exc_matrix = network.get_weight_matrix("inh exc matrix").to_list()

    if infile is not None:
        if not path.exists(infile):
            print("Could not open state file: " + infile)
        else:
            print("Loading state from " + infile + " ...")
            network.load_state(infile)
            print("... done.")

    if print_stats:
        pre_exc_exc_matrix = network.get_weight_matrix("exc exc matrix").to_list()
        pre_exc_inh_matrix = network.get_weight_matrix("exc inh matrix").to_list()
        pre_inh_exc_matrix = network.get_weight_matrix("inh exc matrix").to_list()

    if device is None:
        device = get_gpus()[1]
    if do_training:
        train_args = {"multithreaded" : True,
                      "worker threads" : 4 if device == get_cpu() else 0,
                      "devices" : device,
                      "iterations" : iterations,
                      "verbose" : True}
        if refresh_rate > 0:
            train_args["refresh rate"] = refresh_rate;
        report = network.run(env, train_args)

        if report is None:
            print("Engine failure.  Exiting...")
            return
        print(report)

        if outfile is not None:
            print("Saving state to " + outfile + " ...")
            network.save_state(outfile)
            print("... done.")

    if print_stats:
        post_exc_exc_matrix = network.get_weight_matrix("exc exc matrix").to_list()
        post_exc_inh_matrix = network.get_weight_matrix("exc inh matrix").to_list()
        post_inh_exc_matrix = network.get_weight_matrix("inh exc matrix").to_list()

        exc_dim = dim**2
        inh_dim = (dim/2)**2
        print("Excitatory-excitatory Matrix:")
        compare_matrices(init_exc_exc_matrix, pre_exc_exc_matrix, post_exc_exc_matrix, exc_dim)
        print("Excitatory-inhibitory Matrix:")
        compare_matrices(init_exc_inh_matrix, pre_exc_inh_matrix, post_exc_inh_matrix, inh_dim)
        print("Inhibitory-excitatory Matrix:")
        compare_matrices(init_inh_exc_matrix, pre_inh_exc_matrix, post_inh_exc_matrix, exc_dim)

    # Delete the objects
    del network
    del env

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', type=str,
                        help='source state file')
    parser.add_argument('-o', type=str,
                        help='destination state file')
    parser.add_argument('-t', action='store_true', default=False,
                        dest='train',
                        help='run training')
    parser.add_argument('-s', action='store_true', default=False,
                        dest='stats',
                        help='print statistics')
    parser.add_argument('-dim', type=int, default=128,
                        help='dimensions')
    parser.add_argument('-p', type=int, default=1,
                        help='noise peaks')
    parser.add_argument('-v', action='store_true', default=False,
                        dest='visualizer',
                        help='run the visualizer')
    parser.add_argument('-host', action='store_true', default=False,
                        help='run on host CPU')
    parser.add_argument('-d', type=int, default=1,
                        help='run on device #')
    parser.add_argument('-r', type=int, default=0,
                        help='refresh rate')
    parser.add_argument('-it', type=int, default=1000000,
                        help='iterations')
    args = parser.parse_args()

    if args.host or len(get_gpus()) == 0:
        device = get_cpu()
    else:
        device = get_gpus()[args.d]

    set_suppress_output(False)
    set_warnings(False)
    set_debug(False)

    main(args.i, args.o, args.train, args.stats, args.dim, args.p, args.visualizer, args.r, device, args.it)
