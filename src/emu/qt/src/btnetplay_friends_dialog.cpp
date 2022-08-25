/*
 * Copyright (c) 2022 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project
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

#include <qt/btnetplay_friends_dialog.h>
#include <services/bluetooth/protocols/common.h>
#include <services/bluetooth/protocols/btmidman_inet.h>

#include <QIntValidator>
#include <QPushButton>
#include <QMessageBox>

btnetplay_address_field::btnetplay_address_field(QWidget *parent, const int num)
    : QWidget(parent)
    , address_edit_(nullptr)
    , port_edit_(nullptr)
    , numbering_label_(nullptr) {
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHorizontalStretch(4);
    address_edit_ = new QLineEdit;
    address_edit_->setSizePolicy(policy);

    port_edit_ = new QLineEdit;
    policy.setHorizontalStretch(2);
    port_edit_->setSizePolicy(policy);

    numbering_label_ = new QLabel(QString::number(num));
    policy.setHorizontalStretch(1);
    numbering_label_->setAlignment(Qt::AlignCenter);
    numbering_label_->setSizePolicy(policy);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(numbering_label_);
    layout->addWidget(address_edit_);
    layout->addWidget(port_edit_);

    port_edit_->setValidator(new QIntValidator(0, 65535, this));

    setLayout(layout);

    layout->setContentsMargins(0, 0, 0, 0);
    setContentsMargins(0, 0, 0, 0);
}

btnetplay_friends_dialog::btnetplay_friends_dialog(QWidget *parent, eka2l1::epoc::bt::midman_inet *midman, eka2l1::config::state &conf)
    : QDialog(parent)
    , friends_layout_(nullptr)
    , grid_layout_(nullptr)
    , add_more_btn_(nullptr)
    , remove_btn_(nullptr)
    , register_btn_(nullptr)
    , conf_(conf)
    , current_friend_count_(0)
    , midman_(midman) {
    setWindowTitle(tr("Modify friends' IP addresses"));
    setAttribute(Qt::WA_DeleteOnClose);

    grid_layout_ = new QGridLayout(this);
    friends_layout_ = new QVBoxLayout;

    QHBoxLayout *info_row_layout = new QHBoxLayout;

    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHorizontalStretch(1);

    QLabel *empty_label = new QLabel(" ");
    empty_label->setSizePolicy(policy);

    QLabel *address_label = new QLabel;
    address_label->setText(tr("IP address (IPv4 or IPv6)"));
    policy.setHorizontalStretch(4);
    address_label->setSizePolicy(policy);

    QLabel *port_label = new QLabel;
    port_label->setText(tr("Port (can be empty)"));
    policy.setHorizontalStretch(2);
    port_label->setSizePolicy(policy);

    info_row_layout->addWidget(empty_label);
    info_row_layout->addWidget(address_label);
    info_row_layout->addWidget(port_label);
    info_row_layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QWidget *info_row_widget = new QWidget;
    info_row_widget->setLayout(info_row_layout);

    friends_layout_->addWidget(info_row_widget);

    QWidget *friends_widget = new QWidget;
    friends_widget->setLayout(friends_layout_);

    QHBoxLayout *buttons_layout = new QHBoxLayout;
    QWidget *buttons_widget = new QWidget;
    buttons_widget->setLayout(buttons_layout);

    grid_layout_->addWidget(friends_widget, 0, 0);
    grid_layout_->addWidget(buttons_widget, 1, 0);

    for (std::size_t i = 0; i < eka2l1::epoc::bt::MAX_INET_DEVICE_AROUND; i++) {
        btnetplay_address_field *field = new btnetplay_address_field(nullptr, static_cast<int>(i + 1));

        if (i < conf_.friend_addresses.size()) {
            field->set_address_value(QString::fromStdString(conf_.friend_addresses[i].addr_));
            field->set_port_value(conf_.friend_addresses[i].port_);
        } else {
            field->hide();
        }

        friends_layout_->addWidget(field);
    }

    current_friend_count_ = conf_.friend_addresses.size();

    add_more_btn_ = new QPushButton;
    remove_btn_ = new QPushButton;
    register_btn_ = new QPushButton;

    add_more_btn_->setText(tr("Add more"));
    remove_btn_->setText(tr("Remove"));
    register_btn_->setText(tr("Update"));

    if (current_friend_count_ == eka2l1::epoc::bt::MAX_INET_DEVICE_AROUND) {
        add_more_btn_->hide();
    }

    buttons_layout->addWidget(add_more_btn_);
    buttons_layout->addWidget(remove_btn_);
    buttons_layout->addWidget(register_btn_);

    connect(add_more_btn_, &QPushButton::pressed, this, &btnetplay_friends_dialog::on_add_more_clicked);
    connect(remove_btn_, &QPushButton::pressed, this, &btnetplay_friends_dialog::on_remove_clicked);
    connect(register_btn_, &QPushButton::pressed, this, &btnetplay_friends_dialog::on_save_clicked);
}

btnetplay_friends_dialog::~btnetplay_friends_dialog() {
}

void btnetplay_friends_dialog::on_add_more_clicked() {
    bool should_disable = false;
    if (current_friend_count_ > 0) {
        if (current_friend_count_ == eka2l1::epoc::bt::MAX_INET_DEVICE_AROUND - 1) {
            should_disable = true;
        }
    }
    if (should_disable) {
        add_more_btn_->setDisabled(true);
    }

    btnetplay_address_field *addr_field = reinterpret_cast<btnetplay_address_field*>(friends_layout_->itemAt(static_cast<int>(current_friend_count_ + 1))->widget());
    addr_field->show();

    current_friend_count_++;
}

void btnetplay_friends_dialog::on_remove_clicked() {
    if (current_friend_count_ == 1) {
        remove_btn_->setDisabled(true);
    }

    btnetplay_address_field *addr_field = reinterpret_cast<btnetplay_address_field*>(friends_layout_->itemAt(static_cast<int>(current_friend_count_))->widget());
    addr_field->hide();

    current_friend_count_--;
}

void btnetplay_friends_dialog::on_save_clicked() {
    eka2l1::config::friend_address addr_temp;
    std::vector<eka2l1::config::friend_address> friend_collection;

    for (std::size_t i = 0; i < current_friend_count_; i++) {
        btnetplay_address_field *addr_field = reinterpret_cast<btnetplay_address_field*>(friends_layout_->itemAt(static_cast<int>(i + 1))->widget());
        addr_temp.addr_ = addr_field->get_address_value().toStdString();
        addr_temp.port_ = addr_field->get_port_value();

        if (addr_temp.port_ == 0xFFFFFFFF) {
            addr_temp.port_ = 35689;        // Default port
        }

        friend_collection.push_back(addr_temp);
    }

    std::vector<std::uint64_t> errors;
    midman_->update_friend_list(friend_collection, errors);

    if (errors.empty()) {
        QMessageBox::information(this, tr("Update friends success!"), tr("Friends' IP addresses have been successfully saved!"));

        conf_.friend_addresses = std::move(friend_collection);
        conf_.serialize();
    } else {
        std::vector<eka2l1::config::friend_address> friend_collection_filtered;

        QString error_string;
        for (std::size_t i = 0; i < errors.size(); i++) {
            std::uint32_t error_index = static_cast<std::uint32_t>((errors[i] & eka2l1::epoc::bt::FRIEND_UPDATE_INDEX_FAULT_MASK));
            switch (errors[i] & eka2l1::epoc::bt::FRIEND_UPDATE_ERROR_MASK) {
            case eka2l1::epoc::bt::FRIEND_UPDATE_ERROR_INVALID_ADDR:
                if (friend_collection[i].addr_.empty()) {
                    error_string += tr("Friend %1 has empty IP address!\n").arg(error_index + 1);
                } else {
                    error_string += tr("Friend %1 has invalid IP address!\n").arg(error_index + 1);
                }
                break;

            case eka2l1::epoc::bt::FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER:
                error_string += tr("Friend %1 has invalid port number (must be between 0 and 65535)!\n").arg(error_index + 1);
                break;

            default:
                break;
            }
        }

        for (std::size_t i = 0; i < friend_collection.size(); i++) {
            bool fault = false;
            for (std::size_t j = 0; j < errors.size(); j++) {
                if (static_cast<std::size_t>(errors[j] & eka2l1::epoc::bt::FRIEND_UPDATE_INDEX_FAULT_MASK) == i) {
                    errors.erase(errors.begin() + j);

                    fault = true;
                    break;
                }
            }
            if (!fault) {
                friend_collection_filtered.push_back(friend_collection[i]);
            }
        }

        conf_.friend_addresses = std::move(friend_collection_filtered);
        conf_.serialize();

        QMessageBox box(this);
        box.setWindowTitle(tr("Some errors in updating friends!"));
        box.setText(tr("Some friends' IP addresses can't be updated (see detailed text).<br>"
                       "Addresses that are able to update have been saved."));
        box.setDetailedText(error_string);
        box.exec();
    }
}
