//
// Created by captainchen on 2025/1/6.
//

#ifndef EXAMPLE_PROFILER_APP_MAIN_WINDOW_H
#define EXAMPLE_PROFILER_APP_MAIN_WINDOW_H


#include <iostream>

class MainWindow {
public:
    void ShowProgress(bool show) {
        // Show progress
        std::cout << "Show progress" << std::endl;
    }

    void UpdateProgress(int progress) {
        // Update progress
        std::cout << "Update progress: " << progress << std::endl;
    }
};


#endif //EXAMPLE_PROFILER_APP_MAIN_WINDOW_H
