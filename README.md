# PyHandler

PyHandler is a C++ class designed to interface with Python scripts, providing a seamless way to execute Python functions, set variables, and run code or files directly from C++ applications. This project enables C++ programs to tap into the power of Python's extensive libraries and features by running Python code in a separate process and communicating via JSON.

## Features

- Directly call Python functions from C++ with arguments and return values.
- Set variables in the Python environment from C++.
- Execute arbitrary Python code snippets and obtain the results.
- Run Python files as scripts within the C++ application context.
- Ensure safe and managed execution of Python code in a separate process.
- Facilitate easy integration with existing C++ projects.

## Requirements

- A C++ compiler with C++11 support or higher.
- Python 3 installed on the system (located at `/usr/bin/python3`).
- A C++ JSON library (e.g., [nlohmann/json](https://github.com/nlohmann/json)).

## Installation

To integrate PyHandler into your C++ project, follow these steps:

1. Clone the repository or download the source files into your project directory.
2. Verify that Python 3 is installed on your system and is accessible at `/usr/bin/python3`.
3. Incorporate the `PyHandler` class into your C++ project.
4. If not already present, include a JSON library such as `nlohmann/json` in your project.

## Usage

### Calling a Python Function

```cpp
#include "pyhandler/pyhandler.hpp"
#include <iostream>
#include <vector>

namespace ph = pyhandler;

int main() {
    auto sum = ph::call<int>("sum", std::vector<int>{1, 2, 3, 4, 5});
    std::cout << "Sum(1, 2, 3, 4, 5) = " << sum << std::endl;

    return 0;
}
```

Compile and run:

```bash
g++ -std=c++11 -I<pyhandler_path> -I<nlohmann_path> -o example example.cpp
./example
```

Output:

```
Sum(1, 2, 3, 4, 5) = 15
```

### Executing Arbitrary Code

```cpp
ph::set_vars({"a", "b"}, 12, 34);
auto power_result = ph::exec<std::string>("str(a ** b)");
std::cout << "12 ** 34 = " << power_result << std::endl;
```

Output:

```
12 ** 34 = 4922235242952026704037113243122008064
```

### Executing Complex Code and Retrieving a Result

```cpp
ph::set_vars({"msg"}, "123321");
auto encoded_message = ph::exec<std::string>(R"(
import base64
msg_b64 = base64.b64encode(msg.encode()).decode()
)", "f'Message: {msg}, Base64Encoded: {msg_b64}'");
std::cout << encoded_message << std::endl;
```

Output:

```
Message: 123321, Base64Encoded: MTIzMzIx
```

### Executing a File

Save the following Python function to a file (e.g., `funcs.py`):

```python
def lets_233(s):
    i = 0
    d = 1
    while i < len(s):
        k = 2
        print(s[i:i+k], end='')
        print('233' + d * '3', end='')
        d += 2
        i += k
        print()
```

Execute the file from C++:

```cpp
ph::exec_file("funcs.py");
ph::call<void>("lets_233", "abcde");
ph::call<void>("lets_233", "qwertyasdf");
```

Output:

```
ab2333cd233333e23333333
qw2333er233333ty23333333as2333333333df233333333333
```

### Working with Numpy Arrays

```cpp
std::vector<uint8_t> data(1000000, 123);
auto sum = ph::call<int>("lambda x: int(np.sum(x))", ph::NDArray(data.data(), {data.size()}));
std::cout << "Sum of array elements: " << sum << std::endl;
```

Output:

```
Sum of array elements: 123000000
```

## API

```cpp
// Call a Python function and retrieve the result.
ResultType call<ResultType>(string function_name, ParamType... params);

// Call a Python function without expecting a result.
void call<void>(string function_name, ParamType... params);

// Set variables in the Python environment.
void set_vars(array<string, N> var_names, VarType... var_values);

// Execute Python code and retrieve the result.
ResultType exec<ResultType>(string py_code, string result_expr);

// Execute Python code without expecting a result.
void exec<void>(string py_code);

// Execute a single expression and retrieve the result.
ResultType exec<ResultType>(string result_expr);

// Execute a Python file.
void exec_file(string file_path);
```

## License

This project is open-sourced under the [MIT License](LICENSE).