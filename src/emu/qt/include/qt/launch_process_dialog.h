#pragma once

#include <QDialog>

namespace Ui {
    class launch_process_dialog;
}

namespace eka2l1 {
    class kernel_system;
}

class launch_process_dialog : public QDialog {
    Q_OBJECT

private:
    Ui::launch_process_dialog *ui_;
    eka2l1::kernel_system *kern_;

private slots:
    void on_launch_button_clicked();
    void on_cancel_button_clicked();

signals:
    void launched();

public:
    explicit launch_process_dialog(QWidget *parent, eka2l1::kernel_system *kern);
    ~launch_process_dialog();
};
