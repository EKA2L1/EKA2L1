#include <epoc9/cons.h>
#include <epoc9/user.h>

#include <common/cvt.h>
#include <hle/libmanager.h>

BRIDGE_FUNC(eka2l1::ptr<CConsoleBase>, ConsoleNewL, eka2l1::ptr<TLit> aName, TSize aSize) {
    eka2l1::ptr<CConsoleBaseAdvance> console = UserAllocZL(sys, sizeof(CConsoleBaseAdvance)).cast<CConsoleBaseAdvance>();
    CConsoleBaseAdvance *cons_ptr = console.get(sys->get_memory_system());

    memory_system *mem = sys->get_memory_system();

    cons_ptr->iVtable = sys->get_lib_manager()->get_vtable_address("CConsoleBase");

    // Overwrite with our own provide
    (cons_ptr->iVtable.cast<uint32_t>().get(mem))[6] = 0x802CDCC0;

    cons_ptr->iConsoleSize = aSize;

    if (aName) {
        TLit name = *aName.get(mem);

        cons_ptr->iNameLen = name.iTypeLength;
        cons_ptr->iName = ptr<TUint16>(aName.ptr_address() + 4);

        TUint16 *name_ptr = GetLit16Ptr(mem, aName);
    
        LOG_INFO("Console created: {}", common::ucs2_to_utf8(std::u16string(name_ptr, name_ptr + name.iTypeLength)));
    }
    else {
        cons_ptr->iNameLen = 0;
    }

    return console.cast<CConsoleBase>();
}

BRIDGE_FUNC(void, CConsoleBaseAdvanceWrite, eka2l1::ptr<CConsoleBase> aConsole, eka2l1::ptr<TDesC> aDes) {
    memory_system *mem = sys->get_memory_system();
    TUint16 *name_ptr = GetTDes16Ptr(sys, aDes.get(mem));
    TDesC16* trye = aDes.get(mem);
    auto len = aDes.get(mem)->iLength & 0xfffffff;
    LOG_INFO("{}", common::ucs2_to_utf8(std::u16string(name_ptr, name_ptr + len)));
}

BRIDGE_FUNC(void, CConsoleBaseGetch) {
    LOG_INFO("Us assumes someone press the key, no problem here.");
}

const eka2l1::hle::func_map cons_register_funcs = {
    BRIDGE_REGISTER(4262343383, ConsoleNewL),
    BRIDGE_REGISTER(4012343003, CConsoleBaseGetch)
};

const eka2l1::hle::func_map cons_custom_register_funcs = {
    BRIDGE_REGISTER(0x802CDCC0, CConsoleBaseAdvanceWrite)
};