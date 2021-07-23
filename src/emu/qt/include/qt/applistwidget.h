#ifndef APPLISTWIDGET_H
#define APPLISTWIDGET_H

#include <QListWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QGridLayout>

namespace eka2l1 {
    class applist_server;
    class fbs_server;
    class io_system;

    struct apa_app_registry;
}

class applist_widget_item: public QListWidgetItem {
public:
    int registry_index_;

    applist_widget_item(const QIcon &icon, const QString &name, int registry_index, QListWidget *listview);
};

class applist_search_bar: public QWidget {
    Q_OBJECT;

private:
    QLabel *search_label_;
    QLineEdit *search_line_edit_;

    QHBoxLayout *search_layout_;

private slots:
    void on_search_bar_content_changed(QString content);

signals:
    void new_search(QString content);

public:
    applist_search_bar(QWidget *parent = nullptr);
    ~applist_search_bar();
};

class applist_widget : public QWidget
{
    Q_OBJECT;

public:
    applist_search_bar *search_bar_;
    QListWidget *list_widget_;
    QGridLayout *layout_;

    eka2l1::applist_server *lister_;
    eka2l1::fbs_server *fbss_;
    eka2l1::io_system *io_;

    void add_registeration_item(eka2l1::apa_app_registry &reg, const int index);
    eka2l1::apa_app_registry *get_registry_from_widget_item(applist_widget_item *item);

    void hide_all();
    void show_all();
    void on_search_content_changed(QString content);

private slots:
    void on_list_widget_item_clicked(QListWidgetItem *item);

signals:
    void app_launch(applist_widget_item *item);

public:
    explicit applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss, eka2l1::io_system *io,
                            const bool ngage_mode = false);
    ~applist_widget();

    bool launch_from_widget_item(applist_widget_item *item);
    void reload_whole_list();
};

#endif // APPLISTWIDGET_H
