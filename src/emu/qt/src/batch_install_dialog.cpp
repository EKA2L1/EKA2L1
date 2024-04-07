#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

#include <qt/batch_install_dialog.h>

#include "./ui_batch_install_dialog.h"

batch_install_dialog::batch_install_dialog(QWidget *parent, QList<QString> initial_files)
    : QDialog(parent)
    , ui_(new Ui::batch_install_dialog) {
    ui_->setupUi(this);

    ignore_msgs_check_box_ = ui_->install_option_ignore_msgs;
    auto_lang_check_box_ = ui_->install_option_auto_choose_lang;
    list_widget_ = ui_->install_file_list;

    files_ = std::move(initial_files);

    update_install_list();

    connect(ui_->add_more_files_btn, &QPushButton::clicked, this, &batch_install_dialog::on_add_files_button_clicked);
    connect(ui_->clear_files_btn, &QPushButton::clicked, this, &batch_install_dialog::on_clear_files_button_clicked);
    connect(ui_->start_install_btn, &QPushButton::clicked, this, &batch_install_dialog::on_start_install_clicked);
}

batch_install_dialog::~batch_install_dialog() {
    delete ui_;
}

void batch_install_dialog::update_install_list() {
    list_widget_->clear();

    for (const auto &file : files_) {
        auto *item = new QListWidgetItem(file);
        list_widget_->addItem(item);
    }
}

void batch_install_dialog::on_add_files_button_clicked() {
    auto files = QFileDialog::getOpenFileNames(this, tr("Select files to install"), "", tr("SIS file (*.sis *.sisx)"));

    for (const auto &file : files) {
        if (files_.contains(file)) {
            continue;
        }

        files_.push_back(file);
    }

    update_install_list();
}

void batch_install_dialog::on_clear_files_button_clicked() {
    files_.clear();
    update_install_list();
}

void batch_install_dialog::on_start_install_clicked() {
    emit install_multiple(ignore_msgs_check_box_->isChecked(), auto_lang_check_box_->isChecked(), files_);
    close();
}

bool batch_install_dialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == list_widget_) {
        if (event->type() == QEvent::DragEnter) {
            auto drag_event = reinterpret_cast<QDragEnterEvent *>(event);

            if (drag_event->mimeData()->hasUrls()) {
                QList<QUrl> list_url = drag_event->mimeData()->urls();
                QString local_path = list_url[0].toLocalFile();
                if (local_path.endsWith(".sis") || local_path.endsWith(".sisx")) {
                    drag_event->acceptProposedAction();
                    return true;
                }
            }
        } else if (event->type() == QEvent::Drop) {
            auto drop_event = reinterpret_cast<QDropEvent *>(event);
            bool updated = false;

            for (const QUrl &url : drop_event->mimeData()->urls()) {
                auto local_file_path = url.toLocalFile();

                if (local_file_path.endsWith(".sis") || local_file_path.endsWith(".sisx")) {
                    if (files_.contains(local_file_path)) {
                        continue;
                    }

                    files_.push_back(local_file_path);
                    updated = true;
                }
            }

            if (updated) {
                update_install_list();
            }

            return true;
        }
    }

    return false;
}