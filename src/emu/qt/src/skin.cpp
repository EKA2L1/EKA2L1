/*
 * Copyright (c) 2026 EKA2L1 Team.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <qt/skin.h>

#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <stb_image.h>

namespace eka2l1::desktop {
    static eka2l1::rect read_rect(const QJsonObject &object) {
        return eka2l1::rect(eka2l1::vec2(object["x"].toInt(), object["y"].toInt()),
            eka2l1::vec2(object["w"].toInt(), object["h"].toInt()));
    }

    bool skin_layout::load(const std::string &json_path) {
        QFile file(QString::fromStdString(json_path));
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_ERROR(FRONTEND_UI, "Unable to open the skin layout {}", json_path);
            return false;
        }

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (!document.isObject()) {
            LOG_ERROR(FRONTEND_UI, "Skin layout {} is not valid: {}", json_path, error.errorString().toStdString());
            return false;
        }

        const QJsonObject root = document.object();
        const QJsonObject size_object = root["size"].toObject();

        path = json_path;
        size = eka2l1::vec2(size_object["w"].toInt(), size_object["h"].toInt());
        screen = read_rect(root["screen"].toObject());

        if ((size.x <= 0) || (size.y <= 0) || (screen.size.x <= 0) || (screen.size.y <= 0)) {
            LOG_ERROR(FRONTEND_UI, "Skin layout {} has no size or no screen", json_path);
            return false;
        }

        const std::string folder = eka2l1::file_directory(json_path);
        image_path = eka2l1::add_path(folder, root["image"].toString().toStdString());
        if (root.contains("lit_image")) {
            lit_image_path = eka2l1::add_path(folder, root["lit_image"].toString().toStdString());
        }

        keys.clear();
        for (const QJsonValue &value : root["keys"].toArray()) {
            const QJsonObject key_object = value.toObject();
            if (!key_object.contains("code")) {
                continue;
            }

            skin_key key;
            key.code = static_cast<std::uint32_t>(key_object["code"].toInt());
            key.area = read_rect(key_object);
            key.lit = read_rect(key_object["lit"].toObject());
            key.ellipse = (key_object["shape"].toString() == "ellipse");

            keys.push_back(key);
        }

        return true;
    }

    static drivers::handle upload_picture(drivers::graphics_driver *driver, const std::string &picture_path) {
        FILE *f = eka2l1::common::open_c_file(picture_path, "rb");
        if (!f) {
            LOG_ERROR(FRONTEND_UI, "Unable to open the skin picture {}", picture_path);
            return 0;
        }

        int x = 0;
        int y = 0;
        int comp = STBI_rgb_alpha;

        stbi_uc *data = stbi_load_from_file(f, &x, &y, &comp, STBI_rgb_alpha);
        fclose(f);

        if (!data) {
            LOG_ERROR(FRONTEND_UI, "Unable to read the skin picture {}", picture_path);
            return 0;
        }

        drivers::handle texture = drivers::create_texture(driver, 2, 0, drivers::texture_format::rgba, drivers::texture_format::rgba,
            drivers::texture_data_type::ubyte, data, x * y * 4, eka2l1::vec3(x, y, 0));

        stbi_image_free(data);

        if (texture) {
            // The picture is scaled to the window
            drivers::graphics_command_builder builder;
            builder.set_texture_filter(texture, true, drivers::filter_option::linear);
            builder.set_texture_filter(texture, false, drivers::filter_option::linear);

            auto cmd_list = builder.retrieve_command_list();
            driver->submit_command_list(cmd_list);
        }

        return texture;
    }

    bool skin_layout::upload(drivers::graphics_driver *driver) {
        image = upload_picture(driver, image_path);
        if (!image) {
            return false;
        }

        if (!lit_image_path.empty()) {
            lit_image = upload_picture(driver, lit_image_path);
        }

        LOG_INFO(FRONTEND_UI, "Skin {} ready, {} keys", path, keys.size());
        return true;
    }

    const skin_key *skin_layout::key_at(const eka2l1::vec2 &point) const {
        for (const skin_key &key : keys) {
            const eka2l1::rect &area = key.area;
            if ((point.x < area.top.x) || (point.y < area.top.y) || (point.x >= area.top.x + area.size.x) || (point.y >= area.top.y + area.size.y)) {
                continue;
            }

            if (key.ellipse) {
                const float rx = area.size.x / 2.0f;
                const float ry = area.size.y / 2.0f;
                const float dx = (point.x - area.top.x - rx) / rx;
                const float dy = (point.y - area.top.y - ry) / ry;

                if (dx * dx + dy * dy > 1.0f) {
                    continue;
                }
            }

            return &key;
        }

        return nullptr;
    }
}
