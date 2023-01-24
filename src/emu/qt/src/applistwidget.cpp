/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <QBitmap>
#include <QLineEdit>
#include <QPainter>
#include <QMovie>
#include <QSettings>
#include <QMenu>
#include <QInputDialog>
#include <QtSvg/QSvgRenderer>
#include <QtConcurrent/QtConcurrent>

#include <common/buffer.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/pystr.h>
#include <config/config.h>
#include <qt/applistwidget.h>
#include <qt/utils.h>
#include <services/applist/applist.h>
#include <services/fbs/fbs.h>
#include <system/devices.h>
#include <utils/apacmd.h>
#include <j2me/applist.h>
#include <j2me/interface.h>

#include <vector>

#include <loader/mif.h>
#include <loader/svgb.h>
#include <loader/nvg.h>

static QSize ICON_GRID_SIZE = QSize(64, 64);
static QSize ICON_GRID_SPACING_SIZE = QSize(20, 20);
static constexpr std::uint32_t WARE_APP_UID_START = 0x10300000;
static const char *J2ME_MODE_ENABLED_SETTING = "J2MEModeEnabled";

static inline bool is_app_reg_system_app(const eka2l1::apa_app_registry *reg) {
    return (reg->land_drive == drive_z) && (reg->mandatory_info.uid < WARE_APP_UID_START);
}

applist_search_bar::applist_search_bar(QWidget *parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    search_label_ = new QLabel(tr("Search"));
    search_line_edit_ = new QLineEdit(this);

    search_layout_ = new QHBoxLayout(this);
    search_layout_->addWidget(search_label_);
    search_layout_->addWidget(search_line_edit_);
    search_layout_->setContentsMargins(0, 0, 0, 0);

    setLayout(search_layout_);

    connect(search_line_edit_, &QLineEdit::textChanged, this, &applist_search_bar::on_search_bar_content_changed);
}

applist_search_bar::~applist_search_bar() {
    delete search_layout_;
    delete search_line_edit_;
    delete search_label_;
}

const QString applist_search_bar::value() const {
    return search_line_edit_->text();
}

void applist_search_bar::on_search_bar_content_changed(QString content) {
    emit new_search(content);
}

applist_device_combo::applist_device_combo(QWidget *parent)
    : QWidget(parent)
    , current_index_(-1) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    device_label_ = new QLabel(tr("Device"));
    device_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    device_combo_ = new QComboBox;

    device_layout_ = new QHBoxLayout;
    device_layout_->addWidget(device_label_);
    device_layout_->addWidget(device_combo_);

    setLayout(device_layout_);

    connect(device_combo_, QOverload<int>::of(&QComboBox::activated), this, &applist_device_combo::on_device_combo_changed);
}

applist_device_combo::~applist_device_combo() {
    delete device_layout_;
    delete device_combo_;
    delete device_label_;
}

void applist_device_combo::update_devices(const QStringList &devices, const int index) {
    device_combo_->clear();
    for (const QString &device: devices) {
        device_combo_->addItem(device);
    }
    device_combo_->setCurrentIndex(index);
    current_index_ = index;
}

void applist_device_combo::on_device_combo_changed(int index) {
    if (current_index_ == index) {
        return;
    }

    current_index_ = index;
    emit device_combo_changed(index);
}

applist_widget_item::applist_widget_item(const QIcon &icon, const QString &name, int registry_index, bool is_j2me, QListWidget *parent)
    : QListWidgetItem(icon, name, parent)
    , registry_index_(registry_index)
    , is_j2me_(is_j2me) {
}

static void configure_list_widget_to_grid(QListWidget *list_widget) {
    list_widget->setFrameShape(QFrame::NoFrame);
    list_widget->viewport()->setAutoFillBackground(false);
    list_widget->setFlow(QListWidget::Flow::LeftToRight);
    list_widget->setResizeMode(QListWidget::ResizeMode::Adjust);
    list_widget->setSpacing(100);
    list_widget->setGridSize(ICON_GRID_SIZE + ICON_GRID_SPACING_SIZE);
    list_widget->setIconSize(ICON_GRID_SIZE);
    list_widget->setViewMode(QListWidget::ViewMode::IconMode);
    list_widget->setMovement(QListWidget::Movement::Static);
    list_widget->setMinimumHeight(ICON_GRID_SIZE.height() + ICON_GRID_SPACING_SIZE.height() + 200);
}

applist_widget::applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss, eka2l1::io_system *io, eka2l1::j2me::app_list *lister_j2me, eka2l1::config::state &conf, const bool hide_system_apps, const bool ngage_mode)
    : QWidget(parent)
    , search_bar_(nullptr)
    , device_combo_bar_(nullptr)
    , list_widget_(nullptr)
    , layout_(nullptr)
    , bar_widget_(nullptr)
    , loading_widget_(nullptr)
    , loading_gif_(nullptr)
    , loading_label_(nullptr)
    , j2me_mode_btn_(nullptr)
    , lister_(lister)
    , lister_j2me_(lister_j2me)
    , no_app_visible_normal_label_(nullptr)
    , no_app_visible_hide_sysapp_label_(nullptr)
    , fbss_(fbss)
    , io_(io)
    , hide_system_apps_(hide_system_apps)
    , conf_(conf)
    , should_dead_(false)
    , context_j2me_item_(nullptr) {
    search_bar_ = new applist_search_bar;
    device_combo_bar_ = new applist_device_combo;
    j2me_mode_btn_ = new QPushButton;

    loaded_[0] = loaded_[1] = false;

    QSettings settings;
    j2me_mode_btn_->setCheckable(true);
    j2me_mode_btn_->setChecked(settings.value(J2ME_MODE_ENABLED_SETTING, false).toBool());
    update_j2me_button_name();

    list_widget_ = new QListWidget;
    list_j2me_widget_ = new QListWidget;

    loading_label_ = new QLabel;
    loading_gif_ = new QMovie(":/assets/loading.gif");
    loading_label_->setMovie(loading_gif_);
    loading_gif_->setScaledSize(ICON_GRID_SIZE - QSize(2, 2));
    loading_label_->setFixedSize(ICON_GRID_SIZE);

    no_app_visible_normal_label_ = new QLabel("No app installed on the device!");
    no_app_visible_hide_sysapp_label_ = new QLabel("No non-system app installed on the device!");

    static const char *EMPTY_APP_LIST_TEXT_STYLESHEET = "QLabel { font-size: 24px; }";

    no_app_visible_normal_label_->setStyleSheet(EMPTY_APP_LIST_TEXT_STYLESHEET);
    no_app_visible_hide_sysapp_label_->setStyleSheet(EMPTY_APP_LIST_TEXT_STYLESHEET);

    layout_ = new QVBoxLayout(this);

    bar_widget_ = new QWidget;
    QHBoxLayout *temp_layout = new QHBoxLayout;

    temp_layout->addWidget(search_bar_, 10);
    temp_layout->addWidget(device_combo_bar_, 4);
    temp_layout->addWidget(j2me_mode_btn_, 1);
    temp_layout->setContentsMargins(0, 0, 0, 0);
    j2me_mode_btn_->setContentsMargins(0, 0, 10, 0);
    bar_widget_->setLayout(temp_layout);

    j2me_mode_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    bar_widget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    layout_->addWidget(bar_widget_);
    layout_->addWidget(list_widget_);
    layout_->addWidget(list_j2me_widget_);
    layout_->addWidget(no_app_visible_normal_label_);
    layout_->addWidget(no_app_visible_hide_sysapp_label_);

    loading_widget_ = new QWidget;
    QGridLayout *temp_containing_grid_layout = new QGridLayout;

    loading_widget_->setLayout(temp_containing_grid_layout);
    loading_label_->setAlignment(Qt::AlignCenter);

    temp_containing_grid_layout->setSizeConstraint(QLayout::SetMaximumSize);
    temp_containing_grid_layout->addWidget(loading_label_);
    temp_containing_grid_layout->setAlignment(Qt::AlignCenter);

    layout_->addWidget(loading_widget_);
    layout_->setContentsMargins(0, 0, 0, 0);

    no_app_visible_hide_sysapp_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    no_app_visible_hide_sysapp_label_->setAlignment(Qt::AlignCenter);

    no_app_visible_normal_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    no_app_visible_normal_label_->setAlignment(Qt::AlignCenter);

    no_app_visible_normal_label_->hide();
    no_app_visible_hide_sysapp_label_->hide();
    list_widget_->hide();
    loading_label_->show();
    loading_gif_->start();

    setLayout(layout_);

    // Well, just make it a part of the window. Hehe
    configure_list_widget_to_grid(list_widget_);
    configure_list_widget_to_grid(list_j2me_widget_);

    list_j2me_widget_->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(list_widget_, &QListWidget::itemClicked, this, &applist_widget::on_list_widget_item_clicked);
    connect(list_j2me_widget_, &QListWidget::itemClicked, this, &applist_widget::on_list_widget_item_clicked);
    connect(list_j2me_widget_, &QListWidget::customContextMenuRequested, this, &applist_widget::on_j2me_list_widget_custom_menu_requested);
    connect(search_bar_, &applist_search_bar::new_search, this, &applist_widget::on_search_content_changed);
    connect(device_combo_bar_, &applist_device_combo::device_combo_changed, this, &applist_widget::on_device_change_request);
    connect(j2me_mode_btn_, &QPushButton::clicked, this, &applist_widget::on_j2me_mode_btn_clicked);
    connect(this, &applist_widget::new_registeration_item_come, this, &applist_widget::on_new_registeration_item_come, Qt::QueuedConnection);
}

applist_widget::~applist_widget() {
    exit_mutex_.lock();

    scanning_done_evt_.reset();
    should_dead_ = true;

    exit_mutex_.unlock();

    if (scanning_) {
        scanning_done_evt_.wait();
    }

    delete search_bar_;
    delete device_combo_bar_;
    delete loading_gif_;
    delete loading_label_;
    delete loading_widget_;

    delete layout_;
    delete bar_widget_;
    delete list_widget_;
    delete list_j2me_widget_;
}

void applist_widget::hide_all() {
    for (qsizetype i = 0; i < list_widget_->count(); i++) {
        list_widget_->item(i)->setHidden(true);
    }
}

void applist_widget::show_all() {
    for (qsizetype i = 0; i < list_widget_->count(); i++) {
        list_widget_->item(i)->setHidden(false);
    }
}

void applist_widget::on_search_content_changed(QString content) {
    if (content.isEmpty()) {
        show_all();
        return;
    }

    hide_all();

    QList<QListWidgetItem *> matches(list_widget_->findItems(content, Qt::MatchFlag::MatchContains));

    for (QListWidgetItem *item : matches) {
        item->setHidden(false);
    }

    if (matches.size() >= 1) {
        list_widget_->scrollToItem(matches.front());
    }
}

void applist_widget::on_list_widget_item_clicked(QListWidgetItem *item) {
    applist_widget_item *item_translated = reinterpret_cast<applist_widget_item *>(item);
    emit app_launch(item_translated);
}

void applist_widget::on_new_registeration_item_come(QListWidgetItem *item) {
    if (item->text().contains(search_bar_->value(), Qt::CaseInsensitive)) {
        item->setHidden(false);
    } else {
        item->setHidden(true);
    }

    const bool in_j2me = j2me_mode_btn_->isChecked();

    if (in_j2me) {
        list_j2me_widget_->addItem(item);
    } else {
        list_widget_->addItem(item);
    }

    if (!loading_label_->isHidden()) {
        loading_label_->hide();
        loading_gif_->stop();

        if (in_j2me) {
            list_j2me_widget_->show();
        } else {
            list_widget_->show();
        }

        no_app_visible_normal_label_->hide();
        no_app_visible_hide_sysapp_label_->hide();
    }
}

void applist_widget::reload_whole_list() {
    if (should_dead_) {
        scanning_done_evt_.set();
        return;
    }

    list_widget_->hide();
    list_j2me_widget_->hide();
    no_app_visible_normal_label_->hide();
    no_app_visible_hide_sysapp_label_->hide();
    
    if (loaded_[0] && j2me_mode_btn_->isChecked()) {
        const bool app_avail = list_j2me_widget_->count() > 0;
        list_j2me_widget_->setVisible(app_avail);
        list_widget_->hide();

        if (!app_avail)
            show_no_apps_avail();

        return;
    }

    if (loaded_[1] && !j2me_mode_btn_->isChecked()) {
        const bool app_avail = list_widget_->count() > 0;
        list_widget_->setVisible(app_avail);
        list_j2me_widget_->hide();

        if (!app_avail)
            show_no_apps_avail();

        return;
    }

    if (j2me_mode_btn_->isChecked()) {
        loaded_[0] = true;
        list_j2me_widget_->clear();
    } else {
        loaded_[1] = true;
        list_widget_->clear();
    }

    loading_label_->show();
    loading_gif_->start();

    // This vector icon list is synced with
    QFuture<void> future;

    const bool in_j2me = j2me_mode_btn_->isChecked();

    if (in_j2me) {
        future = QtConcurrent::run([this]() {
            exit_mutex_.lock();
            scanning_ = true;
            exit_mutex_.unlock();

            exit_mutex_.lock();

            if (should_dead_) {
                scanning_done_evt_.set();
                return;
            }

            std::vector<eka2l1::j2me::app_entry> entries = lister_j2me_->get_entries();
            exit_mutex_.unlock();

            if (entries.size() == 0) {
                show_no_apps_avail();
            } else {
                for (std::size_t i = 0; i < entries.size(); i++) {
                    const std::lock_guard<std::mutex> guard(exit_mutex_);
                    if (should_dead_) {
                        scanning_done_evt_.set();
                        return;
                    }

                    add_registeration_item_j2me(entries[i]);
                }
            }

            const std::lock_guard<std::mutex> guard(exit_mutex_);
            scanning_ = false;
            scanning_done_evt_.set();
        });
    } else {
        future = QtConcurrent::run([this]() {
            exit_mutex_.lock();
            scanning_ = true;
            exit_mutex_.unlock();

            exit_mutex_.lock();
            if (should_dead_) {
                scanning_done_evt_.set();
                return;
            }
            std::vector<eka2l1::apa_app_registry> &registries = lister_->get_registerations();
            exit_mutex_.unlock();

            if (registries.size() == 0) {
                show_no_apps_avail();
            } else {
                for (std::size_t i = 0; i < registries.size(); i++) {
                    const std::lock_guard<std::mutex> guard(exit_mutex_);
                    if (should_dead_) {
                        scanning_done_evt_.set();
                        return;
                    }

                    if (!registries[i].caps.is_hidden) {
                        if (!hide_system_apps_ || (hide_system_apps_ && !is_app_reg_system_app(registries.data() + i))) {
                            add_registeration_item_native(registries[i], static_cast<int>(i));
                        }
                    }
                }
            }

            const std::lock_guard<std::mutex> guard(exit_mutex_);

            scanning_ = false;
            scanning_done_evt_.set();
        });
    }

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (in_j2me) {
        if (list_j2me_widget_->count() == 0) {
            show_no_apps_avail();
        }
    } else {
        if (list_widget_->count() == 0) {
            show_no_apps_avail();
        }
    }
}

void applist_widget::show_no_apps_avail() {
    loading_label_->hide();
    if (hide_system_apps_) {
        no_app_visible_normal_label_->hide();
        no_app_visible_hide_sysapp_label_->show();
    } else {
        no_app_visible_normal_label_->show();
        no_app_visible_hide_sysapp_label_->hide();
    }
}

eka2l1::apa_app_registry *applist_widget::get_registry_from_widget_item(applist_widget_item *item) {
    if (!item) {
        return nullptr;
    }

    std::vector<eka2l1::apa_app_registry> &registries = lister_->get_registerations();

    if (registries.size() <= item->registry_index_) {
        return nullptr;
    }

    return &registries[item->registry_index_];
}

bool applist_widget::launch_from_widget_item(applist_widget_item *item) {
    eka2l1::apa_app_registry *registry = get_registry_from_widget_item(item);
    if (registry) {
        eka2l1::epoc::apa::command_line cmd_line;
        cmd_line.launch_cmd_ = eka2l1::epoc::apa::command_open;

        return lister_->launch_app(*registry, cmd_line, nullptr);
    }

    return false;
}

void applist_widget::add_registeration_item_j2me(const eka2l1::j2me::app_entry &entry) {
    QIcon display_icon;
    QString icon_path_relocalized = QString::fromStdString(eka2l1::add_path(conf_.storage, entry.icon_path_));

    if (entry.icon_path_.empty() || !QDir().exists(icon_path_relocalized)) {
        display_icon = QIcon(":/assets/duck_tank.png");
    } else {
        QPixmap pixmap(icon_path_relocalized);
        pixmap = pixmap.scaled(ICON_GRID_SIZE - QSize(4, 4));

        display_icon = QIcon(pixmap);
    }

    QListWidgetItem *newItem = new applist_widget_item(display_icon, QString::fromStdString(entry.title_),
        static_cast<int>(entry.id_), true, list_j2me_widget_);

    newItem->setSizeHint(ICON_GRID_SIZE + QSize(20, 20));
    newItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

    emit new_registeration_item_come(newItem);
}

void applist_widget::add_registeration_item_native(eka2l1::apa_app_registry &reg, const int index) {
    QString app_name = QString::fromUtf16(reg.mandatory_info.long_caption.to_std_string(nullptr).data(), reg.mandatory_info.long_caption.get_length());

    bool icon_pair_rendered = false;
    QPixmap final_pixmap;

    const std::u16string path_ext = eka2l1::common::lowercase_ucs2_string(eka2l1::path_extension(reg.icon_file_path));

    if (path_ext == u".mif") {
        eka2l1::symfile file_route = io_->open_file(reg.icon_file_path, READ_MODE | BIN_MODE);
        eka2l1::common::create_directories("cache");

        if (file_route) {
            const std::uint64_t mif_last_modified = file_route->last_modify_since_0ad();
            const std::string cached_path = fmt::format("cache/debinarized_{}.svg", eka2l1::common::pystr(app_name.toStdString()).strip_reserverd().strip().std_str());

            std::unique_ptr<QSvgRenderer> renderer = nullptr;

            if (eka2l1::common::exists(cached_path)) {
                if (eka2l1::common::get_last_modifiy_since_ad(eka2l1::common::utf8_to_ucs2(cached_path)) >= mif_last_modified) {
                    renderer = std::make_unique<QSvgRenderer>(QString::fromUtf8(cached_path.c_str()));
                }
            }

            eka2l1::ro_file_stream file_route_stream(file_route.get());
            eka2l1::loader::mif_file file_mif_parser(reinterpret_cast<eka2l1::common::ro_stream *>(&file_route_stream));

            if (!renderer && file_mif_parser.do_parse()) {
                std::vector<std::uint8_t> data;
                int dest_size = 0;
                if (file_mif_parser.read_mif_entry(0, nullptr, dest_size)) {
                    data.resize(dest_size);
                    file_mif_parser.read_mif_entry(0, data.data(), dest_size);

                    eka2l1::common::ro_buf_stream inside_stream(data.data(), data.size());
                    std::unique_ptr<eka2l1::common::wo_std_file_stream> outfile_stream = std::make_unique<eka2l1::common::wo_std_file_stream>(cached_path, true);

                    eka2l1::loader::mif_icon_header header;
                    inside_stream.read(&header, sizeof(eka2l1::loader::mif_icon_header));

                    std::vector<eka2l1::loader::svgb_convert_error_description> errors;
                    std::vector<eka2l1::loader::nvg_convert_error_description> errors_nvg;

                    if (header.type == eka2l1::loader::mif_icon_type_svg) {
                        if (!eka2l1::loader::convert_svgb_to_svg(inside_stream, *outfile_stream, errors)) {
                            if (errors[0].reason_ == eka2l1::loader::svgb_convert_error_invalid_file) {
                                outfile_stream->write(reinterpret_cast<const char *>(data.data()) + sizeof(eka2l1::loader::mif_icon_header), data.size() - sizeof(eka2l1::loader::mif_icon_header));
                            }
                        }

                        outfile_stream.reset();
                        renderer = std::make_unique<QSvgRenderer>(QString::fromUtf8(cached_path.c_str()));
                    } else {
                        inside_stream = eka2l1::common::ro_buf_stream(data.data() + sizeof(eka2l1::loader::mif_icon_header),
                            data.size() - sizeof(eka2l1::loader::mif_icon_header));

                        if (eka2l1::loader::convert_nvg_to_svg(inside_stream, *outfile_stream, errors_nvg)) {
                            outfile_stream.reset();
                            renderer = std::make_unique<QSvgRenderer>(QString::fromUtf8(cached_path.c_str()));
                        } else  {
                            LOG_ERROR(eka2l1::FRONTEND_UI, "Icon for app {} can't be decoded!", header.type, app_name.toStdString());
                            outfile_stream.reset();

                            eka2l1::common::remove(cached_path);
                        }  
                    }
                }
            }

            if (renderer) {
                final_pixmap = QPixmap(ICON_GRID_SIZE);
                final_pixmap.fill(Qt::transparent);

                QPainter painter(&final_pixmap);
                renderer->render(&painter);

                icon_pair_rendered = true;
            }
        }
    } else if (path_ext == u".mbm") {
        eka2l1::symfile file_route = io_->open_file(reg.icon_file_path, READ_MODE | BIN_MODE);
        if (file_route) {
            eka2l1::ro_file_stream file_route_stream(file_route.get());
            eka2l1::loader::mbm_file file_mbm_parser(reinterpret_cast<eka2l1::common::ro_stream *>(&file_route_stream));

            if (file_mbm_parser.do_read_headers() && !file_mbm_parser.sbm_headers.empty()) {
                eka2l1::loader::sbm_header *icon_header = &file_mbm_parser.sbm_headers[0];
                std::vector<std::uint8_t> converted_data(icon_header->size_pixels.x * icon_header->size_pixels.y * 4);
                eka2l1::common::wo_buf_stream converted_write_stream(converted_data.data(), converted_data.size());

                if (eka2l1::epoc::convert_to_rgba8888(fbss_, file_mbm_parser, 0, converted_write_stream)) {
                    QImage main_bitmap_image(converted_data.data(), icon_header->size_pixels.x, icon_header->size_pixels.y,
                        QImage::Format_RGBA8888);

                    final_pixmap = QPixmap::fromImage(main_bitmap_image);
                    icon_pair_rendered = true;
                }
            }
        }
    } else {
        std::optional<eka2l1::apa_app_masked_icon_bitmap> icon_pair = lister_->get_icon(reg, 0);

        if (icon_pair.has_value()) {
            eka2l1::epoc::bitwise_bitmap *main_bitmap = icon_pair->first;
            const std::size_t main_bitmap_data_new_size = main_bitmap->header_.size_pixels.x * main_bitmap->header_.size_pixels.y * 4;
            std::vector<std::uint8_t> main_bitmap_data(main_bitmap_data_new_size);
            eka2l1::common::wo_buf_stream main_bitmap_buf(main_bitmap_data.data(), main_bitmap_data_new_size);

            if (!eka2l1::epoc::convert_to_rgba8888(fbss_, main_bitmap, main_bitmap_buf)) {
                LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load main icon of app {}", app_name.toStdString());
            } else {
                QImage main_bitmap_image(main_bitmap_data.data(), main_bitmap->header_.size_pixels.x, main_bitmap->header_.size_pixels.y,
                    QImage::Format_RGBA8888);

                if (icon_pair->second) {
                    eka2l1::epoc::bitwise_bitmap *second_bitmap = icon_pair->second;
                    const std::size_t second_bitmap_data_new_size = second_bitmap->header_.size_pixels.x * second_bitmap->header_.size_pixels.y * 4;
                    std::vector<std::uint8_t> second_bitmap_data(second_bitmap_data_new_size);
                    eka2l1::common::wo_buf_stream second_bitmap_buf(second_bitmap_data.data(), second_bitmap_data_new_size);

                    if (!eka2l1::epoc::convert_to_rgba8888(fbss_, second_bitmap, second_bitmap_buf, true)) {
                        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load mask bitmap icon of app {}", app_name.toStdString());
                    } else {
                        QImage mask_bitmap_image(second_bitmap_data.data(), second_bitmap->header_.size_pixels.x, second_bitmap->header_.size_pixels.y,
                            QImage::Format_RGBA8888);

                        QImage mask_bitmap_alpha_image = mask_bitmap_image.createAlphaMask().convertToFormat(QImage::Format_Mono);
                        mask_bitmap_alpha_image.invertPixels();
                        QBitmap mask_bitmap_pixmap = QBitmap::fromImage(mask_bitmap_alpha_image);

                        final_pixmap = QPixmap(QSize(main_bitmap->header_.size_pixels.x, main_bitmap->header_.size_pixels.y));
                        final_pixmap.fill(Qt::transparent);

                        QPainter final_pixmap_painter(&final_pixmap);
                        final_pixmap_painter.setClipRegion(QRegion(mask_bitmap_pixmap));
                        final_pixmap_painter.drawImage(QPoint(0, 0), main_bitmap_image);

                        icon_pair_rendered = true;
                    }
                }

                if (!icon_pair_rendered) {
                    final_pixmap = QPixmap::fromImage(main_bitmap_image);
                    icon_pair_rendered = true;
                }
            }
        }
    }

    QIcon final_icon;

    if (!icon_pair_rendered) {
        final_icon = QIcon(":/assets/duck_tank.png");
    } else {
        if ((final_pixmap.size().width() < ICON_GRID_SIZE.width()) && (final_pixmap.size().height() < ICON_GRID_SIZE.height())) {
            QRect position_to_draw = QRect(0, 0, ICON_GRID_SIZE.width(), ICON_GRID_SIZE.height());
            QPixmap another_pixmap(ICON_GRID_SIZE);
            another_pixmap.fill(Qt::transparent);

            QPainter another_pixmap_painter(&another_pixmap);
            another_pixmap_painter.drawPixmap(position_to_draw, final_pixmap);

            final_icon = QIcon(another_pixmap);
        } else {
            final_icon = QIcon(final_pixmap);
        }
    }

    QListWidgetItem *newItem = new applist_widget_item(final_icon, app_name, index, false, list_widget_);
    newItem->setSizeHint(ICON_GRID_SIZE + QSize(20, 20));
    newItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

    // Sometimes app can't have full name. Just display it :)
    QString tool_tip = app_name + tr("<br>App UID: 0x%1").arg(reg.mandatory_info.uid, 0, 16);
    newItem->setToolTip(tool_tip);

    emit new_registeration_item_come(newItem);
}

void applist_widget::set_hide_system_apps(const bool should_hide) {
    if (should_hide != hide_system_apps_) {
        hide_system_apps_ = should_hide;
        request_reload(false);
    }
}

void applist_widget::update_devices(const QStringList &devices, const int index) {
    device_combo_bar_->update_devices(devices, index);
}

void applist_widget::on_device_change_request(int index) {
    emit device_change_request(index);
}

void applist_widget::update_devices(eka2l1::device_manager *device_mngr) {
    const std::lock_guard<std::mutex> guard(device_mngr->lock);
    QStringList list_devices;

    for (std::size_t i = 0; i < device_mngr->total(); i++) {
        eka2l1::device *dvc = device_mngr->get(static_cast<std::uint8_t>(i));
        if (dvc) {
            QString item_des = QString("%1 (%2 - %3)").arg(QString::fromUtf8(dvc->model.c_str()), QString::fromUtf8(dvc->firmware_code.c_str()),
                                                           epocver_to_symbian_readable_name(dvc->ver));
            list_devices.append(item_des);
        }
    }

    update_devices(list_devices, device_mngr->get_current_index());
}

void applist_widget::update_j2me_button_name() {
    if (j2me_mode_btn_->isChecked()) {
        j2me_mode_btn_->setText("J2ME");
    } else {
        j2me_mode_btn_->setText("Symbian");
    }
}

void applist_widget::on_j2me_mode_btn_clicked() {
    QSettings settings;
    settings.setValue(J2ME_MODE_ENABLED_SETTING, j2me_mode_btn_->isChecked());

    update_j2me_button_name();
    reload_whole_list();
}

void applist_widget::request_reload(const bool j2me) {
    if (j2me) {
        loaded_[0] = false;
    } else {
        loaded_[1] = false;
    }

    if ((j2me && j2me_mode_btn_->isChecked()) || (!j2me && !j2me_mode_btn_->isChecked())) {
        reload_whole_list();
    }
}

void applist_widget::request_reload() {
    reload_whole_list();
}

void applist_widget::on_action_rename_j2me_app_triggered() {
    // 打开输入窗口
    bool is_ok = false;
    const QString result = QInputDialog::getText(this, tr("Enter a new name"), QString(), QLineEdit::Normal, context_j2me_item_->text(),
        &is_ok);
    
    // 重命名了就马上更新db
    if (!is_ok) {
        return;
    }

    if (!lister_j2me_->update_name(static_cast<std::uint32_t>(context_j2me_item_->registry_index_), result.toStdString())) {
        QMessageBox::critical(this, tr("Update name failed"), tr("An error occured while trying to rename the app!"));
    } else {
        context_j2me_item_->setText(result);
        lister_j2me_->flush();
    }
}

void applist_widget::on_action_delete_j2me_app_triggered() {
    if (!eka2l1::j2me::uninstall(lister_->get_system(), static_cast<std::uint32_t>(context_j2me_item_->registry_index_))) {
        QMessageBox::critical(this, tr("Delete app failed"), tr("An error occured while trying to delete the app!"));
    } else {
        list_j2me_widget_->removeItemWidget(context_j2me_item_);
        lister_j2me_->flush();

        delete context_j2me_item_;
        context_j2me_item_ = nullptr;
    }
}

void applist_widget::on_j2me_list_widget_custom_menu_requested(const QPoint &pos) {
    applist_widget_item *item = reinterpret_cast<applist_widget_item*>(list_j2me_widget_->itemAt(pos));
    if (item == nullptr) {
        return;
    }

    context_j2me_item_ = item;

    QMenu menu;

    QAction rename_action(tr("Rename"), list_j2me_widget_);
    QAction delete_action(tr("Delete"), list_j2me_widget_);

    connect(&rename_action, &QAction::triggered, this, &applist_widget::on_action_rename_j2me_app_triggered);
    connect(&delete_action, &QAction::triggered, this, &applist_widget::on_action_delete_j2me_app_triggered);

    menu.addAction(&rename_action);
    menu.addAction(&delete_action);
    menu.exec(mapToGlobal(pos));
}