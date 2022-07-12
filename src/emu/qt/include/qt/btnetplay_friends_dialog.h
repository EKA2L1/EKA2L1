#ifndef BTNETPLAY_FRIENDS_DIALOG_H
#define BTNETPLAY_FRIENDS_DIALOG_H

#include <services/bluetooth/protocols/btmidman_inet.h>
#include <config/config.h>

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QWidget>

struct btnetplay_address_field : public QWidget {
    Q_OBJECT;

private:
    QLineEdit *address_edit_;
    QLineEdit *port_edit_;
    QLabel *numbering_label_;

public:
    btnetplay_address_field(QWidget *parent = nullptr, const int num = 0);

    QString get_address_value() const {
        return address_edit_->text();
    }

    std::uint32_t get_port_value() const {
        if (port_edit_->text().isEmpty()) {
            return 0xFFFFFFFF;
        }

        return static_cast<std::uint32_t>(port_edit_->text().toULong());
    }

    void set_address_value(const QString &addr) {
        address_edit_->setText(addr);
    }

    void set_port_value(const std::uint32_t port) {
        port_edit_->setText(QString::number(port));
    }
};

class btnetplay_friends_dialog : public QDialog {
    Q_OBJECT;

private:
    QVBoxLayout *friends_layout_;
    QGridLayout *grid_layout_;

    QPushButton *add_more_btn_;
    QPushButton *remove_btn_;
    QPushButton *register_btn_;

    eka2l1::config::state &conf_;
    std::size_t current_friend_count_;

    eka2l1::epoc::bt::midman_inet *midman_;

private slots:
    void on_add_more_clicked();
    void on_remove_clicked();
    void on_save_clicked();

public:
    btnetplay_friends_dialog(QWidget *parent, eka2l1::epoc::bt::midman_inet *midman, eka2l1::config::state &conf);
    ~btnetplay_friends_dialog();
};

#endif // BTNETPLAY_FRIENDS_DIALOG_H
