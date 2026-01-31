#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
#include <algorithm>

using namespace std;
using namespace chrono;


time_t now() { return system_clock::to_time_t(system_clock::now()); }

void rotate_log(const string& fname, time_t limit_sec) {
    vector<pair<time_t, string>> entries;
    ifstream in(fname);
    string line;
    while (getline(in, line)) {
        istringstream iss(line);
        time_t ts; string rest;
        if (iss >> ts) {
            getline(iss, rest);
            if (now() - ts <= limit_sec)
                entries.emplace_back(ts, rest);
        }
    }
    ofstream out(fname);
    for (auto& [ts, data] : entries)
        out << ts << data << "\n";
}

void log_temp(const string& fname, time_t ts, float temp, time_t keep_sec) {
    ofstream out(fname, ios::app);
    out << ts << " " << temp << "\n";
    rotate_log(fname, keep_sec);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: logger <device_path>\nExample: logger /tmp/ttyVIRT\n";
        return 1;
    }

    ifstream dev(argv[1]);
    if (!dev.is_open()) {
        cerr << "Cannot open " << argv[1] << "\n";
        return 1;
    }

    cout << "Logger started, reading from " << argv[1] << " ...\n";
    cout << "Logs: detailed.log (24h), hourly.log (30d), daily.log (365d)\n";

    time_t last_hour = now() / 3600, last_day = now() / 86400;
    vector<float> hour_vals, day_vals;

    string line;
    while (getline(dev, line)) {
        time_t ts = now();
        float temp = stof(line);

        log_temp("detailed.log", ts, temp, 86400);

        hour_vals.push_back(temp);
        day_vals.push_back(temp);

        time_t cur_hour = ts / 3600;
        if (cur_hour > last_hour && !hour_vals.empty()) {
            float avg = accumulate(hour_vals.begin(), hour_vals.end(), 0.0f) / hour_vals.size();
            log_temp("hourly.log", last_hour * 3600, avg, 2592000); // 30 дней
            hour_vals.clear();
            last_hour = cur_hour;
        }

        time_t cur_day = ts / 86400;
        if (cur_day > last_day && !day_vals.empty()) {
            float avg = accumulate(day_vals.begin(), day_vals.end(), 0.0f) / day_vals.size();
            log_temp("daily.log", last_day * 86400, avg, 31536000); // 365 дней
            day_vals.clear();
            last_day = cur_day;
        }

        this_thread::sleep_for(milliseconds(100));
    }
    return 0;
}