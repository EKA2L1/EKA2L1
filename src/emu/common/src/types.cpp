#include <common/types.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/mman.h>
#else
#include <Windows.h>
#endif

int translate_protection(prot cprot) {
    int tprot = 0;

    if (cprot == prot::none) {
#ifndef WIN32
        tprot = PROT_NONE;
#else
        tprot = PAGE_NOACCESS;
#endif
    } else if (cprot == prot::read) {
#ifndef WIN32
        tprot = PROT_READ;
#else
        tprot = PAGE_READONLY;
#endif
    } else if (cprot == prot::exec) {
#ifndef WIN32
        tprot = PROT_EXEC;
#else
        tprot = PAGE_EXECUTE;
#endif
    } else if (cprot == prot::read_write) {
#ifndef WIN32
        tprot = PROT_READ | PROT_WRITE;
#else
        tprot = PAGE_READWRITE;
#endif
    } else if (cprot == prot::read_exec) {
#ifndef WIN32
        tprot = PROT_READ | PROT_EXEC;
#else
        tprot = PAGE_EXECUTE_READ;
#endif
    } else if (cprot == prot::read_write_exec) {
#ifndef WIN32
        tprot = PROT_READ | PROT_WRITE | PROT_EXEC;
#else
        tprot = PAGE_EXECUTE_READWRITE;
#endif
    } else {
        tprot = -1;
    }

    return tprot;
}

