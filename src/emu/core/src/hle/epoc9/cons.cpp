#include <epoc9/cons.h>
#include <epoc9/user.h>

#include <common/cvt.h>
#include <hle/libmanager.h>

BRIDGE_FUNC(eka2l1::ptr<CConsoleBase>, ConsoleNewL, eka2l1::ptr<TLit> aName, TSize aSize) {
    eka2l1::ptr<CConsoleBaseAdvance> console = UserAllocZL(sys, sizeof(CConsoleBaseAdvance)).cast<CConsoleBaseAdvance>();
    CConsoleBaseAdvance *cons_ptr = console.get(sys->get_memory_system());

    memory_system *mem = sys->get_memory_system();

    cons_ptr->iVtable = sys->get_lib_manager()->get_vtable_address("CConsoleBase");

	uint32_t write_ptr6 = cons_ptr->iVtable.ptr_address() + 24;

    // Overwrite with our own provide
    //(cons_ptr->iVtable.cast<uint32_t>().get(mem))[6] = sys->ge;
	sys->get_lib_manager()->register_custom_func(BRIDGE_REGISTER(write_ptr6, CConsoleBaseAdvanceWrite));

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

    cons_ptr->iCurrentX = 12;
    cons_ptr->iCurrentY = sys->get_screen_driver()->get_window_size().y - 12;

    auto render_my_func = [](eka2l1::system *sys, CConsoleBaseAdvance *adv) {
        TUint32 old_x = adv->iCurrentX;
        TUint32 old_y = adv->iCurrentY;

        uint32_t i = 0;

        for (auto &text: adv->iPrintList) {
            point temp = { static_cast<const int>(adv->iCurrentX), static_cast<const int>(adv->iCurrentY) };
            sys->get_screen_driver()->blit(text, temp);

            adv->iCurrentX = temp.x;
            adv->iCurrentY = temp.y;
        }

        adv->iCurrentX = old_x;
        adv->iCurrentY = old_y;
    };

    sys->get_screen_driver()->register_render_func(std::bind(render_my_func, sys, cons_ptr));

    return console.cast<CConsoleBase>();
}

BRIDGE_FUNC(void, CConsoleBaseAdvanceWrite, eka2l1::ptr<CConsoleBase> aConsole, eka2l1::ptr<TDesC> aDes) {
    CConsoleBaseAdvance *cons_ptr = aConsole.cast<CConsoleBaseAdvance>().get(sys->get_memory_system());
    auto &print_list = cons_ptr->iPrintList;

    memory_system *mem = sys->get_memory_system();
    TUint16 *name_ptr = GetTDes16Ptr(sys, aDes.get(mem));
    TDesC16* trye = aDes.get(mem);
    auto len = aDes.get(mem)->iLength & 0xfffffff;

    std::string print = common::ucs2_to_utf8(std::u16string(name_ptr, name_ptr + len));

    LOG_INFO("{}", print);
    print_list.push_back(print);
}

BRIDGE_FUNC(void, CConsoleBaseGetch) {
    LOG_INFO("Us assumes someone press the key, no problem here.");
}

const eka2l1::hle::func_map cons_register_funcs = {
    BRIDGE_REGISTER(4262343383, ConsoleNewL),
    BRIDGE_REGISTER(4012343003, CConsoleBaseGetch)
};

const eka2l1::hle::func_map cons_custom_register_funcs = {
};