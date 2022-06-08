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
#include <QtSvg/QSvgRenderer>

#include <common/buffer.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/pystr.h>
#include <qt/applistwidget.h>
#include <services/applist/applist.h>
#include <services/fbs/fbs.h>
#include <utils/apacmd.h>

#include <vector>

#include <loader/mif.h>
#include <loader/svgb.h>

static QSize ICON_GRID_SIZE = QSize(64, 64);
static QSize ICON_GRID_SPACING_SIZE = QSize(20, 20);
static constexpr std::uint32_t WARE_APP_UID_START = 0x10300000;

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

    setLayout(search_layout_);

    connect(search_line_edit_, &QLineEdit::textChanged, this, &applist_search_bar::on_search_bar_content_changed);
}

applist_search_bar::~applist_search_bar() {
    delete search_layout_;
    delete search_line_edit_;
    delete search_label_;
}

void applist_search_bar::on_search_bar_content_changed(QString content) {
    emit new_search(content);
}

applist_widget_item::applist_widget_item(const QIcon &icon, const QString &name, int registry_index, QListWidget *parent)
    : QListWidgetItem(icon, name, parent)
    , registry_index_(registry_index) {
}

applist_widget::applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss, eka2l1::io_system *io, const bool hide_system_apps, const bool ngage_mode)
    : QWidget(parent)
    , search_bar_(nullptr)
    , list_widget_(nullptr)
    , layout_(nullptr)
    , lister_(lister)
    , fbss_(fbss)
    , io_(io)
    , hide_system_apps_(hide_system_apps) {
    search_bar_ = new applist_search_bar(this);
    list_widget_ = new QListWidget(this);

    layout_ = new QGridLayout(this);
    layout_->addWidget(search_bar_);
    layout_->addWidget(list_widget_);
    layout_->setContentsMargins(0, 0, 0, 0);

    setLayout(layout_);

    // Well, just make it a part of the window. Hehe
    list_widget_->setFrameShape(QFrame::NoFrame);
    list_widget_->viewport()->setAutoFillBackground(false);

    list_widget_->setFlow(QListWidget::Flow::LeftToRight);
    list_widget_->setResizeMode(QListWidget::ResizeMode::Adjust);
    list_widget_->setSpacing(100);
    list_widget_->setGridSize(ICON_GRID_SIZE + ICON_GRID_SPACING_SIZE);
    list_widget_->setIconSize(ICON_GRID_SIZE);
    list_widget_->setViewMode(QListWidget::ViewMode::IconMode);
    list_widget_->setMovement(QListWidget::Movement::Static);
    list_widget_->setMinimumHeight(ICON_GRID_SIZE.height() + ICON_GRID_SPACING_SIZE.height() + 200);

    reload_whole_list();

    connect(list_widget_, &QListWidget::itemClicked, this, &applist_widget::on_list_widget_item_clicked);
    connect(search_bar_, &applist_search_bar::new_search, this, &applist_widget::on_search_content_changed);
}

applist_widget::~applist_widget() {
    delete layout_;
    delete search_bar_;
    delete list_widget_;
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

void applist_widget::reload_whole_list() {
    list_widget_->clear();

    // This vector icon list is synced with
    std::vector<eka2l1::apa_app_registry> &registries = lister_->get_registerations();
    for (std::size_t i = 0; i < registries.size(); i++) {
        if (!registries[i].caps.is_hidden) {
            if (!hide_system_apps_ || (hide_system_apps_ && !is_app_reg_system_app(registries.data() + i))) {
                add_registeration_item(registries[i], static_cast<int>(i));
            }
        }
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

void applist_widget::add_registeration_item(eka2l1::apa_app_registry &reg, const int index) {
    QString app_name = QString::fromUtf16(reg.mandatory_info.long_caption.to_std_string(nullptr).data(), reg.mandatory_info.long_caption.get_length());

    bool icon_pair_rendered = false;
    QPixmap final_pixmap;

    const std::u16string path_ext = eka2l1::common::lowercase_ucs2_string(eka2l1::path_extension(reg.icon_file_path));

    if (path_ext == u".mif") {
        eka2l1::symfile file_route = io_->open_file(reg.icon_file_path, READ_MODE | BIN_MODE);
        eka2l1::common::create_directories("cache");

        if (file_route) {
            eka2l1::ro_file_stream file_route_stream(file_route.get());
            eka2l1::loader::mif_file file_mif_parser(reinterpret_cast<eka2l1::common::ro_stream *>(&file_route_stream));

            if (file_mif_parser.do_parse()) {
                std::vector<std::uint8_t> data;
                int dest_size = 0;
                if (file_mif_parser.read_mif_entry(0, nullptr, dest_size)) {
                    data.resize(dest_size);
                    file_mif_parser.read_mif_entry(0, data.data(), dest_size);

                    const std::string cached_path = fmt::format("cache/debinarized_{}.svg", eka2l1::common::pystr(app_name.toStdString()).strip_reserverd().strip().std_str());

                    eka2l1::common::ro_buf_stream inside_stream(data.data(), data.size());
                    std::unique_ptr<eka2l1::common::wo_std_file_stream> outfile_stream = std::make_unique<eka2l1::common::wo_std_file_stream>(cached_path, true);

                    eka2l1::loader::mif_icon_header header;
                    inside_stream.read(&header, sizeof(eka2l1::loader::mif_icon_header));

                    std::vector<eka2l1::loader::svgb_convert_error_description> errors;

                    if (header.type == eka2l1::loader::mif_icon_type_svg) {
                        std::unique_ptr<QSvgRenderer> renderer = nullptr;
                        if (!eka2l1::loader::convert_svgb_to_svg(inside_stream, *outfile_stream, errors)) {
                            if (errors[0].reason_ == eka2l1::loader::svgb_convert_error_invalid_file) {
                                QByteArray content(reinterpret_cast<const char *>(data.data()) + sizeof(eka2l1::loader::mif_icon_header), data.size() - sizeof(eka2l1::loader::mif_icon_header));
                                renderer = std::make_unique<QSvgRenderer>(content);
                            }
                        } else {
                            outfile_stream.reset();
                            renderer = std::make_unique<QSvgRenderer>(QString::fromUtf8(cached_path.c_str()));
                        }

                        if (renderer) {
                            final_pixmap = QPixmap(ICON_GRID_SIZE);
                            final_pixmap.fill(Qt::transparent);

                            QPainter painter(&final_pixmap);
                            renderer->render(&painter);

                            icon_pair_rendered = true;
                        }
                    } else {
                        LOG_ERROR(eka2l1::FRONTEND_UI, "Unknown icon type {} for app {}", header.type, app_name.toStdString());
                    }
                }
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

    QListWidgetItem *newItem = new applist_widget_item(final_icon, app_name, index, list_widget_);
    newItem->setSizeHint(ICON_GRID_SIZE + QSize(20, 20));
    newItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

    // Sometimes app can't have full name. Just display it :)
    QString tool_tip = app_name + tr("<br>App UID: 0x%1").arg(reg.mandatory_info.uid, 0, 16);
    newItem->setToolTip(tool_tip);

    list_widget_->addItem(newItem);
}

void applist_widget::set_hide_system_apps(const bool should_hide) {
    if (should_hide != hide_system_apps_) {
        hide_system_apps_ = should_hide;
        reload_whole_list();
    }
}