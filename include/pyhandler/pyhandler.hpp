#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <queue>
#include <vector>

#include "nlohmann/json.hpp"

#include "pyhandler/base64.hpp"
#include "pyhandler/concurrent.hpp"

namespace pyhandler {

using json = nlohmann::json;

class NDArray {
public:
    NDArray() {
        this->data = {};
        this->shape = {};
        this->dtype = "float32";
    }

    NDArray(const std::vector<uint8_t>& data, const std::vector<size_t>& shape, const std::string& dtype) {
        this->data = data;
        this->shape = shape;
        this->dtype = dtype;
    }

    NDArray(const int* data, const std::vector<size_t>& shape) {
        this->shape = shape;
        this->dtype = "int32";
        uint8_t* ptr = (uint8_t*)data;
        this->data = {ptr, ptr + nr_elem() * sizeof(int)};
    }

    NDArray(const float* data, const std::vector<size_t>& shape) {
        this->shape = shape;
        this->dtype = "float32";
        uint8_t* ptr = (uint8_t*)data;
        this->data = {ptr, ptr + nr_elem() * sizeof(float)};
    }

    NDArray(const uint16_t* data, const std::vector<size_t>& shape) {
        this->shape = shape;
        this->dtype = "uint16";
        uint8_t* ptr = (uint8_t*)data;
        this->data = {ptr, ptr + nr_elem() * sizeof(uint16_t)};
    }

    NDArray(const uint8_t* data, const std::vector<size_t>& shape) {
        this->shape = shape;
        this->dtype = "uint8";
        uint8_t* ptr = (uint8_t*)data;
        this->data = {ptr, ptr + nr_elem() * sizeof(uint8_t)};
    }

    size_t nr_elem() const {
        if (shape.empty()) {
            return 0;
        }
        size_t cnt = 1;
        for (const auto s : shape) {
            cnt *= s;
        }
        return cnt;
    }

    template <class T>
    T* ptr() {
        return (T*)data.data();
    }

    template <class T>
    const T* ptr() const {
        return (T*)data.data();
    }

    std::vector<uint8_t> data;
    std::vector<size_t> shape;
    std::string dtype;
};

template <class Param>
struct ParamEncoder {
    static inline json impl(const Param& param) { throw std::runtime_error("Unknown param type"); }
};

template <>
struct ParamEncoder<long long> {
    static inline json impl(long long param) { return json::object({{"class", "int"}, {"value", param}}); }
};

template <>
struct ParamEncoder<int> {
    static inline json impl(int param) { return json::object({{"class", "int"}, {"value", param}}); }
};

template <>
struct ParamEncoder<double> {
    static inline json impl(double param) { return json::object({{"class", "float"}, {"value", param}}); }
};

template <>
struct ParamEncoder<float> {
    static inline json impl(float param) { return json::object({{"class", "float"}, {"value", param}}); }
};

template <>
struct ParamEncoder<NDArray> {
    static inline json impl(const NDArray& param) {
        return json::object(
                {{"class", "ndarray"},
                 {"data", base64_encode(param.data)},
                 {"dtype", param.dtype},
                 {"shape", param.shape}});
    }
};

template <>
struct ParamEncoder<std::string> {
    static inline json impl(const std::string& param) { return json::object({{"class", "string"}, {"value", param}}); }
};

template <>
struct ParamEncoder<const char*> {
    static inline json impl(const char* param) { return json::object({{"class", "string"}, {"value", param}}); }
};

template <class T>
struct ParamEncoder<std::vector<T>> {
    static inline json impl(const std::vector<T>& param) {
        json j = json::array();
        for (const auto& item : param) {
            j.push_back(ParamEncoder<typename std::remove_reference<T>::type>::impl(item));
        }
        return json::object({{"class", "list"}, {"value", j}});
    }
};

template <class T, size_t N>
struct ParamEncoder<std::array<T, N>> {
    static inline json impl(const std::array<T, N>& param) {
        json j = json::array();
        for (const auto& item : param) {
            j.push_back(ParamEncoder<typename std::remove_reference<T>::type>::impl(item));
        }
        return json::object({{"class", "list"}, {"value", j}});
    }
};

template <size_t I, size_t N>
struct TupleEncoder {
    template <class... T>
    static void impl(const std::tuple<T...>& t, json& v) {
        v.push_back(ParamEncoder<typename std::remove_reference<
                            typename std::tuple_element<I, std::tuple<T...>>::type>::type>::impl(std::get<I>(t)));
        TupleEncoder<I + 1, N>::impl(t, v);
    }
};

template <size_t N>
struct TupleEncoder<N, N> {
    template <class... T>
    static void impl(const std::tuple<T...>& t, json& v) {}
};

template <class... T>
struct ParamEncoder<std::tuple<T...>> {
    static inline json impl(const std::tuple<T...>& param) {
        json j = json::array();
        TupleEncoder<0, sizeof...(T)>::impl(param, j);
        return json::object({{"class", "list"}, {"value", j}});
    }
};

template <class S, class D>
struct Cast {
    template <class T = D>
    static typename std::enable_if<!std::is_convertible<S, T>::value, T>::type impl(const S& src) {
        throw std::runtime_error("Unknown conversion");
    }

    template <class T = D>
    static typename std::enable_if<std::is_convertible<S, T>::value, T>::type impl(const S& src) {
        return src;
    }
};

template <class S>
struct Cast<S, S> {
    static S impl(const S& src) { return src; }
};

template <>
struct Cast<json, void> {
    static void impl(json src) { (void)src; }
};

template <class T>
struct Cast<json, T> {
    static T impl(const json& result) {
        std::string cls = result["class"];
        if (cls == "int") {
            return Cast<long long, T>::impl((long long)result["value"]);
        } else if (cls == "float") {
            return Cast<double, T>::impl((double)result["value"]);
        } else if (cls == "ndarray") {
            return Cast<NDArray, T>::impl(NDArray(base64_decode(result["data"]), result["shape"], result["dtype"]));
        } else if (cls == "string") {
            return Cast<std::string, T>::impl(std::string(result["value"]));
        } else {
            throw std::runtime_error("Unknown result type");
        }
    }
};

template <class V>
struct Cast<json, std::vector<V>> {
    static std::vector<V> impl(const json& result) {
        std::string cls = result["class"];
        if (cls == "list") {
            std::vector<V> v;
            for (json el : result["value"]) {
                v.push_back(Cast<json, V>::impl(el));
            }
            return v;
        } else {
            throw std::runtime_error("Unknown result type");
        }
    }
};

template <class V>
struct Cast<json, std::map<std::string, V>> {
    static std::map<std::string, V> impl(const json& result) {
        std::string cls = result["class"];
        if (cls == "dict") {
            std::map<std::string, V> m;
            for (auto& el : result["value"].items()) {
                m[el.key()] = Cast<json, V>::impl(el.value());
            }
            return m;
        } else {
            throw std::runtime_error("Unknown result type");
        }
    }
};

template <size_t I, size_t N>
struct TupleAssigner {
    template <class... T>
    static inline void impl(const json& v, std::tuple<T...>& t) {
        std::get<I>(t) =
                Cast<json, typename std::remove_reference<
                                   typename std::tuple_element<I, std::tuple<T...>>::type>::type>::impl(v[I]);
        TupleAssigner<I + 1, N>::impl(v, t);
    }
};

template <size_t N>
struct TupleAssigner<N, N> {
    template <class... T>
    static inline void impl(const json& v, std::tuple<T...>& t) {}
};

template <class... V>
struct Cast<json, std::tuple<V...>> {
    static std::tuple<V...> impl(const json& result) {
        std::string cls = result["class"];
        if (cls == "list") {
            std::tuple<V...> t;
            if (sizeof...(V) != result["value"].size()) {
                throw std::runtime_error("Inconsistent between tuple and value size");
            }
            TupleAssigner<0, sizeof...(V)>::impl(result["value"], t);
            return t;
        } else {
            throw std::runtime_error("Unknown result type");
        }
    }
};

class PyHandler {
public:
    static std::shared_ptr<PyHandler> instance() {
        static std::shared_ptr<PyHandler> instance;
        if (!instance) {
            instance.reset(new PyHandler());
        }
        return instance;
    }

private:
    PyHandler() {
        process = std::make_shared<Process>();
        func = [](std::array<int, 2> io_pipe) {
            std::string cmd =
#include "pyhandler.py"
                    ;
            cmd = cmd + "\n__main(" + std::to_string(io_pipe[0]) + ", " + std::to_string(io_pipe[1]) + ")\n";
            return execl("/usr/bin/python3", "/usr/bin/python3", "-c", cmd.c_str(), (char*)NULL);
        };
        process->start(func);
    }

    template <class Result>
    Result execute_with_data(json& data) {
        std::string result;
        if (!process->communicate_to_proc(data.dump(), result)) {
            throw std::runtime_error("Process failed");
        }
        return Cast<json, Result>::impl(json::parse(result));
    }

public:
    PyHandler(PyHandler const&) = delete;
    void operator=(PyHandler const&) = delete;

    virtual ~PyHandler() {
        process->write_to_proc("EXIT");
        process.reset();
    }

    template <class Result, class... Param>
    Result call(const std::string& func_name, const Param&... params) {
        json jparams = ParamEncoder<decltype(std::make_tuple(params...))>::impl(std::make_tuple(params...));
        json jcommand = json::array({"call", func_name, jparams});
        return this->execute_with_data<Result>(jcommand);
    }

    template <class... Param, size_t N = sizeof...(Param)>
    void set_vars(const std::array<std::string, N>& param_names, const Param&... params) {
        json jparams = ParamEncoder<decltype(std::make_tuple(params...))>::impl(std::make_tuple(params...));
        json jcommand = json::array({"set_vars", param_names, jparams});
        this->execute_with_data<void>(jcommand);
    }

    template <class Result>
    Result exec(const std::string& code, const std::string& result_expr) {
        json jcommand = json::array({"exec", code, result_expr});
        return this->execute_with_data<Result>(jcommand);
    }

    void exec_file(const std::string& file_path) {
        json jcommand = json::array({"exec_file", file_path});
        return this->execute_with_data<void>(jcommand);
    }

    std::shared_ptr<Process> process;
    std::function<int(std::array<int, 2>)> func;
};

inline std::shared_ptr<PyHandler> get_handler() {
    return PyHandler::instance();
}

template <class Result, class... Param>
Result call(const std::string& func_name, const Param&... params) {
    return get_handler()->call<Result, Param...>(func_name, params...);
}

template <class... Param>
void call(const std::string& func_name, const Param&... params) {
    return get_handler()->call<void, Param...>(func_name, params...);
}

template <class... Param, size_t N = sizeof...(Param)>
void set_vars(const std::array<std::string, N>& param_names, const Param&... params) {
    get_handler()->set_vars<Param...>(param_names, params...);
}

template <class Result>
Result exec(const std::string& code, const std::string& result_expr) {
    return get_handler()->exec<Result>(code, result_expr);
}

template <class Result>
Result exec(const std::string& result_expr) {
    return get_handler()->exec<Result>("None", result_expr);
}

void exec(const std::string& code) {
    get_handler()->exec<void>(code, "None");
}

void exec_file(const std::string& file_path) {
    get_handler()->exec_file(file_path);
}

};  // namespace pyhandler
