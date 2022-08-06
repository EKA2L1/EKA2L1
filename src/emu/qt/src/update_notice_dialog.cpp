#include <qt/update_notice_dialog.h>
#include "ui_update_notice_dialog.h"

#include <QSettings>

#define UPDATE_NOTICE_DATE "20220807"

static const char *UPDATE_NOTICE_SHOWN_SETTING = "updateNotice" UPDATE_NOTICE_DATE "Shown";

update_notice_dialog::update_notice_dialog(QWidget *parent)
    : QDialog(parent)
    , ui_(new Ui::update_notice_dialog) {
    setAttribute(Qt::WA_DeleteOnClose);
    ui_->setupUi(this);

    show();
}

update_notice_dialog::~update_notice_dialog() {
    delete ui_;
}

void update_notice_dialog::spawn(QWidget *parent) {
    QSettings local_settings;
    if (!local_settings.value(UPDATE_NOTICE_SHOWN_SETTING, false).toBool()) {
        local_settings.setValue(UPDATE_NOTICE_SHOWN_SETTING, true);
        new update_notice_dialog(parent);
    }
}
