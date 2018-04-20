#pragma once

#include <string>

namespace eka2l1 {
    class process {
        uint32_t uid;
        std::string process_name;

        uint32_t entry_point_addr;

    public:
        process(uint32_t uid, uint32_t epa,
                const std::string& process_name);

        // Create a new thread and run
        // No arguments provided
        bool run();

        // Step through instructions
        bool step();

        // Stop the main process thread
        bool stop();
    };
}
