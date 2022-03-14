#include <qt/symbian_input_dialog.h>
#include "ui_symbian_input_dialog.h"

symbian_input_dialog::symbian_input_dialog(QWidget *parent) :
    QDialog(parent),
    max_length_(0x7FFFFFFF),
    ui(new Ui::SymbianInputDialog) {
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);

    connect(ui->plainTextEdit, &QPlainTextEdit::textChanged, this, &symbian_input_dialog::event_content_changed);
    connect(ui->submitButton, &QPushButton::pressed, this, &symbian_input_dialog::event_submit_pressed);
    connect(this, &QDialog::finished, this, &symbian_input_dialog::event_finished);
}

symbian_input_dialog::~symbian_input_dialog() {
    delete ui;
}

void symbian_input_dialog::open_for_input(const QString &initial_text, const int new_max_length) {
    ui->plainTextEdit->setPlainText(initial_text);
    max_length_ = new_max_length;

    show();
}

void symbian_input_dialog::event_submit_pressed() {
    emit finished_input(ui->plainTextEdit->toPlainText(), false);
}

void symbian_input_dialog::event_finished(int result) {
    if (result == QDialog::Rejected) {
        emit finished_input(ui->plainTextEdit->toPlainText(), true);
    }
}

void symbian_input_dialog::event_content_changed() {
    // Code grabbed from forum! (Mar4eli)
    // https://forum.qt.io/topic/23725/solved-limit-number-of-characters-in-qtextedit/3
    if (ui->plainTextEdit->toPlainText().length() > max_length_) {
        int diff = ui->plainTextEdit->toPlainText().length() - max_length_; //m_maxTextEditLength - just an integer
        QString newStr = ui->plainTextEdit->toPlainText();
        newStr.chop(diff);
        ui->plainTextEdit->setPlainText(newStr);

        // Move the cursor to the end
        QTextCursor cursor(ui->plainTextEdit->textCursor());
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        ui->plainTextEdit->setTextCursor(cursor);
    }
}

QString symbian_input_dialog::get_current_text() const {
    return ui->plainTextEdit->toPlainText();
}