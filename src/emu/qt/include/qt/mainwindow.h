#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class main_window; }
QT_END_NAMESPACE

namespace eka2l1 {
    class system;

    namespace desktop {
        struct emulator;
    }
}

class applist_widget;

class main_window : public QMainWindow
{
    Q_OBJECT

private:
    eka2l1::desktop::emulator &emulator_state_;

private slots:
    void on_about_triggered();

public:
    main_window(QWidget *parent, eka2l1::desktop::emulator &emulator_state);
    ~main_window();

private:
    Ui::main_window *ui_;
    applist_widget *applist_;
};
#endif // MAINWINDOW_H
