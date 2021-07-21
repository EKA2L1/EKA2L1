#include <qt/settings_dialog.h>
#include <qt/utils.h>
#include "ui_settings_dialog.h"

#include <common/algorithm.h>
#include <config/config.h>
#include <config/app_settings.h>
#include <common/fileutils.h>
#include <common/language.h>
#include <common/path.h>
#include <common/crypt.h>
#include <cpu/arm_utils.h>
#include <dispatch/dispatcher.h>
#include <drivers/graphics/emu_window.h>
#include <drivers/input/emu_controller.h>
#include <services/window/window.h>
#include <services/window/screen.h>
#include <services/window/classes/wingroup.h>
#include <services/ui/cap/oom_app.h>
#include <services/hwrm/def.h>
#include <services/hwrm/power/power_def.h>

#include <system/devices.h>
#include <system/epoc.h>

#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <utils/locale.h>
#include <utils/system.h>

#include <QtConcurrent/QtConcurrent>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>

static constexpr qsizetype RTA_LOW_INDEX = 0;
static constexpr qsizetype RTA_MID_INDEX = 1;
static constexpr qsizetype RTA_HIGH_INDEX = 2;

static constexpr qsizetype DYNARMIC_COMBO_INDEX = 0;
static constexpr qsizetype DYNCOM_COMBO_INDEX = 1;

static void set_or_add_binding(eka2l1::config::keybind_profile &profile, eka2l1::config::keybind &new_bind) {
    bool exist_in_config = false;
    for (auto &kb : profile.keybinds) {
        if (kb.target == new_bind.target) {
            kb = new_bind;
            exist_in_config = true;
            break;
        }
    }

    if (!exist_in_config) {
        profile.keybinds.emplace_back(new_bind);
    }
}

void make_default_keybind_profile(eka2l1::config::keybind_profile &profile) {
    profile.keybinds.clear();

    eka2l1::config::keybind bind;
    bind.source.type = eka2l1::config::KEYBIND_TYPE_KEY;

    // Left softkey
    bind.target = eka2l1::epoc::std_key_device_0;
    bind.source.data.keycode = Qt::Key_F1;

    profile.keybinds.push_back(bind);

    // Right softkey
    bind.target = eka2l1::epoc::std_key_device_1;
    bind.source.data.keycode = Qt::Key_F2;

    profile.keybinds.push_back(bind);

    // Green softkey
    bind.target = eka2l1::epoc::std_key_application_0;
    bind.source.data.keycode = Qt::Key_F3;

    profile.keybinds.push_back(bind);

    // Red softkey
    bind.target = eka2l1::epoc::std_key_application_1;
    bind.source.data.keycode = Qt::Key_F4;

    profile.keybinds.push_back(bind);

    // Middle softkey
    bind.target = eka2l1::epoc::std_key_device_3;
    bind.source.data.keycode = Qt::Key_Return;

    profile.keybinds.push_back(bind);

    // Up arrow
    bind.target = eka2l1::epoc::std_key_up_arrow;
    bind.source.data.keycode = Qt::Key_Up;

    profile.keybinds.push_back(bind);

    // Down arrow
    bind.target = eka2l1::epoc::std_key_down_arrow;
    bind.source.data.keycode = Qt::Key_Down;

    profile.keybinds.push_back(bind);

    // Left arrow
    bind.target = eka2l1::epoc::std_key_left_arrow;
    bind.source.data.keycode = Qt::Key_Left;

    profile.keybinds.push_back(bind);

    // Right arrow
    bind.target = eka2l1::epoc::std_key_right_arrow;
    bind.source.data.keycode = Qt::Key_Right;

    profile.keybinds.push_back(bind);

    // 0
    bind.target = '0';
    bind.source.data.keycode = Qt::Key_0;

    profile.keybinds.push_back(bind);

    // 1
    bind.target = '1';
    bind.source.data.keycode = Qt::Key_1;

    profile.keybinds.push_back(bind);

    // 2
    bind.target = '2';
    bind.source.data.keycode = Qt::Key_2;

    profile.keybinds.push_back(bind);

    // 3
    bind.target = '3';
    bind.source.data.keycode = Qt::Key_3;

    profile.keybinds.push_back(bind);

    // 4
    bind.target = '4';
    bind.source.data.keycode = Qt::Key_4;

    profile.keybinds.push_back(bind);

    // 5
    bind.target = '5';
    bind.source.data.keycode = Qt::Key_5;

    profile.keybinds.push_back(bind);

    // 6
    bind.target = '6';
    bind.source.data.keycode = Qt::Key_6;

    profile.keybinds.push_back(bind);

    // 7
    bind.target = '7';
    bind.source.data.keycode = Qt::Key_7;

    profile.keybinds.push_back(bind);

    // 8
    bind.target = '8';
    bind.source.data.keycode = Qt::Key_8;

    profile.keybinds.push_back(bind);

    // 9
    bind.target = '9';
    bind.source.data.keycode = Qt::Key_9;

    profile.keybinds.push_back(bind);

    // *
    bind.target = '*';
    bind.source.data.keycode = Qt::Key_Asterisk;

    profile.keybinds.push_back(bind);

    // #
    bind.target = eka2l1::epoc::std_key_hash;
    bind.source.data.keycode = Qt::Key_Slash;

    profile.keybinds.push_back(bind);

    // Backspace
    bind.target = eka2l1::epoc::std_key_backspace;
    bind.source.data.keycode = Qt::Key_Backspace;

    profile.keybinds.push_back(bind);
}

settings_dialog::settings_dialog(QWidget *parent, eka2l1::system *sys, eka2l1::drivers::emu_controller *controller, eka2l1::config::app_settings *app_settings, eka2l1::config::state &configuration) :
    QDialog(parent),
    configuration_(configuration),
    system_(sys),
    controller_(controller),
    app_settings_(app_settings),
    target_bind_(nullptr),
    ui_(new Ui::settings_dialog)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui_->setupUi(this);

    std::string current_dir;
    eka2l1::get_current_directory(current_dir);

    const std::string storage_abs = eka2l1::absolute_path(configuration_.storage, current_dir);

    // Setup existing attributes in the configuration
    ui_->data_storage_path_edit->setText(QString::fromUtf8(storage_abs.c_str()));
    ui_->debugging_cpu_read_checkbox->setChecked(configuration_.log_read);
    ui_->debugging_cpu_write_checkbox->setChecked(configuration_.log_write);
    ui_->debugging_cpu_step_checkbox->setChecked(configuration_.stepping);
    ui_->debugging_ipc_checkbox->setChecked(configuration_.log_ipc);
    ui_->debugging_system_call_checkbox->setChecked(configuration_.log_svc);
    ui_->debugging_enable_btrace_checkbox->setChecked(configuration_.enable_btrace);

    ui_->emulator_display_hide_cursor_checkbox->setChecked(configuration_.hide_mouse_in_screen_space);
    ui_->emulator_display_nearest_neightbor_checkbox->setChecked(configuration_.nearest_neighbor_filtering);

    if (configuration_.rtos_level == "low") {
        ui_->system_he_rta_combo->setCurrentIndex(RTA_LOW_INDEX);
    } else if (configuration_.rtos_level == "mid") {
        ui_->system_he_rta_combo->setCurrentIndex(RTA_MID_INDEX);
    } else if (configuration_.rtos_level == "high") {
        ui_->system_he_rta_combo->setCurrentIndex(RTA_HIGH_INDEX);
    }

    ui_->system_prop_imei_edit->setText(QString::fromUtf8(configuration_.imei.c_str()));
    ui_->system_audio_vol_slider->setValue(configuration_.audio_master_volume);
    ui_->system_audio_current_val_label->setText(QString("%1").arg(configuration_.audio_master_volume));

    QSettings settings;
    ui_->interface_status_bar_checkbox->setChecked(settings.value(STATUS_BAR_HIDDEN_SETTING_NAME, false).toBool());
    ui_->interface_theme_combo->setCurrentIndex(settings.value(THEME_SETTING_NAME, 0).toInt());

    const arm_emulator_type type = system_->get_cpu_executor_type();
    switch (type) {
    case arm_emulator_type::dynarmic:
        ui_->system_he_cpu_combo->setCurrentIndex(DYNARMIC_COMBO_INDEX);
        break;

    case arm_emulator_type::dyncom:
        ui_->system_he_cpu_combo->setCurrentIndex(DYNCOM_COMBO_INDEX);
        break;

    default:
        break;
    }

    update_device_settings();
    refresh_available_system_languages();
    refresh_device_utils_locking();

    target_bind_codes_ = {
        { ui_->control_bind_up_arrow_btn, eka2l1::epoc::std_key_up_arrow },
        { ui_->control_bind_down_arrow_btn, eka2l1::epoc::std_key_down_arrow },
        { ui_->control_bind_left_arrow_btn, eka2l1::epoc::std_key_left_arrow },
        { ui_->control_bind_right_arrow_btn, eka2l1::epoc::std_key_right_arrow },
        { ui_->control_bind_mid_softkey_btn, eka2l1::epoc::std_key_device_3 },
        { ui_->control_bind_left_softkey_btn, eka2l1::epoc::std_key_device_0 },
        { ui_->control_bind_right_softkey_btn, eka2l1::epoc::std_key_device_1 },
        { ui_->control_bind_green_softkey_btn, eka2l1::epoc::std_key_application_0 },
        { ui_->control_bind_red_softkey_btn, eka2l1::epoc::std_key_application_1 },
        { ui_->control_bind_zero_btn, '0' },
        { ui_->control_bind_one_btn, '1' },
        { ui_->control_bind_two_btn, '2' },
        { ui_->control_bind_three_btn, '3' },
        { ui_->control_bind_four_btn, '4' },
        { ui_->control_bind_five_btn, '5' },
        { ui_->control_bind_six_btn, '6' },
        { ui_->control_bind_seven_btn, '7' },
        { ui_->control_bind_eight_btn, '8' },
        { ui_->control_bind_nine_btn, '9' },
        { ui_->control_bind_star_btn, '*' },
        { ui_->control_bind_tag_btn, eka2l1::epoc::std_key_hash },
    };

    QList<QPushButton*> need_buttons = target_bind_codes_.keys();

    for (QPushButton *&bind_widget: need_buttons) {
        connect(bind_widget, &QPushButton::clicked, this, &settings_dialog::on_binding_button_clicked);
        bind_widget->installEventFilter(this);
    }

    refresh_keybind_profiles();
    refresh_keybind_buttons();
    refresh_configuration_for_who(false);

    connect(ui_->emulator_display_nearest_neightbor_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_nearest_neighbor_toggled);
    connect(ui_->emulator_display_hide_cursor_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_cursor_visibility_change);

    connect(ui_->general_clear_ui_config_btn, &QPushButton::clicked, this, &settings_dialog::on_ui_clear_all_configs_clicked);
    connect(ui_->app_config_fps_slider, &QSlider::valueChanged, this, &settings_dialog::on_fps_slider_value_changed);
    connect(ui_->app_config_time_delay_slider, &QSlider::valueChanged, this, &settings_dialog::on_time_delay_value_changed);
    connect(ui_->app_config_inherit_settings_child_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_inherit_settings_toggled);

    connect(ui_->interface_status_bar_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_status_bar_visibility_change);
    connect(ui_->interface_theme_combo, &QComboBox::activated, this, &settings_dialog::on_theme_changed);

    connect(ui_->debugging_cpu_read_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_cpu_read_toggled);
    connect(ui_->debugging_cpu_write_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_cpu_write_toggled);
    connect(ui_->debugging_cpu_step_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_cpu_step_toggled);
    connect(ui_->debugging_ipc_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_ipc_log_toggled);
    connect(ui_->debugging_system_call_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_syscall_log_toggled);
    connect(ui_->debugging_enable_btrace_checkbox, &QCheckBox::toggled, this, &settings_dialog::on_btrace_enable_toggled);
    connect(ui_->data_storage_browse_btn, &QPushButton::clicked, this, &settings_dialog::on_data_path_browse_clicked);

    connect(ui_->system_device_combo, &QComboBox::activated, this, &settings_dialog::on_device_combo_choose);
    connect(ui_->system_device_current_rename_btn, &QPushButton::clicked, this, &settings_dialog::on_device_rename_requested);
    connect(ui_->system_he_rta_combo, &QComboBox::activated, this, &settings_dialog::on_rta_combo_choose);
    connect(ui_->system_he_cpu_combo, &QComboBox::activated, this, &settings_dialog::on_cpu_backend_changed);
    connect(ui_->system_prop_lang_combobox, &QComboBox::activated, this, &settings_dialog::on_system_language_choose);
    connect(ui_->system_general_validate_dvc_btn, &QPushButton::clicked, this, &settings_dialog::on_device_validate_requested);
    connect(ui_->system_general_rescan_dvcs_btn, &QPushButton::clicked, this, &settings_dialog::on_device_rescan_requested);
    connect(ui_->system_prop_bat_slider, &QSlider::valueChanged, this, &settings_dialog::on_system_battery_slider_value_moved);
    connect(ui_->system_prop_imei_check_btn, &QPushButton::clicked, this, &settings_dialog::on_check_imei_validity_clicked);
    connect(ui_->system_audio_vol_slider, &QSlider::valueChanged, this, &settings_dialog::on_master_volume_value_changed);

    connect(ui_->settings_tab, &QTabWidget::currentChanged, this, &settings_dialog::on_tab_changed);
    connect(ui_->control_profile_add_btn, &QPushButton::clicked, this, &settings_dialog::on_control_profile_add_clicked);
    connect(ui_->control_profile_rename_btn, &QPushButton::clicked, this, &settings_dialog::on_control_profile_rename_clicked);
    connect(ui_->control_profile_delete_btn, &QPushButton::clicked, this, &settings_dialog::on_control_profile_delete_clicked);
    connect(ui_->control_profile_combobox, &QComboBox::textActivated, this, &settings_dialog::on_control_profile_choosen_another);
}

settings_dialog::~settings_dialog()
{
    configuration_.serialize();
    delete ui_;
}

void settings_dialog::on_nearest_neighbor_toggled(bool val) {
    configuration_.nearest_neighbor_filtering = val;
}

void settings_dialog::on_cursor_visibility_change(bool toggled) {
    configuration_.hide_mouse_in_screen_space = toggled;
    emit cursor_visibility_change(!toggled);
}

void settings_dialog::on_status_bar_visibility_change(bool toggled) {
    QSettings settings;
    settings.setValue(STATUS_BAR_HIDDEN_SETTING_NAME, toggled);

    emit status_bar_visibility_change(!toggled);
}

void settings_dialog::on_data_path_browse_clicked() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Choose the data folder"), QString::fromStdString(configuration_.storage));
    if (!path.isEmpty()) {
        if (QMessageBox::question(this, tr("Relaunch needed"), tr("This change requires relaunching the emulator.<br>Do you want to continue?")) == QMessageBox::Yes) {
            ui_->data_storage_path_edit->setText(path);
            configuration_.storage = path.toStdString();
            configuration_.serialize();

            emit relaunch();
        }
    }
}

void settings_dialog::on_cpu_read_toggled(bool val) {
    configuration_.log_read = val;
}

void settings_dialog::on_cpu_write_toggled(bool val) {
    configuration_.log_write = val;
}

void settings_dialog::on_cpu_step_toggled(bool val) {
    configuration_.stepping = val;
}

void settings_dialog::on_ipc_log_toggled(bool val) {
    configuration_.log_ipc = val;
}

void settings_dialog::on_syscall_log_toggled(bool val) {
    configuration_.log_svc = val;
}

void settings_dialog::on_btrace_enable_toggled(bool val) {
    configuration_.enable_btrace = val;
}

static QString epocver_to_symbian_readable_name(const epocver ver) {
    switch (ver) {
    case epocver::epocu6:
        return QString("epocu6");

    case epocver::epoc6:
        return QString("S60v1");

    case epocver::epoc80:
        return QString("S60v2 - 8.0");

    case epocver::epoc81a:
        return QString("S60v2 - 8.1a");

    case epocver::epoc81b:
        return QString("S60v2 - 8.1b");

    case epocver::epoc93:
        return QString("S60v3");

    case epocver::epoc94:
        return QString("S60v5");

    case epocver::epoc95:
        return QString("S^3");

    case epocver::epoc10:
        return QString("Belle");

    default:
        break;
    }

    return QString("Unknown");
}

void settings_dialog::update_device_settings() {
    eka2l1::device_manager *device_mngr = system_->get_device_manager();
    if (device_mngr) {
        ui_->system_device_combo->clear();
        const std::lock_guard<std::mutex> guard(device_mngr->lock);

        for (std::size_t i = 0; i < device_mngr->total(); i++) {
            eka2l1::device *dvc = device_mngr->get(static_cast<std::uint8_t>(i));
            if (dvc) {
                QString item_des = QString("%1 (%2 - %3)").arg(QString::fromUtf8(dvc->model.c_str()), QString::fromUtf8(dvc->firmware_code.c_str()),
                                                               epocver_to_symbian_readable_name(dvc->ver));
                ui_->system_device_combo->addItem(item_des);
            }
        }

        ui_->system_device_combo->setCurrentIndex(device_mngr->get_current_index());
    }
}

void settings_dialog::on_device_combo_choose(const int index) {
    configuration_.device = index;
    configuration_.serialize();

    refresh_available_system_languages(index);
    refresh_configuration_for_who(true);

    ui_->system_general_validate_dvc_btn->setDisabled(false);
    ui_->system_general_rescan_dvcs_btn->setDisabled(false);

    emit restart(index);
}

void settings_dialog::on_app_launching() {
    ui_->system_general_validate_dvc_btn->setDisabled(true);
    ui_->system_general_rescan_dvcs_btn->setDisabled(true);
}

void settings_dialog::on_device_rescan_requested() {
    system_->rescan_devices(drive_z);
    update_device_settings();
}

void settings_dialog::on_device_validate_requested() {
    QFuture<void> future = QtConcurrent::run([this]() {
        system_->validate_current_device();
    });

    QMessageBox block_box(this);
    block_box.setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
    block_box.setWindowTitle(tr("Please wait"));
    block_box.setText(tr("Validation is in process. Please do not exit or launch applications until this operation is done."));
    block_box.setStandardButtons(QMessageBox::NoButton);
    block_box.show();

    bool last_validate_state =  ui_->system_general_validate_dvc_btn->isEnabled();
    bool last_rescan_state = ui_->system_general_rescan_dvcs_btn->isEnabled();

    ui_->system_general_validate_dvc_btn->setDisabled(true);
    ui_->system_general_rescan_dvcs_btn->setDisabled(true);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ui_->system_general_validate_dvc_btn->setDisabled(!last_validate_state);
    ui_->system_general_rescan_dvcs_btn->setDisabled(!last_rescan_state);

    block_box.close();

    QMessageBox::information(this, tr("Validation done!"), tr("The validation has successfully completed!"));
}

void settings_dialog::on_device_rename_requested() {
    eka2l1::device_manager *device_mngr = system_->get_device_manager();
    if (device_mngr) {
        eka2l1::device *current_dvc = device_mngr->get(ui_->system_device_combo->currentIndex());
        if (current_dvc) {
            bool is_ok = false;
            QString text_inside = QString::fromUtf8(current_dvc->model);

            text_inside = QInputDialog::getText(this, tr("Enter new device name"), QString(), QLineEdit::Normal, text_inside, &is_ok);
            if (is_ok) {
                current_dvc->model = text_inside.toStdString();
                device_mngr->save_devices();

                update_device_settings();
            }
        }
    }
}


void settings_dialog::on_rta_combo_choose(const int index) {
    eka2l1::ntimer *timing = system_->get_ntimer();
    if (timing) {
        configuration_.rtos_level = eka2l1::get_string_of_realtime_level(static_cast<eka2l1::realtime_level>(index));
        configuration_.serialize();

        timing->set_realtime_level(static_cast<eka2l1::realtime_level>(index));
    }
}

void settings_dialog::set_current_language(const int lang) {
    configuration_.language = lang;
    system_->set_system_language(static_cast<language>(lang));

    eka2l1::kernel_system *kern = system_->get_kernel_system();

    eka2l1::property_ptr lang_prop = kern->get_prop(eka2l1::epoc::SYS_CATEGORY, eka2l1::epoc::LOCALE_LANG_KEY);
    auto current_lang = lang_prop->get_pkg<eka2l1::epoc::locale_language>();

    if (!current_lang) {
        return;
    }

    current_lang->language = static_cast<eka2l1::epoc::language>(lang);
    lang_prop->set<eka2l1::epoc::locale_language>(current_lang.value());
}

void settings_dialog::refresh_available_system_languages(const int index) {
    ui_->system_prop_lang_combobox->clear();

    eka2l1::device_manager *device_mngr = system_->get_device_manager();
    if (device_mngr) {
        int current_index = -1;
        bool need_set = false;

        {
            const std::lock_guard<std::mutex> guard(device_mngr->lock);
            eka2l1::device *targetto = (index < 0) ? device_mngr->get_current() : device_mngr->get(static_cast<std::uint8_t>(index));
            if (targetto) {
                for (std::size_t i = 0; i < targetto->languages.size(); i++) {
                    if (targetto->languages[i] == configuration_.language) {
                        current_index = static_cast<int>(i);
                    }

                    std::string lang_name = eka2l1::common::get_language_name_by_code(targetto->languages[i]);
                    ui_->system_prop_lang_combobox->addItem(QString::fromUtf8(lang_name.c_str()));
                }

                need_set = true;
            }
        }

        if (need_set)
            ui_->system_prop_lang_combobox->setCurrentIndex((current_index < 0) ? 0 : current_index);
    }
}

void settings_dialog::refresh_device_utils_locking() {
    eka2l1::kernel_system *kern = system_->get_kernel_system();
    if (!kern || kern->get_thread_list().empty()) {
        ui_->system_general_rescan_dvcs_btn->setDisabled(false);
        ui_->system_general_validate_dvc_btn->setDisabled(false);
    } else {
        ui_->system_general_rescan_dvcs_btn->setDisabled(true);
        ui_->system_general_validate_dvc_btn->setDisabled(true);
    }
}

void settings_dialog::on_system_language_choose(const int index) {
    eka2l1::device_manager *device_mngr = system_->get_device_manager();
    if (device_mngr) {
        const std::lock_guard<std::mutex> guard(device_mngr->lock);
        eka2l1::device *targetto = (index < 0) ? device_mngr->get_current() : device_mngr->get(static_cast<std::uint8_t>(index));
        if (targetto) {
            if (index < targetto->languages.size()) {
                set_current_language(targetto->languages[index]);
            }
        }
    }
}

void settings_dialog::refresh_keybind_button(QPushButton *bind_widget) {
    const int target_bind_code = target_bind_codes_[bind_widget];

    ui_->touchscreen_disable_label->setVisible(false);
    bind_widget->setText(tr("Unbind"));

    for (auto &keybind: configuration_.keybinds.keybinds) {
        if (keybind.target == target_bind_code) {
            bind_widget->setText(key_bind_entry_to_string(keybind));
        }

        if (keybind.source.type == eka2l1::config::KEYBIND_TYPE_MOUSE) {
            ui_->touchscreen_disable_label->setVisible(true);
        }
    }
}

void settings_dialog::refresh_keybind_buttons() {
    QList<QPushButton*> need_buttons = target_bind_codes_.keys();

    for (QPushButton *&bind_widget: need_buttons) {
        refresh_keybind_button(bind_widget);
    }
}

void settings_dialog::refresh_keybind_profiles() {
    ui_->control_profile_combobox->clear();

    eka2l1::common::dir_iterator ite("bindings\\");
    eka2l1::common::dir_entry entry;

    while (ite.next_entry(entry) >= 0) {
        if (eka2l1::path_extension(entry.name) == ".yml") {
            const std::string final_name = eka2l1::replace_extension(entry.name, "");
            ui_->control_profile_combobox->addItem(QString::fromUtf8(final_name.c_str()));

            if (final_name == configuration_.current_keybind_profile) {
                ui_->control_profile_combobox->setCurrentIndex(ui_->control_profile_combobox->count() - 1);
            }
        }
    }
}

void settings_dialog::on_binding_button_clicked() {
    if (target_bind_) {
        refresh_keybind_button(target_bind_);
    }

    target_bind_ = qobject_cast<QPushButton*>(sender());
    target_bind_->setText(tr("Waiting for input"));
}

void settings_dialog::on_tab_changed(int index) {
    if (target_bind_) {
        refresh_keybind_button(target_bind_);
    }

    target_bind_ = nullptr;
}

QString settings_dialog::key_bind_entry_to_string(eka2l1::config::keybind &bind) {
    if (bind.source.type == eka2l1::config::KEYBIND_TYPE_KEY) {
        return QKeySequence(bind.source.data.keycode).toString();
    } else if (bind.source.type == eka2l1::config::KEYBIND_TYPE_MOUSE) {
        return tr("Mouse button %1").arg(bind.source.data.keycode - eka2l1::epoc::KEYBIND_TYPE_MOUSE_CODE_BASE);
    } else if (bind.source.type == eka2l1::config::KEYBIND_TYPE_CONTROLLER) {
        QString first_result = tr("Controller %1 : Button %2").arg(bind.source.data.button.controller_id);
        // Backend may not be able to initialized, so should do a check
        if (controller_) {
            return first_result.arg(controller_->button_to_string(bind.source.data.button.button_id));
        } else {
            return first_result.arg(bind.source.data.button.button_id);
        }
    }

    return "?";
}

bool settings_dialog::eventFilter(QObject *obj, QEvent *event) {
    if (target_bind_) {
        std::optional<eka2l1::config::keybind> new_bind;

        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key_event = reinterpret_cast<QKeyEvent*>(event);

            const int target_bind_code = target_bind_codes_[target_bind_];
            target_bind_->setText(QKeySequence(key_event->key()).toString());

            eka2l1::window_server *win_serv = get_window_server_through_system(system_);

            if (win_serv) {
                win_serv->delete_key_mapping(target_bind_code);
                win_serv->input_mapping.key_input_map[key_event->key()] = static_cast<eka2l1::epoc::std_scan_code>(target_bind_code);
            }

            new_bind = std::make_optional<eka2l1::config::keybind>();

            new_bind->target = target_bind_code;
            new_bind->source.type = eka2l1::config::KEYBIND_TYPE_KEY;
            new_bind->source.data.keycode = key_event->key();
        } else if (event->type() == QEvent::MouseButtonPress) {
            if (target_bind_ != obj) {
                // Pass, we need no care. Let the other handles it
                return false;
            }

            QMouseEvent *mouse_event = reinterpret_cast<QMouseEvent*>(event);
            const std::uint32_t mouse_button_order = eka2l1::common::find_most_significant_bit_one(mouse_event->button()) + eka2l1::epoc::KEYBIND_TYPE_MOUSE_CODE_BASE;
            const int target_bind_code = target_bind_codes_[target_bind_];

            target_bind_->setText(tr("Mouse button %1").arg(mouse_button_order - eka2l1::epoc::KEYBIND_TYPE_MOUSE_CODE_BASE));

            eka2l1::window_server *win_serv = get_window_server_through_system(system_);

            if (win_serv) {
                win_serv->delete_key_mapping(target_bind_code);
                win_serv->input_mapping.key_input_map[mouse_button_order] = static_cast<eka2l1::epoc::std_scan_code>(target_bind_code);
            }

            new_bind = std::make_optional<eka2l1::config::keybind>();

            new_bind->target = target_bind_code;
            new_bind->source.type = eka2l1::config::KEYBIND_TYPE_MOUSE;
            new_bind->source.data.keycode = mouse_button_order;
        }

        if (new_bind.has_value()) {
            set_or_add_binding(configuration_.keybinds, new_bind.value());
            configuration_.serialize();

            target_bind_ = nullptr;
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void settings_dialog::on_controller_button_press(eka2l1::drivers::input_event event) {
    if (!target_bind_) {
        return;
    }

    if (event.button_.state_ == eka2l1::drivers::button_state::pressed) {
        eka2l1::config::keybind new_bind;

        const int target_bind_code = target_bind_codes_[target_bind_];
        new_bind.target = target_bind_code;
        new_bind.source.type = eka2l1::config::KEYBIND_TYPE_CONTROLLER;
        new_bind.source.data.button.controller_id = event.button_.controller_;
        new_bind.source.data.button.button_id = event.button_.button_;

        target_bind_->setText(tr("Controller %1 : Button %2").arg(event.button_.controller_).arg(controller_->button_to_string(event.button_.button_)));

        eka2l1::window_server *win_serv = get_window_server_through_system(system_);

        if (win_serv) {
            win_serv->delete_key_mapping(target_bind_code);
            win_serv->input_mapping.button_input_map[std::make_pair(event.button_.controller_, event.button_.button_)] =
                    static_cast<eka2l1::epoc::std_scan_code>(target_bind_code);
        }

        set_or_add_binding(configuration_.keybinds, new_bind);
        configuration_.serialize();

        target_bind_ = nullptr;
    }
}

void settings_dialog::active_keybind_profile_change() {
    eka2l1::window_server *server = get_window_server_through_system(system_);
    if (server) {
        server->init_key_mappings();
    }
}

void settings_dialog::on_control_profile_add_clicked() {
    bool is_ok = false;
    QString result = QInputDialog::getText(this, tr("Enter profile name"), QString(), QLineEdit::Normal, QString(), &is_ok);

    if (!is_ok) {
        return;
    }

    std::string path_to_file = fmt::format("bindings\\{}.yml", result.toStdString());
    if (eka2l1::exists(path_to_file)) {
        QMessageBox::critical(this, tr("Profile creation failed"), tr("A profile with that name already exists!"));
    } else {
        configuration_.serialize();

        configuration_.current_keybind_profile = result.toStdString();
        make_default_keybind_profile(configuration_.keybinds);
        configuration_.serialize();

        refresh_keybind_profiles();
        refresh_keybind_buttons();
        active_keybind_profile_change();
    }
}

void settings_dialog::on_control_profile_rename_clicked() {
    bool is_ok = false;
    QString result = QInputDialog::getText(this, tr("Enter profile name"), QString(), QLineEdit::Normal, ui_->control_profile_combobox->currentText(), &is_ok);

    if (!is_ok) {
        return;
    }

    std::string path_to_file = fmt::format("bindings\\{}.yml", result.toStdString());
    if (eka2l1::exists(path_to_file)) {
        QMessageBox::critical(this, tr("Profile rename failed"), tr("A profile with that name already exists!"));
    } else {
        eka2l1::common::move_file(fmt::format("bindings\\{}.yml", configuration_.current_keybind_profile), path_to_file);

        configuration_.current_keybind_profile = result.toStdString();
        configuration_.serialize();

        refresh_keybind_profiles();
        refresh_keybind_buttons();
        active_keybind_profile_change();
    }
}

void settings_dialog::on_control_profile_delete_clicked() {
    if (ui_->control_profile_combobox->count() <= 1) {
        QMessageBox::critical(this, tr("Profile deletion failed"), tr("This is the only profile left!"));
    } else {
        std::string path_to_file = fmt::format("bindings\\{}.yml", configuration_.current_keybind_profile);
        eka2l1::common::remove(path_to_file);

        ui_->control_profile_combobox->removeItem(ui_->control_profile_combobox->currentIndex());

        configuration_.current_keybind_profile = ui_->control_profile_combobox->currentText().toStdString();
        configuration_.serialize(false);
        configuration_.deserialize(true);

        refresh_keybind_profiles();
        refresh_keybind_buttons();
        active_keybind_profile_change();
    }
}

void settings_dialog::on_control_profile_choosen_another(const QString &text) {
    configuration_.current_keybind_profile = text.toStdString();
    configuration_.serialize(false);
    configuration_.deserialize(true);

    refresh_keybind_buttons();
    active_keybind_profile_change();
}

void settings_dialog::refresh_configuration_for_who(const bool clear) {
    ui_->app_config_for_who_label->clear();
    ui_->app_config_all_widget->setVisible(false);

    eka2l1::kernel_system *kernel = system_->get_kernel_system();
    if (kernel)
        kernel->lock();

    QString format_string = tr("<b>Configuration for:</b> %1");

    if (clear) {
        ui_->app_config_for_who_label->setText(format_string.arg(tr("None")));
    } else if (kernel) {
        std::optional<eka2l1::akn_running_app_info> info = ::get_active_app_info(system_);

        if (!info.has_value()) {
            ui_->app_config_for_who_label->setText(format_string.arg(tr("None")));
        } else {
            ui_->app_config_for_who_label->setText(format_string.arg(info->app_name_));
            ui_->app_config_all_widget->setVisible(true);

            eka2l1::epoc::screen *scr = ::get_current_active_screen(system_);

            if (scr) {
                ui_->app_config_fps_slider->setValue(scr->refresh_rate);
                ui_->app_config_fps_current_val->setText(QString("%1").arg(scr->refresh_rate));
            }

            ui_->app_config_time_delay_slider->setValue(info->associated_->get_time_delay());
            ui_->app_config_inherit_settings_child_checkbox->setChecked(info->associated_->get_child_inherit_setting());

            ui_->app_config_time_delay_current_val->setText(QString("%1").arg(info->associated_->get_time_delay()));
        }
    }

    if (kernel)
        kernel->unlock();
}

void settings_dialog::on_restart_requested_from_main() {
    refresh_configuration_for_who(true);
}

void settings_dialog::refresh_app_configuration_details() {
    refresh_configuration_for_who(false);
}

void settings_dialog::on_fps_slider_value_changed(int value) {
    ui_->app_config_fps_current_val->setText(QString("%1").arg(value));

    eka2l1::epoc::screen *scr = get_current_active_screen(system_);
    if (!scr) {
        return;
    }

    scr->refresh_rate = static_cast<std::uint8_t>(value);
    emit active_app_setting_changed();
}

void settings_dialog::on_time_delay_value_changed(int value) {
    ui_->app_config_time_delay_current_val->setText(QString("%1").arg(value));

    std::optional<eka2l1::akn_running_app_info> info = ::get_active_app_info(system_);
    if (info.has_value()) {
        info->associated_->set_time_delay(static_cast<std::uint32_t>(value));
        emit active_app_setting_changed();
    }
}

void settings_dialog::on_inherit_settings_toggled(bool checked) {
    std::optional<eka2l1::akn_running_app_info> info = ::get_active_app_info(system_);
    if (info.has_value()) {
        info->associated_->set_child_inherit_setting(checked);
        emit active_app_setting_changed();
    }
}

void settings_dialog::on_system_battery_slider_value_moved(int value) {
    ui_->system_prop_bat_current_val_label->setText(QString("%1").arg(value));

    eka2l1::kernel_system *kernel = system_->get_kernel_system();
    if (kernel) {
        kernel->lock();
        eka2l1::property_ptr power_prop = kernel->get_prop(eka2l1::epoc::hwrm::power::STATE_UID, eka2l1::epoc::hwrm::power::BATTERY_LEVEL_KEY);
        if (power_prop) {
            power_prop->set_int(value * eka2l1::epoc::hwrm::power::BATTERY_LEVEL_MAX / 100);
        }
        kernel->unlock();
    }
}

void settings_dialog::on_master_volume_value_changed(int value) {
    ui_->system_audio_current_val_label->setText(QString("%1").arg(value));

    eka2l1::dispatch::dispatcher *dispatcher = system_->get_dispatcher();
    if (dispatcher) {
        eka2l1::dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        manager.master_volume(static_cast<std::uint32_t>(value));

        configuration_.audio_master_volume = value;
        configuration_.serialize();
    }
}

QString settings_dialog::get_imei_error_string(const int err) {
    switch (err) {
    case eka2l1::crypt::IMEI_ERROR_INVALID_CHARACTER:
        return tr("IMEI sequence contains non-numeric character!");

    case eka2l1::crypt::IMEI_ERROR_INVALID_SUM:
        return tr("IMEI sequence has invalid sum!");

    case eka2l1::crypt::IMEI_ERROR_NO_RIGHT_LENGTH:
        return tr("IMEI sequence length must be 15!");

    default:
        break;
    }

    return tr("Unidentified error!");
}

void settings_dialog::on_check_imei_validity_clicked() {
    const std::string value = ui_->system_prop_imei_edit->text().toStdString();
    const eka2l1::crypt::imei_valid_error validity = eka2l1::crypt::is_imei_valid(value);
    if (validity == eka2l1::crypt::IMEI_ERROR_NONE) {
        QMessageBox::information(this, tr("IMEI valid!"), tr("The IMEI sequence is valid!"));
    } else {
        const QString target = get_imei_error_string(validity);
        QMessageBox::critical(this, tr("IMEI invalid!"), target);
    }
}

void settings_dialog::closeEvent(QCloseEvent *event) {
    const std::string value = ui_->system_prop_imei_edit->text().toStdString();
    const eka2l1::crypt::imei_valid_error validity = eka2l1::crypt::is_imei_valid(value);

    if (validity != eka2l1::crypt::IMEI_ERROR_NONE) {
        const QString error_string = get_imei_error_string(validity);
        const QString reminder = tr("Your IMEI is invalid because: %1.<br>Do you want to edit the current IMEI instead of closing? Choosing \"No\" will save the current IMEI value.").arg(error_string);

        if (QMessageBox::question(this, tr("Cancel closing"), reminder) == QMessageBox::Yes) {
            event->ignore();
        } else {
            configuration_.imei = value;
            event->accept();
        }
    } else {
        configuration_.imei = value;
        event->accept();
    }
}

void settings_dialog::on_theme_changed(int value) {
    if (value <= 1) {
        QSettings settings;
        settings.setValue(THEME_SETTING_NAME, value);

        emit theme_change_request(QString("%1").arg(value));
    } else {
        emit theme_change_request(ui_->interface_theme_combo->itemText(value));
    }
}

void settings_dialog::on_cpu_backend_changed(int value) {
    arm_emulator_type type = arm_emulator_type::dynarmic;
    switch (value) {
    case DYNARMIC_COMBO_INDEX:
        type = arm_emulator_type::dynarmic;
        break;

    case DYNCOM_COMBO_INDEX:
        type = arm_emulator_type::dyncom;
        break;

    default:
        break;
    }

    if (type != system_->get_cpu_executor_type()) {
        QMessageBox::information(this, tr("Relaunch needed"), tr("This change will be effective on the next launch of the emulator."));
    }

    configuration_.cpu_backend = eka2l1::arm::arm_emulator_type_to_string(type);
    configuration_.serialize();
}

void settings_dialog::on_ui_clear_all_configs_clicked() {
    const QMessageBox::StandardButton button = QMessageBox::question(this, tr("Confirmation"), tr("Are you sure about this? Your current theme will be reset, all message boxes that have been disabled will be re-enabled, and all recent mounts will be cleared."));
    if (button == QMessageBox::Yes) {
        QSettings settings;
        settings.clear();
    }
}
