#include <qt/package_manager_dialog.h>
#include "ui_package_manager_dialog.h"

#include <package/manager.h>
#include <loader/sis_fields.h>

#include <QMenu>
#include <QMessageBox>
#include <QListWidget>
#include <QGridLayout>

#include <common/log.h>

package_manager_table_info_item::package_manager_table_info_item(const std::uint32_t uid, const int index)
    : QTableWidgetItem(QString("0x%1").arg(uid, 8, 16, QChar('0')))
    , uid_(uid)
    , index_(index) {
}

package_manager_dialog::package_manager_dialog(QWidget *parent, eka2l1::manager::packages *packages) :
    QDialog(parent),
    packages_(packages),
    ui_(new Ui::package_manager_dialog)
{
    ui_->setupUi(this);
    refresh_package_manager_items();

    connect(ui_->package_list_search_edit, &QLineEdit::textChanged, this, &package_manager_dialog::on_search_bar_edit);
    connect(ui_->package_list_widget, &QWidget::customContextMenuRequested, this, &package_manager_dialog::on_custom_menu_requested);

    connect(ui_->action_package_uninstall, &QAction::triggered, this, &package_manager_dialog::on_package_uninstall_requested);
    connect(ui_->action_package_list_files, &QAction::triggered, this, &package_manager_dialog::on_package_list_file_requested);

    setAttribute(Qt::WA_DeleteOnClose);
}

package_manager_dialog::~package_manager_dialog()
{
    delete ui_;
}

void package_manager_dialog::refresh_package_manager_items() {
    ui_->package_list_widget->setRowCount(0);

    int current_row = 0;

    for (auto &package: *packages_) {
        if (package.second.is_removable) {
            ui_->package_list_widget->insertRow(ui_->package_list_widget->rowCount());

            package_manager_table_info_item *uid_item = new package_manager_table_info_item(package.first, package.second.index);
            ui_->package_list_widget->setItem(current_row, 0, uid_item);
            ui_->package_list_widget->setItem(current_row, 1, new QTableWidgetItem(QString::fromUtf16(package.second.package_name.c_str())));
            ui_->package_list_widget->setItem(current_row, 2, new QTableWidgetItem(QString::fromUtf16(package.second.vendor_name.c_str())));

            QString all_drives_str;
            for (drive_number drv = drive_a; drv <= drive_z; drv = static_cast<drive_number>(static_cast<int>(drv) + 1)) {
                if (package.second.drives & (1 << static_cast<int>(drv))) {
                    all_drives_str += QString("%1:, ").arg(QString(QChar('A' + static_cast<int>(drv))));
                }
            }

            if (!all_drives_str.isEmpty()) {
                all_drives_str = all_drives_str.left(all_drives_str.length() - 2);
            }

            ui_->package_list_widget->setItem(current_row, 3, new QTableWidgetItem(all_drives_str));
            current_row++;
        }
    }
}

void package_manager_dialog::on_search_bar_edit(const QString &new_text) {
    if (new_text.isEmpty()) {
        for (qsizetype i = 0; i < ui_->package_list_widget->rowCount(); i++) {
            ui_->package_list_widget->showRow(i);
        }

        return;
    }

    for (qsizetype i = 0; i < ui_->package_list_widget->rowCount(); i++) {
        QTableWidgetItem *item = ui_->package_list_widget->item(i, 1);
        if (item && item->text().contains(new_text, Qt::CaseInsensitive)) {
            ui_->package_list_widget->showRow(i);
        } else {
            ui_->package_list_widget->hideRow(i);
        }
    }
}

void package_manager_dialog::on_custom_menu_requested(const QPoint &pos) {
    QTableWidgetItem *item = ui_->package_list_widget->itemAt(pos);
    if (item) {
        current_item_ = reinterpret_cast<package_manager_table_info_item*>(ui_->package_list_widget->item(item->row(), 0));

        QMenu the_menu;
        the_menu.addAction(ui_->action_package_uninstall);
        the_menu.addAction(ui_->action_package_list_files);

        the_menu.exec(ui_->package_list_widget->mapToGlobal(pos));
    }
}

void package_manager_dialog::on_package_uninstall_requested() {
    eka2l1::package::object *pkg = packages_->package(current_item_->uid(), current_item_->index());
    if (pkg) {
        if (!packages_->uninstall_package(*pkg)) {
            QMessageBox::critical(this, tr("Uninstall failed!"), tr("Failed to uninstall the package"));
        }

        refresh_package_manager_items();
    }
}

void package_manager_dialog::on_package_list_file_requested() {
    eka2l1::package::object *pkg = packages_->package(current_item_->uid(), current_item_->index());
    if (!pkg) {
        return;
    }

    QDialog file_list_dialog(this);
    QGridLayout file_list_layout;

    file_list_dialog.setWindowTitle(tr("%1's files").arg(QString::fromStdU16String(pkg->package_name)));
    file_list_dialog.setMinimumSize(QSize(500, 500));
    file_list_dialog.setLayout(&file_list_layout);

    QListWidget file_list_widget;
    for (std::size_t i = 0; i < pkg->file_descriptions.size(); i++) {
        QString status;
        switch (static_cast<eka2l1::loader::ss_op>(pkg->file_descriptions[i].operation)) {
        case eka2l1::loader::ss_op::install:
            status = tr("Installed");
            break;

        case eka2l1::loader::ss_op::null:
            status = tr("Delete on uninstall");
            break;

        default:
            break;
        }

        QString file_status_final = QString::fromStdU16String(pkg->file_descriptions[i].target);
        if (!status.isEmpty()) {
            file_status_final += QString(" (%1)").arg(status);
        }

        file_list_widget.addItem(file_status_final);
    }

    file_list_layout.addWidget(&file_list_widget);
    file_list_dialog.exec();
}
