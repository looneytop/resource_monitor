#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

// структура для хранения инфы о процессе
struct ProcessInfo {
    int pid;
    std::string name;
    long memory_kb;
};

// структура расширенных метрик
struct SystemStats {
    double cpu_usage_percent = 0.0;
    double ram_total_gb = 0.0;
    double ram_used_gb = 0.0;
    double ram_free_gb = 0.0;
    double swap_total_gb = 0.0;
    double swap_used_gb = 0.0;
    double load_avg_1m = 0.0;
    double load_avg_5m = 0.0;
    double load_avg_15m = 0.0;
    std::vector<ProcessInfo> top_processes;
};

class SystemCollector {
public:
    SystemCollector();
    ~SystemCollector();
    SystemStats getstats(); // получение кэшированных данных

private:
    SystemStats current_stats;
    std::mutex mtx;
    std::atomic<bool> running;
    std::thread worker_thread;

    // переменные для вычисления дельты cpu
    long prev_idle = 0;
    long prev_total = 0;

    void update_loop();
    void calculate_cpu(SystemStats& stats);
    void calculate_ram(SystemStats& stats);
    void calculate_load_avg(SystemStats& stats);
    void calculate_processes(SystemStats& stats);
};