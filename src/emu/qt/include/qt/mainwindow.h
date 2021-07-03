#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QProgressDialog>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class main_window; }
QT_END_NAMESPACE

namespace eka2l1 {
    class system;
    class window_server;

    namespace desktop {
        struct emulator;
    }
}

class applist_widget;
class display_widget;

class main_window : public QMainWindow
{
    Q_OBJECT

private:
    eka2l1::desktop::emulator &emulator_state_;

    int active_screen_number_;
    std::size_t active_screen_draw_callback_;
    std::size_t active_screen_mode_change_callback_;

    QProgressDialog *current_package_install_dialog_;

    void setup_screen_draw();

private slots:
    void on_about_triggered();
    void on_package_install_clicked();
    void on_package_install_progress_change(const std::size_t now, const std::size_t total);
    bool on_package_install_text_ask(const char *text, const bool one_button);
    int on_package_install_language_choose(const int *languages, const int language_count);

    void on_app_clicked(QListWidgetItem *item);

signals:
    void package_install_progress_change(const std::size_t now, const std::size_t total);
    bool package_install_text_ask(const char *text, const bool one_button);
    int package_install_language_choose(const int *languages, const int language_count);

public:
    main_window(QWidget *parent, eka2l1::desktop::emulator &emulator_state);
    ~main_window();

    void resizeEvent(QResizeEvent *event) override;

    display_widget *render_window() {
        return displayer_;
    }

private:
    Ui::main_window *ui_;
    applist_widget *applist_;
    display_widget *displayer_;
};
#endif // MAINWINDOW_H
