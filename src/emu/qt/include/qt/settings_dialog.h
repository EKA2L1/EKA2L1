#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <drivers/input/common.h>

#include <QPushButton>
#include <QDialog>
#include <QMap>

namespace Ui {
class settings_dialog;
}

namespace eka2l1 {
class system;

namespace config {
struct keybind;
struct keybind_profile;
struct state;

struct app_setting;
class app_settings;
}

namespace epoc {
struct window_group;
}

namespace drivers {
class emu_controller;
}
}

static constexpr const char *STATUS_BAR_HIDDEN_SETTING_NAME = "statusBarHidden";

void make_default_keybind_profile(eka2l1::config::keybind_profile &profile);

class settings_dialog : public QDialog
{
    Q_OBJECT

private:
    eka2l1::config::state &configuration_;
    eka2l1::system *system_;
    eka2l1::drivers::emu_controller *controller_;
    eka2l1::config::app_settings *app_settings_;

    QPushButton *target_bind_;
    QString target_original_text_;
    QMap<QPushButton*, int> target_bind_codes_;

    QString key_bind_entry_to_string(eka2l1::config::keybind &bind);

    void update_device_settings();
    void set_current_language(const int lang);
    void refresh_available_system_languages(const int index = -1);
    void refresh_device_utils_locking();
    void refresh_keybind_buttons();
    void refresh_keybind_profiles();
    void refresh_keybind_button(QPushButton *button);
    void active_keybind_profile_change();
    void refresh_configuration_for_who(const bool clear);
    void on_check_imei_validity_clicked();
    QString get_imei_error_string(const int err);

private slots:
    void on_nearest_neighbor_toggled(bool val);
    void on_cpu_read_toggled(bool val);
    void on_cpu_write_toggled(bool val);
    void on_cpu_step_toggled(bool val);
    void on_ipc_log_toggled(bool val);
    void on_syscall_log_toggled(bool val);
    void on_btrace_enable_toggled(bool val);
    void on_cursor_visibility_change(bool val);
    void on_status_bar_visibility_change(bool val);
    void on_data_path_browse_clicked();
    void on_tab_changed(int index);

    void on_control_profile_add_clicked();
    void on_control_profile_rename_clicked();
    void on_control_profile_delete_clicked();
    void on_control_profile_choosen_another(const QString &text);
    void on_binding_button_clicked();

    void on_device_combo_choose(const int index);
    void on_device_rename_requested();
    void on_device_rescan_requested();
    void on_device_validate_requested();
    void on_rta_combo_choose(const int index);
    void on_system_language_choose(const int index);
    void on_system_battery_slider_value_moved(int value);
    void on_master_volume_value_changed(int value);

    void on_fps_slider_value_changed(int value);
    void on_time_delay_value_changed(int value);
    void on_inherit_settings_toggled(bool checked);

public slots:
    void on_app_launching();
    void on_controller_button_press(eka2l1::drivers::input_event event);
    void refresh_app_configuration_details();
    void on_restart_requested_from_main();

signals:
    void cursor_visibility_change(bool visible);
    void status_bar_visibility_change(bool visible);
    void relaunch();
    void restart(const int index);
    void active_app_setting_changed();

public:
    explicit settings_dialog(QWidget *parent, eka2l1::system *sys, eka2l1::drivers::emu_controller *controller,
                             eka2l1::config::app_settings *app_settings, eka2l1::config::state &configuration);
    ~settings_dialog();

    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::settings_dialog *ui_;
};

#endif // SETTINGS_DIALOG_H
