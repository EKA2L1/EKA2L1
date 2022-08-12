#include "ui_launch_process_dialog.h"
#include <qt/launch_process_dialog.h>

#include <kernel/kernel.h>
#include <QMessageBox>

launch_process_dialog::launch_process_dialog(QWidget *parent, eka2l1::kernel_system *kern)
    : QDialog(parent)
    , ui_(new Ui::launch_process_dialog)
    , kern_(kern) {
    setAttribute(Qt::WA_DeleteOnClose);
    ui_->setupUi(this);
}

launch_process_dialog::~launch_process_dialog() {
    delete ui_;
}

void launch_process_dialog::on_launch_button_clicked() {
    QString launch_path = ui_->launch_path_line_edit->text();
    if (launch_path.isEmpty()) {
        QMessageBox::critical(this, tr("Launch failed"), tr("The executable path field is empty!"));
        return;
    }

    QString arg = ui_->arguments_line_edit->text();

    kern_->lock();
    eka2l1::kernel::process *process_ptr = kern_->spawn_new_process(launch_path.toStdU16String(), arg.toStdU16String());

    if (!process_ptr) {
        kern_->unlock();
        QMessageBox::critical(this, tr("Launch failed"), tr("The executable path is invalid or the executable is corrupted. Check the log for more info!"));

        return;
    }

    process_ptr->run();
    kern_->unlock();

    emit launched();
    close();
}

void launch_process_dialog::on_cancel_button_clicked() {
    close();
}
