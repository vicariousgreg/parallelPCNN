from ctypes import *
from collections import OrderedDict
from json import dumps
from copy import deepcopy
from numpy import copyto
from numpy.ctypeslib import as_array

""" Wrapper for C Array Structure """
class CArray(Structure):
    _fields_=[("size",c_int),
              ("type",c_uint),
              ("data",c_void_p),
              ("owner",c_bool)]

""" Base class for typed Python array wrappers """
class PArray:
    def __init__(self, size, ptr):
        self.size = size
        self.data = ptr

    def __getitem__(self, index):
        return self.data[index]

    def __setitem__(self, index, value):
        self.data[index] = value

    def __iter__(self):
        curr = 0
        while (curr < self.size):
            yield self.data[curr]
            curr += 1

    def is_void(self):
        return False

    def to_np_array(self):
        return as_array(self.data,shape=(self.size,))

    def copy_from(self, arr):
        copyto(self.to_np_array(), arr)

""" Typed Python array wrappers """
class FloatArray(PArray):
    def __init__(self, size, ptr):
        self.size = size
        self.data = cast(ptr, POINTER(c_float))

    def to_list(self):
        return [float(x) for x in self]

class IntArray(PArray):
    def __init__(self, size, ptr):
        self.size = size
        self.data = cast(ptr, POINTER(c_int))

    def to_list(self):
        return [int(x) for x in self]

class StringArray(PArray):
    def __init__(self, size, ptr):
        self.size = size
        self.data = cast(ptr, POINTER(POINTER(c_char)))

class VoidArray(PArray):
    def __init__(self, size, ptr):
        self.size = size
        self.data = cast(ptr, POINTER(c_void_p))

    def is_void(self):
        return True

""" Construct a Python array from a C array """
def build_array(base_array):
    # See POINTER_TYPE in extern.h
    (F_P, I_P, S_P, P_P, V_P) = (1,2,3,4,5)

    if base_array.type == F_P:
        return FloatArray(base_array.size, base_array.data)
    elif base_array.type == I_P:
        return IntArray(base_array.size, base_array.data)
    elif base_array.type == S_P:
        return StringArray(base_array.size, base_array.data)
    elif base_array.type == P_P:
        return VoidArray(base_array.size, base_array.data)
    elif base_array.type == V_P:
        return VoidArray(base_array.size, base_array.data)


""" Library loading, arg/ret initialization """
_syn = CDLL('libsyngen.so')

_syn.free_array.argtypes = (CArray, )
_syn.free_array_deep.argtypes = (CArray, )

_syn.create_properties.restype = c_void_p
_syn.add_property.argtypes = (c_void_p, c_char_p, c_char_p)
_syn.add_child.argtypes = (c_void_p, c_char_p, c_void_p)
_syn.add_to_array.argtypes = (c_void_p, c_char_p, c_char_p)
_syn.add_to_child_array.argtypes = (c_void_p, c_char_p, c_void_p)

_syn.get_keys.argtypes = (c_void_p,)
_syn.get_keys.restype = CArray
_syn.get_child_keys.argtypes = (c_void_p,)
_syn.get_child_keys.restype = CArray
_syn.get_array_keys.argtypes = (c_void_p,)
_syn.get_array_keys.restype = CArray
_syn.get_child_array_keys.argtypes = (c_void_p,)
_syn.get_child_array_keys.restype = CArray

_syn.get_property.restype = c_char_p
_syn.get_property.argtypes = (c_void_p, c_char_p)
_syn.get_child.restype = c_void_p
_syn.get_child.argtypes = (c_void_p, c_char_p)
_syn.get_array.restype = CArray
_syn.get_array.argtypes = (c_void_p, c_char_p)
_syn.get_child_array.restype = CArray
_syn.get_child_array.argtypes = (c_void_p, c_char_p)

_syn.create_environment.restype = c_void_p
_syn.create_environment.argtypes = (c_void_p,)
_syn.load_env.restype = c_void_p
_syn.load_env.argtypes = (c_char_p,)
_syn.save_env.restype = c_bool
_syn.save_env.argtypes = (c_void_p, c_char_p)

_syn.create_network.restype = c_void_p
_syn.create_network.argtypes = (c_void_p,)
_syn.load_net.restype = c_void_p
_syn.load_net.argtypes = (c_char_p,)
_syn.save_net.restype = c_bool
_syn.save_net.argtypes = (c_void_p, c_char_p)

_syn.build_state.restype = c_void_p
_syn.build_state.argtypes = (c_void_p,)
_syn.build_load_state.restype = c_void_p
_syn.build_load_state.argtypes = (c_void_p,c_char_p)
_syn.load_state.restype = c_bool
_syn.load_state.argtypes = (c_void_p, c_char_p)
_syn.save_state.restype = c_bool
_syn.save_state.argtypes = (c_void_p, c_char_p)

_syn.get_neuron_data.restype = CArray
_syn.get_neuron_data.argtypes = (c_void_p, c_char_p, c_char_p, c_char_p)
_syn.get_layer_data.restype = CArray
_syn.get_layer_data.argtypes = (c_void_p, c_char_p, c_char_p, c_char_p)
_syn.get_connection_data.restype = CArray
_syn.get_connection_data.argtypes = (c_void_p, c_char_p, c_char_p)
_syn.get_weight_matrix.restype = CArray
_syn.get_weight_matrix.argtypes = (c_void_p, c_char_p, c_char_p)

_syn.run.restype = c_void_p
_syn.run.argtypes = (c_void_p, c_void_p, c_void_p, c_void_p)

_syn.destroy.argtypes = (c_void_p,)
_syn.destroy_network.argtypes = (c_void_p,)
_syn.destroy_environment.argtypes = (c_void_p,)
_syn.destroy_state.argtypes = (c_void_p,)
_syn.destroy_properties.argtypes = (c_void_p,)

_syn.get_cpu.restype = c_int
_syn.get_gpus.restype = CArray
_syn.get_all_devices.restype = CArray

_syn.set_suppress_output.argtypes = (c_bool,)
_syn.set_warnings.argtypes = (c_bool,)
_syn.set_debug.argtypes = (c_bool,)

_syn.add_io_callback.argtypes = (c_char_p, c_longlong)
_syn.add_weight_callback.argtypes = (c_char_p, c_longlong)
_syn.add_indices_weight_callback.argtypes = (c_char_p, c_longlong)
_syn.add_distance_weight_callback.argtypes = (c_char_p, c_longlong)
_syn.add_delay_weight_callback.argtypes = (c_char_p, c_longlong)

_syn.get_mpi_rank.restype = c_int
_syn.get_mpi_size.restype = c_int


""" Wrappers for simple functions """
def get_cpu():
    return _syn.get_cpu()

def get_gpus():
    gpus_object = _syn.get_gpus()
    gpus = build_array(gpus_object).to_list()
    _syn.free_array(gpus_object)
    return gpus

def get_all_devices():
    devices_object = _syn.get_all_devices()
    devices = build_array(devices_object).to_list()
    _syn.free_array(devices_object)
    return devices

def set_suppress_output(val):
    _syn.set_suppress_output(val)

def set_warnings(val):
    _syn.set_warnings(val)

def set_debug(val):
    _syn.set_debug(val)

def interrupt_engine():
    _syn.interrupt_engine()


""" Callback Maintenance """
_io_callbacks = dict()
_weight_callbacks = dict()
_indices_weight_callbacks = dict()
_distance_weight_callbacks = dict()
_callback_config_map = dict()

def make_callback_id(config):
    _callback_config_map[len(_callback_config_map_)] = config

def get_config(ID):
    return _callback_config_map[ID]

def create_io_callback(name, f):
    cb = CFUNCTYPE(None, c_int, c_int, c_void_p)(f)
    addr = cast(cb, c_void_p).value
    _io_callbacks[name] = (cb, addr)
    _syn.add_io_callback(name.encode('utf-8'), addr)

def create_weight_callback(name, f):
    cb = CFUNCTYPE(None, c_int, c_int, c_void_p)(f)
    addr = cast(cb, c_void_p).value
    _weight_callbacks[name] = (cb, addr)
    _syn.add_weight_callback(name.encode('utf-8'), addr)

def create_indices_weight_callback(name, f):
    cb = CFUNCTYPE(None, c_int, c_int, c_void_p, c_void_p, c_void_p, c_void_p)(f)
    addr = cast(cb, c_void_p).value
    _indices_weight_callbacks[name] = (cb, addr)
    _syn.add_indices_weight_callback(name.encode('utf-8'), addr)

def create_distance_weight_callback(name, f):
    cb = CFUNCTYPE(None, c_int, c_int, c_void_p, c_void_p)(f)
    addr = cast(cb, c_void_p).value
    _distance_weight_callbacks[name] = (cb, addr)
    _syn.add_distance_weight_callback(name.encode('utf-8'), addr)

def create_delay_weight_callback(name, f):
    cb = CFUNCTYPE(None, c_int, c_int, c_void_p, c_void_p)(f)
    addr = cast(cb, c_void_p).value
    _delay_weight_callbacks[name] = (cb, addr)
    _syn.add_delay_weight_callback(name.encode('utf-8'), addr)


""" Properties: C dictionary implementation """
class Properties:
    def __init__(self, props=dict()):
        # Dictionaries of Property Objects
        self.properties = OrderedDict()
        self.children = OrderedDict()
        self.arrays = OrderedDict()
        self.child_arrays = OrderedDict()
        self.obj = None

        # If dictionary, build C properties
        if type(props) is dict:
            self.obj = _syn.create_properties()
            for k,v in props.items():
                if v is None: continue

                if type(v) is dict:
                    self.add_child(k, v)
                elif type(v) is list:
                    for x in v:
                        if type(x) is dict:
                            self.add_to_child_array(k, x)
                        elif type(x) is bool:
                            self.add_to_array(k, "true" if x else "false")
                        else:
                            self.add_to_array(k, str(x))
                else:
                    self.add_property(k, v)
        # If int, interpret as C properties pointer and retrieve data
        elif type(props) is int:
            self.obj = props

            # Get string properties
            keys_obj = _syn.get_keys(props)
            keys = build_array(keys_obj)
            for i in range(keys.size):
                key = keys.data[i]
                key_str = string_at(cast(key, c_char_p)).decode('utf-8')
                self.properties[key_str] = string_at(
                    _syn.get_property(props, key)).decode('utf-8')

            _syn.free_array_deep(keys_obj)

            # Get child properties
            keys_obj = _syn.get_child_keys(props)
            keys = build_array(keys_obj)
            for i in range(keys.size):
                key = keys.data[i]
                key_str = string_at(cast(key, c_char_p)).decode('utf-8')
                self.children[key_str] = Properties(
                    _syn.get_child(props, key))
            _syn.free_array_deep(keys_obj)

            # Get array properties
            keys_obj = _syn.get_array_keys(props)
            keys = build_array(keys_obj)
            for i in range(keys.size):
                key = keys.data[i]
                key_str = string_at(cast(key, c_char_p)).decode('utf-8')
                arr_obj = _syn.get_array(props, key)
                arr = build_array(arr_obj)

                if key_str not in self.arrays:
                    self.arrays[key_str] = []

                for j in range(arr.size):
                    self.arrays[key_str].append(
                        string_at(cast(arr.data[j], c_char_p)).decode('utf-8'))
                _syn.free_array(arr_obj)
            _syn.free_array_deep(keys_obj)

            # Get child array properties
            keys_obj = _syn.get_child_array_keys(props)
            keys = build_array(keys_obj)
            for i in range(keys.size):
                key = keys.data[i]
                key_str = string_at(cast(key, c_char_p)).decode('utf-8')
                arr_obj = _syn.get_child_array(props, key)
                arr = build_array(arr_obj)

                if key_str not in self.child_arrays:
                    self.child_arrays[key_str] = []

                for j in range(arr.size):
                    self.child_arrays[key_str].append(
                        Properties(arr.data[j]))
                _syn.free_array(arr_obj)
            _syn.free_array_deep(keys_obj)

    def to_dict(self):
        out = OrderedDict()
        for k,v in self.properties.items():
            out[k] = v
        for k,v in self.children.items():
            out[k] = v.to_dict()
        for k,v in self.arrays.items():
            out[k] = [str(p) for p in v]
        for k,v in self.child_arrays.items():
            out[k] = [p.to_dict() for p in v]
        return out

    def __str__(self):
        return dumps(self.to_dict(), indent=4)

    def add_property(self, key, val):
        self.properties[key] = val
        if type(val) is bool:
            val = "true" if val else "false"
        _syn.add_property(self.obj,
            key.encode('utf-8'),
            str(val).encode('utf-8'))

    def add_child(self, key, child):
        child = Properties(child)
        self.children[key] = child
        _syn.add_child(self.obj,
            key.encode('utf-8'),
            child.obj)

    def add_to_array(self, key, val):
        if key not in self.arrays:
            self.arrays[key] = []
        self.arrays[key].append(val)
        _syn.add_to_array(self.obj,
            key.encode('utf-8'),
            val.encode('utf-8'))

    def add_to_child_array(self, key, dictionary):
        props = Properties(dictionary)
        if key not in self.child_arrays:
            self.child_arrays[key] = []
        self.child_arrays[key].append(props)
        _syn.add_to_child_array(self.obj,
            key.encode('utf-8'),
            props.obj)

    def free(self):
        if _syn is not None:
            if self.obj is not None:
                _syn.destroy(self.obj)
                self.obj = None


""" Environment Wrapper """
class Environment:
    def __init__(self, env):
        if type(env) is str:
            self.obj = _syn.load_env(env)
            self.props = None
        elif type(env) is dict:
            self.props = Properties(env)
            self.obj = _syn.create_environment(self.props.obj)
        else:
            self.obj = None

    def save(self, filename):
        return _syn.save_env(self.obj, filename.encode('utf-8'))

    def free(self):
        if _syn is not None:
            if self.props is not None:
                self.props.free()
                self.props = None
            _syn.destroy_environment(self.obj)
            self.obj = None


""" Network Wrapper """
class Network:
    def __init__(self, net):
        if type(net) is str:
            self.obj = _syn.load_net(net)
            self.props = None
        elif type(net) is dict:
            self.props = Properties(net)
            self.obj = _syn.create_network(self.props.obj)
        else:
            self.obj = None
        self.state = None

    def save(self, filename):
        return _syn.save_net(self.obj, filename.encode('utf-8'))

    def free(self):
        if _syn is not None:
            if self.props is not None:
                self.props.free()
                self.props = None
            if self.state is not None:
                _syn.destroy_state(self.state)
                self.state = None
            _syn.destroy_network(self.obj)
            self.obj = None

    def build_state(self, filename=None):
        if self.state is not None:
            _syn.destroy_state(self.state)

        if filename is None:
            self.state = _syn.build_state(self.obj)
        else:
            self.state = _syn.build_load_state(self.obj,
                filename.encode('utf-8'))

    def load_state(self, filename):
        if self.state is None: self.build_state(filename)
        else: _syn.load_state(self.state, filename.encode('utf-8'))

    def save_state(self, filename):
        if self.state is None: self.build_state()
        _syn.save_state(self.state, filename.encode('utf-8'))

    def get_neuron_data(self, structure, layer, key):
        if self.state is None: self.build_state()
        return build_array(
            _syn.get_neuron_data(self.state,
                structure.encode('utf-8'),
                layer.encode('utf-8'),
                key.encode('utf-8')))

    def get_layer_data(self, structure, layer, key):
        if self.state is None: self.build_state()
        return build_array(
            _syn.get_layer_data(self.state,
                structure.encode('utf-8'),
                layer.encode('utf-8'),
                key.encode('utf-8')))

    def get_connection_data(self, structure, connection, key):
        if self.state is None: self.build_state()
        return build_array(
            _syn.get_connection_data(self.state,
                connection.encode('utf-8'),
                key.encode('utf-8')))

    def get_weight_matrix(self, connection, key="weights"):
        if self.state is None: self.build_state()
        return build_array(
            _syn.get_weight_matrix(self.state,
                connection.encode('utf-8'),
                key.encode('utf-8')))

    def run(self, environment, args=dict()):
        # Build state
        if self.state is None: self.build_state()

        # Build environment
        if not isinstance(environment, Environment):
            environment = Environment(environment)

        # Build args
        if not isinstance(args, Properties):
            args = Properties(args)

        report = _syn.run(self.obj, environment.obj, self.state, args.obj)
        if report is None: print("Failed to run network!")
        return Properties(report)


def fill_in(defaults, props):
    new_props = deepcopy(props)

    for k,v in defaults.items():
        if type(v) is dict:
            if k in new_props:
                new_props[k] = fill_in(v, new_props[k])
            else:
                new_props[k] = deepcopy(v)
        elif k not in new_props:
            new_props[k] = deepcopy(v)
    return new_props


""" Properties Factory """
class PropertiesFactory:
    def __init__(self, defaults=None, func=None):
        if defaults is None and func is None:
            raise ValueError
        self.set_defaults(defaults)
        self.set_func(func)

    def set_defaults(self, defaults):
        if defaults is not None and not type(defaults) is dict:
            raise ValueError
        self.defaults = defaults

    def set_func(self, func):
        if func is not None and not callable(func):
            raise ValueError
        self.func = func

    def build(self, props=dict()):
        if self.defaults is not None:
            new_props = fill_in(self.defaults, new_props)
        if self.func is not None:
            self.func(new_props)
        return new_props

""" Layer Factory """
class LayerFactory(PropertiesFactory):
    pass

""" Connection Factory """
class ConnectionFactory(PropertiesFactory):
    def build(self, from_layer, to_layer, props=dict()):
        try:
            new_props = self.build(props)
        except TypeError:
            new_props = fill_in(self.defaults, props)
            if self.defaults is not None:
                new_props = fill_in(self.defaults, new_props)
            self.func(from_layer, to_layer, new_props)
        return new_props


""" Callback module builders """
def make_custom_input_module(structure, layer_names, name, cb, clear=True):

    def custom_callback(ID, size, ptr):
        cb(layer_names[ID], FloatArray(size,ptr).to_np_array())

    create_io_callback(name, custom_callback)

    return {
        "type" : "callback",
        "clear" : clear,
        "layers" : [
            {
                "structure" : structure,
                "layer" : layer_name,
                "input" : True,
                "function" : name,
                "id" : i
            } for i,layer_name in enumerate(layer_names)
        ]
    }

def make_custom_output_module(structure, layer_names, name, cb):

    def custom_callback(ID, size, ptr):
        cb(layer_names[ID], FloatArray(size,ptr).to_np_array())

    create_io_callback(name, custom_callback)

    return {
        "type" : "callback",
        "layers" : [
            {
                "structure" : structure,
                "layer" : layer_name,
                "output" : True,
                "function" : name,
                "id" : i
            } for i,layer_name in enumerate(layer_names)
        ]
    }


""" Common callbacks """
from math import exp
def gauss(dist, peak, sig, norm=False):
    inv_sqrt_2pi = 0.3989422804014327
    peak_coeff = peak * (1.0 if norm else inv_sqrt_2pi / sig)

    a = dist / sig
    return peak_coeff * exp(-0.5 * a * a)

# Difference of gaussians
def diff_gauss(dist, peak, sig, norm=False):
    v = gauss(dist, peak, sig, False) - gauss(dist, peak, sig * 1.6, False)

    if norm:
        inv_sqrt_2pi = 0.3989422804014327
        peak_coeff = ((inv_sqrt_2pi / sig) - (inv_sqrt_2pi / (sig * 1.6)))
        v = v / peak_coeff

    return v

# Initializes weights based on the distances between the nodes
def gaussian_dist_callback(ID, size, weights, distances):
    w_arr = FloatArray(size, weights)
    d_arr = FloatArray(size, distances)

    # Use half maximum distance for standard deviation
    sigma = max(d_arr.data[i] for i in range(size)) / 2

    # Smooth out weights based on distances
    if sigma != 0:
        for i in range(size):
            w_arr.data[i] = gauss(d_arr.data[i], w_arr.data[i], sigma, True)

create_distance_weight_callback("gaussian", gaussian_dist_callback)

# Initializes weights based on the distances between the nodes
# Uses difference of gaussian method to create mexican hat receptive field
def mexican_hat_dist_callback(ID, size, weights, distances):
    w_arr = FloatArray(size, weights)
    d_arr = FloatArray(size, distances)

    # Use half maximum distance for standard deviation
    sigma = max(d_arr.data[i] for i in range(size)) / 2

    # Smooth out weights based on distances
    if sigma != 0:
        for i in range(size):
            w_arr.data[i] = diff_gauss(d_arr.data[i], w_arr.data[i], sigma, True)

create_distance_weight_callback("mexican_hat", mexican_hat_dist_callback)


""" Other Library Functions """
def get_dsst_params(num_rows=8, cell_res=8):
    num_cols = 18
    cell_rows = 2*cell_res + 1
    spacing = cell_res / 4

    input_rows = (num_rows + 2) * (cell_rows + spacing) - spacing
    input_cols = num_cols * (cell_res + spacing) - spacing

    focus_rows = input_rows - cell_rows
    focus_cols = input_cols - cell_res

    return {
        "columns" : num_cols,
        "rows" : num_rows,
        "cell columns" : cell_res,
        "cell rows" : cell_rows,
        "spacing" : spacing,
        "input rows" : input_rows,
        "input columns" : input_cols,
        "focus rows" : focus_rows,
        "focus columns" : focus_cols,
    }


""" MPI """
try:
    # Attempt to load MPI library, and initialize in synaptogenesis
    CDLL("libmpi.so", 2 | 4 | 256)
    _syn.initialize_mpi()


    # Make sure to finalize MPI on exit
    import atexit

    def finalize():
        _syn.finalize_mpi()

    atexit.register(finalize)
except OSError: pass

def get_mpi_rank():
    return _syn.get_mpi_rank()

def get_mpi_size():
    return _syn.get_mpi_size()
