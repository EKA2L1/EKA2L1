#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QProgressDialog>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class main_window; }
QT_END_NAMESPACE

static constexpr std::size_t MAX_RECENT_ENTRIES = 5;

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

    QMenu *recent_mount_folder_menu_;
    QAction *recent_mount_folder_actions_[MAX_RECENT_ENTRIES];
    QAction *recent_mount_clear_;

    void setup_screen_draw();
    void setup_app_list();
    void setup_package_installer_ui_hooks();
    void refresh_recent_mounts();
    void mount_game_card_dump(QString path);
    void spawn_package_install_camper(QString package_install_path);

private slots:
    void on_about_triggered();
    void on_package_install_clicked();
    void on_device_install_clicked();
    void on_package_install_progress_change(const std::size_t now, const std::size_t total);
    bool on_package_install_text_ask(const char *text, const bool one_button);
    void on_new_device_added();
    void on_mount_card_clicked();
    void on_recent_mount_clear_clicked();
    void on_recent_mount_card_folder_clicked();

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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    display_widget *render_window() {
        return displayer_;
    }

private:
    Ui::main_window *ui_;
    applist_widget *applist_;
    display_widget *displayer_;
};
#endif // MAINWINDOW_H
