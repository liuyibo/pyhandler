#include "pyhandler/pyhandler.hpp"

#include <iostream>
#include <vector>
#include <string>


namespace ph = pyhandler;


int main() {
    {
        auto v = ph::call<int>("sum", std::vector<int>{1, 2, 3, 4, 5});
        std::cout << "Sum(1, 2, 3, 4, 5) = " << v << std::endl;
    }

    {
        ph::set_vars({"a", "b"}, 12, 34);
        auto v = ph::exec<std::string>("str(a ** b)");
        std::cout << "12 ** 34 = " << v << std::endl;
    }

    {
        ph::set_vars({"msg"}, "123321");
        auto v = ph::exec<std::string>(R"(
import base64
msg_b64 = base64.b64encode(msg.encode()).decode()
)", "f'Message: {msg}, Base64Encoded: {msg_b64}'");
        std::cout << v << std::endl;
    }

    {
        auto t1 = ph::exec<long long>("int(time.time() * 1000)");
        for (int i = 0; i < 1000; ++i) {
            ph::exec<long long>("int(time.time() * 1000)");
        }
        auto t2 = ph::exec<long long>("int(time.time() * 1000)");
        std::cout << "Command time cost: " << t2 -  t1 << " us" << std::endl;
    }

    {
        ph::exec("t0 = time.time()");
        std::vector<uint8_t> data(1000000, 123);
        auto v = ph::call<int>("lambda x: int(np.sum(x))", ph::NDArray(data.data(), {data.size()}));
        ph::exec("print(f'C++ -> Python: {1.0 / (time.time() - t0)} MB/s')");
    }

    {
        ph::exec("t0 = time.time()");
        auto v = ph::exec<ph::NDArray>("np.ones(1000000, 'uint8')");
        ph::exec("print(f'Python -> C++: {1.0 / (time.time() - t0)} MB/s')");
    }

    {
        ph::exec_file("funcs.py");
        ph::call("lets_233", "abcde");
        ph::call("lets_233", "qwertyasdf");
    }

    return 0;
}
