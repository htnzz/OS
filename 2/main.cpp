#include <iostream>
#include <thread>
#include <chrono>
#include "process.hpp"

int main() {
    BackgroundProcess proc;
    
#ifdef _WIN32
    if (!proc.start("cmd /c \"echo Hello from Windows & timeout /t 2 >nul\"")) {
#else
    if (!proc.start("sh -c 'echo Hello from Ubuntu; sleep 2'")) {
#endif
        std::cerr << "Ошибка запуска!\n";
        return 1;
    }

    std::cout << "Процесс запущен в фоне...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Ожидаем завершения...\n";

    int code = proc.wait();
    std::cout << "Процесс завершён с кодом: " << code << "\n";
    return 0;
}