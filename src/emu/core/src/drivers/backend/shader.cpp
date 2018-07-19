#include <core/drivers/backend/shader.h>
#include <fstream>

namespace eka2l1 {
    namespace driver {
        bool shader::load(const std::string &path, const shader_type type) {
            std::ifstream input(path, std::ifstream::binary);

            if (input.fail()) {
                return false;
            }

            size_t size = 0;
            input.seekg(0, input.end);
            size = input.tellg();

            std::string binary_data;
            binary_data.resize(size);

            input.read(binary_data.data(), size);

            return load(binary_data.c_str(), size, type);
        }
    }
}