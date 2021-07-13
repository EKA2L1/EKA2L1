#include <qt/state.h>
#include <qt/thread.h>

#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("EKA2L1");
    QCoreApplication::setApplicationName("EKA2L1");

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "eka2l1_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    eka2l1::desktop::emulator emulator_state;
    return eka2l1::desktop::emulator_entry(a, emulator_state, argc, const_cast<const char**>(argv));
}
