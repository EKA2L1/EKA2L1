#ifndef APPLISTWIDGET_H
#define APPLISTWIDGET_H

#include <QListWidget>

namespace eka2l1 {
    class applist_server;
    class fbs_server;

    struct apa_app_registry;
}

class applist_widget_item: public QListWidgetItem {
public:
    int registry_index_;

    applist_widget_item(const QIcon &icon, const QString &name, int registry_index, QListWidget *listview);
};

class applist_widget : public QListWidget
{
private:
    eka2l1::applist_server *lister_;
    eka2l1::fbs_server *fbss_;

    void add_registeration_item(eka2l1::apa_app_registry &reg, const int index);
    eka2l1::apa_app_registry *get_registry_from_widget_item(QListWidgetItem *item);

public:
    explicit applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss);
    bool launch_from_widget_item(QListWidgetItem *item);
};

#endif // APPLISTWIDGET_H
