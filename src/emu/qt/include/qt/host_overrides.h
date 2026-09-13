// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QTreeWidget;
namespace eka2l1 {
    class system;
    namespace config { struct state; }
}

class host_overrides_widget : public QWidget {
    Q_OBJECT

    eka2l1::config::state &config_;
    eka2l1::system *system_;
    QTreeWidget *entries_;

    void refresh();
    void edit(bool create);
    void remove();

public:
    host_overrides_widget(eka2l1::config::state &config, eka2l1::system *system, QWidget *parent = nullptr);
};
