#include <qt/state.h>
#include <qt/thread.h>
#include <qt/utils.h>

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QSettings>

#include <memory>
#include <common/log.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("EKA2L1");
    QCoreApplication::setApplicationName("EKA2L1");

    QTranslator translator;
    QSettings settings;

    QVariant language_variant = settings.value(LANGUAGE_SETTING_NAME);
    bool lang_loaded = false;

    if (language_variant.isValid()) {
        const QString base_name = "eka2l1_" + language_variant.toString();
        if (translator.load(":/languages/" + base_name)) {
            a.installTranslator(&translator);
            lang_loaded = true;
        }  
    }

    if (!lang_loaded) {
        const QStringList ui_languages = QLocale::system().uiLanguages();
        for (const QString &locale : ui_languages) {
            const QString base_name = "eka2l1_" + QLocale(locale).name();
            if (translator.load(":/languages/" + base_name)) {
                a.installTranslator(&translator);
                settings.setValue(LANGUAGE_SETTING_NAME, translator.language());

                break;
            }
        }
    }

    eka2l1::desktop::emulator emulator_state;
    return eka2l1::desktop::emulator_entry(a, emulator_state, argc, const_cast<const char**>(argv));
}
