#include "GUI.hpp"

void GUI::run() {
    while (true) {
        sleep(2);
        std::lock_guard<std::mutex> lock(monitor_.getMutex());

        std::cout << "Device list:\n";
        for (auto I = monitor_.cbegin(); I != monitor_.cend(); ++I) {
            std::cout << "ID: " << I->second.getId() << " Status: " 
                      << static_cast<int>(I->second.getStatus()) << '\n'; 
        }
        std::cout << std::endl;
    }
}