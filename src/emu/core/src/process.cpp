#include <process.h>

namespace eka2l1 {
    process::process(uint32_t uid, uint32_t epa,
        const std::string& process_name)
        : uid(uid), process_name(process_name),
          entry_point_addr(epa) {}
}
