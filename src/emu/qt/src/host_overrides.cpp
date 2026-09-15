// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <qt/host_overrides.h>
#include <config/config.h>
#include <kernel/kernel.h>
#include <system/epoc.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

host_overrides_widget::host_overrides_widget(eka2l1::config::state &config, eka2l1::system *system, QWidget *parent)
    : QWidget(parent), config_(config), system_(system), entries_(new QTreeWidget(this)) {
    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(tr("Redirect guest hostnames or wildcard suffixes to an IP address or another hostname, optionally with a port. Changes affect new connections; restart the game if it has cached an address."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    entries_->setObjectName("host_mappings");
    entries_->setHeaderLabels({tr("Hostname or *.suffix"), tr("Target and optional port")});
    entries_->setRootIsDecorated(false);
    entries_->header()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(entries_);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add"), this);
    auto *change = new QPushButton(tr("Edit"), this);
    auto *erase = new QPushButton(tr("Remove"), this);
    for (auto *button : {add, change, erase}) {
        button->setAutoDefault(false);
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this] { edit(true); });
    connect(change, &QPushButton::clicked, this, [this] { edit(false); });
    connect(erase, &QPushButton::clicked, this, &host_overrides_widget::remove);
    connect(entries_, &QTreeWidget::itemActivated, this, [this] { edit(false); });
    connect(entries_, &QTreeWidget::itemSelectionChanged, this, [this, change, erase] {
        change->setEnabled(entries_->currentItem() != nullptr);
        erase->setEnabled(entries_->currentItem() != nullptr);
    });
    change->setEnabled(false);
    erase->setEnabled(false);
    refresh();
}

void host_overrides_widget::refresh() {
    entries_->clear();
    for (const auto &[name, target] : config_.hosts) {
        new QTreeWidgetItem(entries_, {QString::fromStdString(name), QString::fromStdString(target)});
    }
}

void host_overrides_widget::edit(bool create) {
    const auto *selected = entries_->currentItem();
    if (!create && !selected) {
        return;
    }
    const auto old_name = create ? std::string{} : selected->text(0).toStdString();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Host mapping"));
    auto *layout = new QFormLayout(&dialog);
    auto *name = new QLineEdit(QString::fromStdString(old_name), &dialog);
    auto *target = new QLineEdit(create ? QString{} : selected->text(1), &dialog);
    name->setObjectName("host_name");
    target->setObjectName("host_target");
    layout->addRow(tr("Hostname or *.suffix"), name);
    layout->addRow(tr("Target and optional port"), target);
    auto *error = new QLabel(&dialog);
    error->setWordWrap(true);
    layout->addRow(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto hostname = eka2l1::config::normalize_host_name(name->text().toStdString());
        const auto address = eka2l1::config::normalize_host_name(target->text().toStdString());
        if (!eka2l1::config::valid_host_pattern(hostname) || !eka2l1::config::valid_host_target(address)) {
            error->setText(tr("Enter a valid hostname or *.suffix and an IP address or hostname with an optional port."));
            return;
        }
        for (const auto &[existing, value] : config_.hosts) {
            if (existing != old_name && eka2l1::config::normalize_host_name(existing) == hostname) {
                error->setText(tr("This hostname already has a mapping. Edit the existing entry."));
                return;
            }
        }
        auto *kernel = system_->get_kernel_system();
        if (kernel) kernel->lock();
        config_.hosts.erase(old_name);
        config_.hosts[hostname] = address;
        if (kernel) kernel->unlock();
        config_.serialize();
        dialog.accept();
    });
    if (dialog.exec() == QDialog::Accepted) {
        refresh();
    }
}

void host_overrides_widget::remove() {
    if (const auto *selected = entries_->currentItem()) {
        auto *kernel = system_->get_kernel_system();
        if (kernel) kernel->lock();
        config_.hosts.erase(selected->text(0).toStdString());
        if (kernel) kernel->unlock();
        config_.serialize();
        refresh();
    }
}
