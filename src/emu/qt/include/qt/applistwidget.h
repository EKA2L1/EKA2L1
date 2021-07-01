#ifndef APPLISTWIDGET_H
#define APPLISTWIDGET_H

#include <QListWidget>

namespace eka2l1 {
    class applist_server;
    class fbs_server;

    struct apa_app_registry;
}

class applist_widget : public QListWidget
{
private:
    eka2l1::applist_server *lister_;
    eka2l1::fbs_server *fbss_;

    void add_registeration_item(eka2l1::apa_app_registry &reg);

public:
    explicit applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss);
};

#endif // APPLISTWIDGET_H
