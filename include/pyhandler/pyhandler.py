R"PYHANDLER( # START"


import os
import json
import math
import re
import time
import base64

import numpy as np


def __decode_param(param):
    cls = param['class']
    if cls == 'int':
        return int(param['value'])
    elif cls == 'float':
        return float(param['value'])
    elif cls == 'ndarray':
        return np.frombuffer(base64.b64decode(param['data']), param['dtype']).reshape(param['shape'])
    elif cls == 'string':
        return param['value']
    elif cls == 'list':
        return [__decode_param(item) for item in param['value']]
    elif cls == 'dict':
        return {k: __decode_param(v) for k, v in param['value']}
    else:
        raise RuntimeError(f'Param can not be decoded: {cls}')


def __encode_result(result):
    if result is None:
        return {
            'class': 'null',
        }
    if isinstance(result, int):
        return {
            'class': 'int',
            'value': result,
        }
    elif isinstance(result, float):
        return {
            'class': 'float',
            'value': result,
        }
    elif isinstance(result, np.ndarray):
        return {
            'class': 'ndarray',
            'data': base64.b64encode(result.tobytes()).decode(),
            'dtype': result.dtype.name,
            'shape': result.shape,
        }
    elif isinstance(result, str):
        return {
            'class': 'string',
            'value': result,
        }
    elif isinstance(result, (list, tuple)):
        return {
            'class': 'list',
            'value': [__encode_result(item) for item in result],
        }
    elif isinstance(result, dict):
        return {
            'class': 'object',
            'value': {k: __encode_result(v) for k, v in result.items()},
        }
    else:
        raise RuntimeError(f'Unknown result type: {type(result)}')


def __main(__in_pipe, __out_pipe):
    __in_stream = os.fdopen(__in_pipe, 'r')
    __out_stream = os.fdopen(__out_pipe, 'w')

    for __line in __in_stream:
        if __line.strip() == 'EXIT':
            break

        __cmd, *__args = json.loads(__line)
        if __cmd == 'call':
            __func_name, __params = __args
            __result = eval(__func_name, globals())(*__decode_param(__params))
        elif __cmd == 'set_vars':
            __param_names, __params = __args
            globals().update(dict(zip(__param_names, __decode_param(__params))))
            __result = None
        elif __cmd == 'exec':
            __code, __result_expr = __args
            exec(__code, globals())
            __result = eval(__result_expr, globals())
        elif __cmd == 'exec_file':
            __file_path, = __args
            with open(__file_path) as __f:
                exec(__f.read(), globals())
            __result = None

        __msg = json.dumps(__encode_result(__result))
        __out_stream.write(__msg + '\n')
        __out_stream.flush()

    os.close(__in_pipe)
    os.close(__out_pipe)


# END )PYHANDLER";
