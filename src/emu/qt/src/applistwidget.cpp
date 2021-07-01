#include <qt/applistwidget.h>
#include <services/applist/applist.h>
#include <services/fbs/fbs.h>
#include <common/buffer.h>

#include <vector>
#include <QPainter>
#include <QBitmap>

static QSize ICON_GRID_SIZE = QSize(64, 64);

applist_widget::applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss)
    : QListWidget(parent)
    , lister_(lister)
    , fbss_(fbss)
{
    // Well, just make it a part of the window. Hehe
    setStyleSheet("border: none; background: transparent");

    setFlow(Flow::LeftToRight);
    setResizeMode(ResizeMode::Adjust);
    setSpacing(100);
    setGridSize(ICON_GRID_SIZE + QSize(20, 20));
    setIconSize(ICON_GRID_SIZE);
    setViewMode(ViewMode::IconMode);

    // This vector icon list is synced with
    std::vector<eka2l1::apa_app_registry> &registries = lister_->get_registerations();
    for (eka2l1::apa_app_registry &reg: registries) {
        if (!reg.caps.is_hidden) {
            add_registeration_item(reg);
        }
    }
}

void applist_widget::add_registeration_item(eka2l1::apa_app_registry &reg) {
    QString app_name = QString::fromUtf16(reg.mandatory_info.long_caption.to_std_string(nullptr).data(), reg.mandatory_info.long_caption.get_length());

    std::optional<eka2l1::apa_app_masked_icon_bitmap> icon_pair = lister_->get_icon(reg, 0);

    bool icon_pair_rendered = false;
    QPixmap final_pixmap;

    if (icon_pair.has_value()) {
        eka2l1::epoc::bitwise_bitmap *main_bitmap = icon_pair->first;
        const std::size_t main_bitmap_data_new_size = main_bitmap->header_.size_pixels.x * main_bitmap->header_.size_pixels.y * 4;
        std::vector<std::uint8_t> main_bitmap_data(main_bitmap_data_new_size);
        eka2l1::common::wo_buf_stream main_bitmap_buf(main_bitmap_data.data(), main_bitmap_data_new_size);

        if (!eka2l1::epoc::convert_to_argb8888(fbss_, main_bitmap, main_bitmap_buf)) {
            LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load main icon of app {}", app_name.toStdString());
        } else {
            QImage main_bitmap_image(main_bitmap_data.data(), main_bitmap->header_.size_pixels.x, main_bitmap->header_.size_pixels.y,
                                     QImage::Format_RGBA8888);
            if (icon_pair->second) {
                eka2l1::epoc::bitwise_bitmap *second_bitmap = icon_pair->second;
                const std::size_t second_bitmap_data_new_size = second_bitmap->header_.size_pixels.x * second_bitmap->header_.size_pixels.y * 4;
                std::vector<std::uint8_t> second_bitmap_data(second_bitmap_data_new_size);
                eka2l1::common::wo_buf_stream second_bitmap_buf(second_bitmap_data.data(), second_bitmap_data_new_size);

                if (!eka2l1::epoc::convert_to_argb8888(fbss_, second_bitmap, second_bitmap_buf)) {
                    LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load mask bitmap icon of app {}", app_name.toStdString());
                } else {
                    QImage mask_bitmap_image(second_bitmap_data.data(), second_bitmap->header_.size_pixels.x, second_bitmap->header_.size_pixels.y,
                                             QImage::Format_RGBA8888);

                    QImage mask_bitmap_alpha_image = mask_bitmap_image.createAlphaMask();
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

    QIcon final_icon;

    if (!icon_pair_rendered) {
        final_icon = QIcon(":/assets/duck_icon.png");
    } else {
        if ((final_pixmap.size().width() < ICON_GRID_SIZE.width()) && (final_pixmap.size().height() < ICON_GRID_SIZE.height())) {
            QPoint position_to_draw = QPoint(((64 - final_pixmap.size().width()) / 2), ((64 - final_pixmap.size().height()) / 2));
            QPixmap another_pixmap(ICON_GRID_SIZE);
            another_pixmap.fill(Qt::transparent);

            QPainter another_pixmap_painter(&another_pixmap);
            another_pixmap_painter.drawPixmap(position_to_draw, final_pixmap);

            final_icon = QIcon(another_pixmap);
        } else {
            final_icon = QIcon(final_pixmap);
        }
    }

    QListWidgetItem *newItem = new QListWidgetItem(final_icon, app_name, this);
    newItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

    // Sometimes app can't have full name. Just display it :)
    QString tool_tip = app_name + tr("<br>App UID: 0x%1").arg(reg.mandatory_info.uid, 0, 16);
    newItem->setToolTip(tool_tip);

    addItem(newItem);
}
