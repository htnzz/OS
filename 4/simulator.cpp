#include <iostream>
#include <fstream>
#include <thread>
#include <random>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: simulator <device_path>\nExample: simulator /tmp/ttyVIRT\n";
        return 1;
    }

    std::ofstream dev(argv[1]);
    if (!dev.is_open()) {
        std::cerr << "Cannot open " << argv[1] << "\n";
        return 1;
    }

    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<> dis(20.0, 30.0);

    std::cout << "Simulator started, writing to " << argv[1] << " ...\n";
    while (true) {
        float temp = dis(gen);
        dev << temp << "\n";
        dev.flush();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}