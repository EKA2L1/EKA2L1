#include <epoc9/lock.h>
#include <epoc9/user.h>

#include <common/random.h>
#include <common/cvt.h>

BRIDGE_FUNC(TInt, RSemaphoreOpen, eka2l1::ptr<RSemaphore> aSema, TInt aSlot, TOwnerType aOwnerType) {
    kernel_system *kern = sys->get_kernel_system();
    memory_system *mem = sys->get_memory_system();

    process_ptr pr = kern->get_process(kern->crr_process());

    std::optional<pass_arg> slot = pr->get_arg_slot(aSlot);

    if (!slot) {
        return KErrNotFound;
    }

    // Maybe it is handle
    TInt maybeHandle = slot->data;
    kernel_obj_ptr objSema = kern->get_kernel_obj(maybeHandle);

    if (!objSema || objSema->get_object_type() != kernel::object_type::sema) {
        return KErrArgument;
    }

    RSemaphore *sema = aSema.get(mem);
    sema->iHandle = maybeHandle;

    objSema->set_owner_type(aOwnerType == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread);

    return KErrNone;
}

BRIDGE_FUNC(TInt, RSemaphoreCreateLocal, eka2l1::ptr<RSemaphore> aSema, TInt aInitCount, TOwnerType aOwnerType) {
    kernel_system *kern = sys->get_kernel_system();
    memory_system *mem = sys->get_memory_system();

    if (aInitCount < 0) {
        // It supposed to panic
        LOG_ERROR("Semaphore init count is negative");
        return KErrArgument;
    }

    sema_ptr sema_hle = kern->create_sema("sema" + eka2l1::common::to_string(eka2l1::random()),
        aInitCount, 50, aOwnerType == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread);

    RSemaphore *sema_lle = aSema.get(mem);
    sema_lle->iHandle = sema_hle->unique_id();

    return KErrNone;
}

BRIDGE_FUNC(TInt, RSemaphoreCreateGlobal, eka2l1::ptr<RSemaphore> aSema, eka2l1::ptr<TDesC> aName, TInt aInitCount, TOwnerType aOwnerType) {
    kernel_system *kern = sys->get_kernel_system();
    memory_system *mem = sys->get_memory_system();

    if (aInitCount < 0) {
        // It supposed to panic
        LOG_ERROR("Semaphore init count is negative");
        return KErrArgument;
    }

    TDesC *des_name = aName.get(mem);
    kernel::owner_type otype = aOwnerType == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread;

    sema_ptr sema_hle = kern->create_sema(common::ucs2_to_utf8(des_name->StdString(sys)),
        aInitCount, 50, otype,
        kern->get_id_base_owner(otype), kernel::access_type::global_access);

    RSemaphore *sema_lle = aSema.get(mem);
    sema_lle->iHandle = sema_hle->unique_id();

    return KErrNone;
}

const eka2l1::hle::func_map lock_register_funcs = {
    BRIDGE_REGISTER(1748411401, RSemaphoreCreateGlobal),
    BRIDGE_REGISTER(679899028, RSemaphoreCreateLocal),
    BRIDGE_REGISTER(1741115737, RSemaphoreOpen)
};