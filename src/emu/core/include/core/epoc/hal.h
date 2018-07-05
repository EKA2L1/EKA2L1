#pragma once

#include <functional>
#include <unordered_map>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    enum TKernelHalFunction {
        EKernelHalMemoryInfo,
        EKernelHalRomInfo,
        EKernelHalStartupReason,
        EKernelHalFaultReason,
        EKernelHalExceptionId,
        EKernelHalExceptionInfo,
        EKernelHalCpuInfo,
        EKernelHalPageSizeInBytes,
        EKernelHalTickPeriod,
        EKernelHalMemModelInfo,
        EKernelHalFastCounterFrequency,
        EKernelHalNTickPeriod,
        EKernelHalHardwareFloatingPoint,
        EKernelHalGetNonsecureClockOffset,
        EKernelHalSetNonsecureClockOffset,
        EKernelHalSmpSupported,
        EKernelHalNumLogicalCpus,
        EKernelHalSupervisorBarrier,
        EKernelHalFloatingPointSystemId,
        EKernelHalLockThreadToCpu,
        EKernelHalConfigFlags,
        EKernelHalCpuStates,
        EKernelHalSetNumberOfCpus,
    };

    using hal_func = std::function<int(int *, int *)>;
    using hal_cagetory_funcs = std::unordered_map<uint32_t, hal_func>; 

    struct hal {
        hal_cagetory_funcs funcs;
        eka2l1::system *sys;

    public:
        hal(eka2l1::system *sys);

        int do_hal(uint32_t func, int *a1, int *a2);
    };

    void init_hal(eka2l1::system *sys);
    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2);
}