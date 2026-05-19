#include "../include/SystemCollector.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <cstdlib>

SystemCollector::SystemCollector() : running(true) {
    worker_thread = std::thread(&SystemCollector::update_loop, this);
}

SystemCollector::~SystemCollector() {
    running = false;
    if (worker_thread.joinable()) worker_thread.join();
}

void SystemCollector::update_loop() {
    while (running) {
        SystemStats local_stats;
        
        // сборка всех метрик в временную переменную
        calculate_cpu(local_stats);
        calculate_ram(local_stats);
        calculate_load_avg(local_stats);
        calculate_processes(local_stats);

        // обновление глобального кэша под мьютексом
        {
            std::lock_guard<std::mutex> lock(mtx);
            current_stats = local_stats;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

SystemStats SystemCollector::getstats() {
    std::lock_guard<std::mutex> lock(mtx);
    return current_stats;
}

void SystemCollector::calculate_cpu(SystemStats& stats) {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return;

    std::string line;
    std::getline(file, line);
    std::stringstream ss(line);
    std::string cpu_label;
    ss >> cpu_label;

    long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    if (ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice) {
        long current_idle = idle + iowait;
        long current_non_idle = user + nice + system + irq + softirq + steal;
        long current_total = current_idle + current_non_idle;

        long total_delta = current_total - prev_total;
        long idle_delta = current_idle - prev_idle;

        if (total_delta > 0) {
            stats.cpu_usage_percent = 100.0 * (total_delta - idle_delta) / total_delta;
        }

        prev_idle = current_idle;
        prev_total = current_total;
    }
}

void SystemCollector::calculate_ram(SystemStats& stats) {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return;

    std::string key;
    long value;
    std::string unit;

    long mem_total = 0, mem_avail = 0, swap_total = 0, swap_free = 0;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") mem_total = value;
        else if (key == "MemAvailable:") mem_avail = value;
        else if (key == "SwapTotal:") swap_total = value;
        else if (key == "SwapFree:") swap_free = value;
    }

    stats.ram_total_gb = mem_total / 1024.0 / 1024.0;
    stats.ram_free_gb = mem_avail / 1024.0 / 1024.0;
    stats.ram_used_gb = stats.ram_total_gb - stats.ram_free_gb;

    stats.swap_total_gb = swap_total / 1024.0 / 1024.0;
    stats.swap_used_gb = stats.swap_total_gb - (swap_free / 1024.0 / 1024.0);
}

void SystemCollector::calculate_load_avg(SystemStats& stats) {
    double load[3];
    if (getloadavg(load, 3) != -1) {
        stats.load_avg_1m = load[0];
        stats.load_avg_5m = load[1];
        stats.load_avg_15m = load[2];
    }
}

void SystemCollector::calculate_processes(SystemStats& stats) {
    DIR* dir = opendir("/proc");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string d_name = entry->d_name;
            // проверка что это PID
            if (!d_name.empty() && std::all_of(d_name.begin(), d_name.end(), ::isdigit)) {
                int pid = std::stoi(d_name);
                
                std::ifstream stat_file("/proc/" + d_name + "/stat");
                std::ifstream statm_file("/proc/" + d_name + "/statm");
                
                if (stat_file.is_open() && statm_file.is_open()) {
                    std::string comm;
                    int p;
                    stat_file >> p >> comm; // taking name of process
                    if (comm.size() > 2 && comm.front() == '(' && comm.back() == ')') {
                        comm = comm.substr(1, comm.size() - 2);
                    }

                    long size, resident;
                    statm_file >> size >> resident; // first - size, sec - RSS  
                    long memory_kb = resident * sysconf(_SC_PAGESIZE) / 1024;

                    ProcessInfo proc{pid, comm, memory_kb};
                    stats.top_processes.push_back(proc);
                }
            }
        }
    }
    closedir(dir);

    // сортировка процессов по потребляемым ресурсам 
    std::sort(stats.top_processes.begin(), stats.top_processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.memory_kb > b.memory_kb;
    });

    // топ 10 процессов
    if (stats.top_processes.size() > 10) {
        stats.top_processes.resize(10);
    }
}