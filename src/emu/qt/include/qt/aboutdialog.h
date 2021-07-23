#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>

namespace Ui {
class about_dialog;
}

class about_dialog : public QDialog
{
    Q_OBJECT

private:
    QLabel *main_developer_label_;
    QLabel *contributor_label_;
    QLabel *icon_label_;
    QLabel *honor_label_;
    QLabel *translator_label_;
    QLabel *version_label_;
    QLabel *copyright_label_;

public:
    explicit about_dialog(QWidget *parent = nullptr);
    ~about_dialog();

private:
    Ui::about_dialog *ui_;
};

#endif // ABOUTDIALOG_H
