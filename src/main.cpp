#include <iostream>
#include <fstream>
#include <sstream>
#include "httplib.h"
#include "../include/SystemCollector.hpp"

// функция ручной сборки json для исключения внешних зависимостей
std::string build_stats_json(const SystemStats& stats) {
    std::stringstream ss;
    ss << "{";
    ss << "\"cpu\":" << stats.cpu_usage_percent << ",";
    ss << "\"ram_total\":" << stats.ram_total_gb << ",";
    ss << "\"ram_used\":" << stats.ram_used_gb << ",";
    ss << "\"ram_free\":" << stats.ram_free_gb << ",";
    ss << "\"swap_total\":" << stats.swap_total_gb << ",";
    ss << "\"swap_used\":" << stats.swap_used_gb << ",";
    ss << "\"load_1\":" << stats.load_avg_1m << ",";
    ss << "\"load_5\":" << stats.load_avg_5m << ",";
    ss << "\"load_15\":" << stats.load_avg_15m << ",";
    ss << "\"processes\":[";
    for (size_t i = 0; i < stats.top_processes.size(); ++i) {
        const auto& p = stats.top_processes[i];
        ss << "{";
        ss << "\"pid\":" << p.pid << ",";
        ss << "\"name\":\"" << p.name << "\",";
        ss << "\"mem\":" << p.memory_kb;
        ss << "}";
        if (i + 1 < stats.top_processes.size()) ss << ",";
    }
    ss << "]";
    ss << "}";
    return ss.str();
}

int main() {
    httplib::Server svr;
    SystemCollector collector;

    // отдаем json
    svr.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
        SystemStats stats = collector.getstats();
        res.set_content(build_stats_json(stats), "application/json");
    });

    // отдаем веб пейдж
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("index.html");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else {
            res.set_content("<h1>Error: index.html not found</h1>", "text/html");
        }
    });

    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    
    return 0;
}