#pragma once

#include "nlohmann/json.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <mutex>
#include <queue>
#include <thread>

namespace pyhandler {

using json = nlohmann::json;

class ReadBuffer {
public:
    ReadBuffer() {
        str_buf = "";
        pos = 0;
    }

    ssize_t read_from_fd(int fd) {
        ssize_t tot_bytes = 0;
        while (true) {
            ssize_t bytes = read(fd, read_buf.data(), read_buf.size());
            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    throw std::runtime_error("read_from_fd failed");
                }
            } else if (bytes == 0) {
                break;
            } else {
                str_buf.append(read_buf.data(), bytes);
                tot_bytes += bytes;
            }
        }
        return tot_bytes;
    }

    std::string block_readline(int fd) {
        while (!has_line()) {
            struct pollfd pfd = {fd, POLLIN};
            if (poll(&pfd, 1, 100) > 0) {
                while (read_from_fd(fd))
                    ;
            }
        }
        return read_line();
    }

    bool has_line() {
        while (pos < str_buf.size() && str_buf[pos] != '\n') {
            pos++;
        }
        return pos < str_buf.size();
    }

    std::string read_line() {
        if (!has_line()) {
            return "";
        }
        std::string line = str_buf.substr(0, pos);
        str_buf = str_buf.substr(pos + 1);
        pos = 0;
        return line;
    }

    std::array<char, 16384> read_buf;
    std::string str_buf;
    size_t pos;
};

class WriteBuffer {
public:
    WriteBuffer(int fd, const std::string& msg) {
        this->fd = fd;
        this->str_buf = msg + "\n";
        this->pos = 0;
    }

    ~WriteBuffer() { fsync(fd); }

    ssize_t write_to_fd() {
        ssize_t tot_bytes = 0;
        while (true) {
            ssize_t bytes = write(fd, str_buf.data() + pos, str_buf.length() - pos);
            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    throw std::runtime_error("write buffer failed");
                }
            } else if (bytes == 0) {
                break;
            } else {
                pos += bytes;
                tot_bytes += bytes;
            }
        }
        return tot_bytes;
    }

    void block_write() {
        while (remain()) {
            struct pollfd pfd = {fd, POLLOUT};
            if (poll(&pfd, 1, 100) > 0) {
                while (write_to_fd())
                    ;
            }
        }
    }

    int remain() { return str_buf.size() - pos; }

    int fd;
    std::string str_buf;
    ssize_t pos;
};

class Process {
public:
    Process() {
        if (pipe(to_child.data()) == -1 || pipe(to_parent.data()) == -1) {
            throw std::runtime_error("create pipe failed");
        }
        set_fd_nonblock(to_child[1]);
        set_fd_nonblock(to_parent[0]);
    }

    ~Process() {
        join();
        close(to_child[0]);
        close(to_child[1]);
        close(to_parent[0]);
        close(to_parent[1]);
    }

    template <class F, class... Args>
    void start(const F& func, const Args&... args) {
        std::array<int, 2> child_io_pipe = {to_child[0], to_parent[1]};

        pid_t p = fork();
        if (p > 0) {
            pid = p;
            proc_is_alive = true;
        } else {
            prctl(PR_SET_PDEATHSIG, SIGHUP);
            exit(func(child_io_pipe, args...));
        }
    }

    void set_fd_nonblock(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("fcntl failed");
        }
        flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        if (flags == -1) {
            throw std::runtime_error("fcntl failed");
        }
    }

    bool is_alive() {
        if (!proc_is_alive) {
            return false;
        }
        int status;
        int ret = waitpid(pid, &status, WNOHANG);
        if (ret == -1) {
            throw std::runtime_error("wait pid failed");
        }
        if (ret > 0) {
            if (WIFEXITED(status)) {
                proc_is_alive = false;
            } else {
                proc_is_alive = false;
            }
        } else {
            proc_is_alive = true;
        }
        return proc_is_alive;
    }

    void join() {
        while (is_alive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool write_to_proc(const std::string& msg) {
        if (is_alive()) {
            WriteBuffer buf(to_child[1], msg);
            while (is_alive() && buf.remain()) {
                struct pollfd pfd = {to_child[1], POLLOUT};
                if (poll(&pfd, 1, 100) > 0) {
                    while (buf.write_to_fd())
                        ;
                }
            }
        }
        return is_alive();
    }

    bool read_from_proc(std::string& msg) {
        if (is_alive()) {
            ReadBuffer buf = {};
            while (is_alive()) {
                struct pollfd pfd = {to_parent[0], POLLIN};
                if (poll(&pfd, 1, 100) > 0) {
                    while (buf.read_from_fd(to_parent[0]))
                        ;
                    if (buf.has_line()) {
                        msg = buf.read_line();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool communicate_to_proc(const std::string& input, std::string& output) {
        if (!write_to_proc(input)) {
            return false;
        }
        if (!read_from_proc(output)) {
            return false;
        }
        return true;
    }

private:
    pid_t pid;
    std::array<int, 2> to_child;
    std::array<int, 2> to_parent;
    bool proc_is_alive;
};

template <class F, class Args, class Callback>
void execute_tasks(size_t num_workers, const F& func, const std::vector<Args>& args, const Callback& callback);

template <
        class F, class Args, class Callback = std::function<void(size_t, typename std::result_of<F&(Args)>::type)>,
        class R = typename std::result_of<F&(Args)>::type>
class TaskExecutor {
private:
    TaskExecutor() = delete;

    explicit TaskExecutor(size_t num_workers) { this->num_workers = num_workers; }

    static int execute_on_child(const std::array<int, 2>& io_pipe, const F& func, const std::vector<Args>& args) {
        while (true) {
            ReadBuffer rbuf = {};
            int idx = std::stoi(rbuf.block_readline(io_pipe[0]));
            if (idx == -1) {
                return 0;
            }
            if (idx < 0 || idx >= (int)args.size()) {
                throw std::runtime_error("message can not be decoded");
            }

            auto ret = func(args[idx]);
            json jret = ret;

            WriteBuffer wbuf(io_pipe[1], jret.dump());
            wbuf.block_write();
        }
        return -1;
    }

    void control_worker(
            const F& func, const std::vector<Args>& args, std::queue<size_t>& input_idxes, const Callback& callback) {
        std::shared_ptr<Process> child = nullptr;

        while (true) {
            int selected_idx = -1;
            {
                std::lock_guard<std::mutex> guard(task_mutex);
                if (!input_idxes.empty()) {
                    selected_idx = input_idxes.front();
                    input_idxes.pop();
                }
            }

            if (!child || !child->is_alive()) {
                child = std::make_shared<Process>();
                child->start(execute_on_child, func, args);
            }

            if (selected_idx == -1) {
                child->write_to_proc("-1");
                child->join();
                return;
            }

            bool write_success = child->write_to_proc(std::to_string(selected_idx));

            bool success = false;
            if (write_success) {
                std::string msg;
                bool read_success = child->read_from_proc(msg);
                if (read_success) {
                    std::lock_guard<std::mutex> guard(task_mutex);
                    callback(selected_idx, json::parse(msg));
                    success = true;
                }
            }
            if (!success) {
                throw std::runtime_error("execute task failed");
            }
        }
    }

    void execute(const F& func, const std::vector<Args>& args, const Callback& callback) {
        std::vector<std::shared_ptr<std::thread>> workers = {};
        std::queue<size_t> input_idxes;
        for (size_t i = 0; i < args.size(); ++i) {
            input_idxes.push(i);
        }
        auto worker_func = [&]() { control_worker(func, args, input_idxes, callback); };
        for (size_t i = 0; i < num_workers; ++i) {
            workers.push_back(std::make_shared<std::thread>(worker_func));
        }
        for (size_t i = 0; i < num_workers; ++i) {
            workers[i]->join();
        }
    }

    friend void execute_tasks<F, Args, Callback>(
            size_t num_workers, const F& func, const std::vector<Args>& args, const Callback& callback);

private:
    size_t num_workers;
    std::mutex task_mutex;
    std::mutex callback_mutex;
};

template <class F, class Args, class Callback>
void execute_tasks(size_t num_workers, const F& func, const std::vector<Args>& args, const Callback& callback) {
    TaskExecutor<F, Args, Callback> f(num_workers);
    f.execute(func, args, callback);
}

}  // namespace pyhandler
