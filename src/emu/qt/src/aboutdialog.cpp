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

#include "ui_aboutdialog.h"
#include <qt/aboutdialog.h>

#include <common/log.h>
#include <common/version.h>
#include <yaml-cpp/yaml.h>

#include <QFile>
#include <QTextStream>

about_dialog::about_dialog(QWidget *parent)
    : QDialog(parent)
    , ui_(new Ui::about_dialog)
    , main_developer_label_(nullptr)
    , contributor_label_(nullptr)
    , icon_label_(nullptr)
    , honor_label_(nullptr)
    , translator_label_(nullptr)
    , version_label_(nullptr) {
    ui_->setupUi(this);

    // Load the whole file in for reading
    QFile credit_yaml_file(":/assets/credits.yml");
    credit_yaml_file.open(QFile::ReadOnly | QFile::Text);

    QTextStream credit_yaml_text_stream(&credit_yaml_file);
    std::string credit_yaml_content = credit_yaml_text_stream.readAll().toStdString();

    YAML::Node credit_yaml_tree;

    version_label_ = new QLabel(tr("<i>Commit <b>%1</b> branch <b>%2</b><i><br>").arg(QString::fromUtf8(GIT_COMMIT_HASH), QString::fromUtf8(GIT_BRANCH)));
    ui_->credit_layout->addWidget(version_label_);

    copyright_label_ = new QLabel(tr("<b>(C) 2018- EKA2L1 Team</b><br><b>Thank you for using the emulator!</b><br>"));
    ui_->credit_layout->addWidget(copyright_label_);

    try {
        credit_yaml_tree = YAML::Load(credit_yaml_content);
    } catch (const std::exception &exp) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load credit file!");
        return;
    }

    try {
        QString developer_str = tr("<b>Main developers:</b><br>");

        for (const auto node : credit_yaml_tree["MainDevs"]) {
            developer_str += QString::fromStdString("- " + node.as<std::string>() + "<br>");
        }

        main_developer_label_ = new QLabel(developer_str);
        ui_->credit_layout->addWidget(main_developer_label_);
    } catch (const std::exception &exp) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load developer section of credit file!");
        return;
    }

    try {
        QString contributor_str = tr("<b>Contributors:</b><br>");

        for (const auto node : credit_yaml_tree["Contributors"]) {
            contributor_str += QString::fromStdString("- " + node.as<std::string>() + "<br>");
        }

        contributor_label_ = new QLabel(contributor_str);
        ui_->credit_layout->addWidget(contributor_label_);
    } catch (const std::exception &exp) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load contributors section of credit file!");
        return;
    }

    try {
        QString icon_str = tr("<b>Icon:</b><br>");

        for (const auto node : credit_yaml_tree["Icon"]) {
            icon_str += QString::fromStdString("- " + node.as<std::string>() + "<br>");
        }

        icon_label_ = new QLabel(icon_str);
        ui_->credit_layout->addWidget(icon_label_);
    } catch (const std::exception &exp) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load icon author section of credit file!");
        return;
    }

    try {
        QString honor_str = tr("<b>Honors:</b><br>");

        for (const auto node : credit_yaml_tree["Honors"]) {
            honor_str += QString::fromStdString("- " + node.as<std::string>() + "<br>");
        }

        honor_label_ = new QLabel(honor_str);
        ui_->credit_layout->addWidget(honor_label_);
    } catch (const std::exception &exp) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load honors section of credit file!");
        return;
    }

    try {
        QString translator_str = tr("<b>Translators:</b><br>");

        for (const auto node : credit_yaml_tree["TranslatorsDesktop"]) {
            translator_str += QString::fromStdString("- " + node.as<std::string>() + "<br>");
        }

        translator_label_ = new QLabel(translator_str);
        ui_->credit_layout->addWidget(translator_label_);
    } catch (const std::exception &exp) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load translators section of credit file!");
        return;
    }
}

about_dialog::~about_dialog() {
    if (translator_label_)
        delete translator_label_;

    if (honor_label_)
        delete honor_label_;

    if (icon_label_)
        delete icon_label_;

    if (contributor_label_)
        delete contributor_label_;

    if (main_developer_label_)
        delete main_developer_label_;

    delete copyright_label_;
    delete version_label_;
    delete ui_;
}
