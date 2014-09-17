// Copyright (C) 2014 Sacha Refshauge

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 3.0 for more details.

// A copy of the GPL 3.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official GIT repository and contact information can be found at
// http://github.com/xsacha/Sachesi

#include <QApplication>
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQmlApplicationEngine>

#include "mainnet.h"
#ifndef BLACKBERRY
#include "installer.h"
#include "backupinfo.h"
#endif
#ifdef BOOTLOADER_ACCESS
#include "boot.h"
#endif

// *** Download Manager Plan
// * Refactor the currentDownload class
// * Save to a temporary file (eg. "." + filename)
// * Don't delete files when finished.
// * When switching files, check if file exists. If OS/Radio, ask server for expected filesize (maybe do this when verifying Links actually!).
// * If filesize > 0, add the size_in_bytes to progress. Then try to resume the download.
// * Send header of "Range", "bytes=size_in_bytes-"
// * Test / Push
// * Now try to make it threaded.

// TODO: Make extraction handle decent % tracking for QNX FS
// TODO: Check and improve the USB Loader (Boot).
// TODO: Use CircleProgress in every progress (Extract) section. Pass a class to QML that contains file count, current and total progress
// TODO: Probably use a Rectangle for CircleProgress container with option to pull out (reparent to Window). This solves mobile incompatibility. Also put each progress in a separate qml.
// TODO: Don't use hardcoded width/height values for UI. Now that we use native UI elements and font sizes, this will break very bad
// TODO: Check PolicyRestrictions:
// Personal: policy_block_backup_and_restore, policy_backup_and_restore
// Enterprise: policy_disable_devmode, policy_log_submission, policy_block_computer_access_to_device

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_X11InitThreads, true);
#ifdef BLACKBERRY
    QApplication::setAttribute(Qt::AA_Use96Dpi, true);
#endif

    QApplication app(argc, argv);
    app.setOrganizationName("Qtness");
    app.setOrganizationDomain("qtness.com");
    app.setApplicationName("Sachesi");
    app.setApplicationVersion("1.8.3");
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    QQmlApplicationEngine engine;
    QQmlContext *context = engine.rootContext();

    // Send the version across. Preferably via the .pro
    context->setContextProperty("version", QVariant::fromValue(QApplication::applicationVersion()));
    // Check if we have at least Qt 5.3 available. If not, do some workarounds for bugs.
    context->setContextProperty("qt_new", QVariant::fromValue(QT_VERSION > QT_VERSION_CHECK(5, 3, 0)));
    // Check if this is a Blackberry device.
    context->setContextProperty("blackberry", QVariant::fromValue(
                                #ifdef BLACKBERRY
                                    1
                                #else
                                    0
                                #endif
                                    ));
#ifdef BLACKBERRY
    MainNet p;
#else
    InstallNet i;
    context->setContextProperty("i", &i);
    MainNet p(&i);
#endif

    if (!checkCurPath())
        return 0;

#ifdef BOOTLOADER_ACCESS
    Boot b;
    context->setContextProperty("b", &b);

    QObject::connect(&b, SIGNAL(started()), &b, SLOT(search()));
    QObject::connect(&b, SIGNAL(finished()), &b, SLOT(exit()));
    QObject::connect(&i, SIGNAL(newPassword(QString)), &b, SLOT(newPassword(QString)));
    b.start();
#endif

    context->setContextProperty("p", &p);
    context->setContextProperty("download", p.currentDownload);
#ifndef BLACKBERRY
    qmlRegisterType<BackupInfo>("BackupTools", 1, 0, "BackupInfo");
#endif
    qmlRegisterType<Apps>("AppLibrary", 1, 0, "Apps");

#ifdef _WIN32
    engine.addImportPath("qrc:/qml/");
#endif
    QScopedPointer<QQmlComponent> comp(new QQmlComponent(&engine));
    comp->loadUrl(QUrl("qrc:/qml/generic/Title.qml"));
    if (comp->status() == QQmlComponent::Error) {
        QMessageBox::information(NULL, "Error", qPrintable(comp->errorString()), QMessageBox::Ok);
        return 0;
    }
    QQuickWindow *window = qobject_cast<QQuickWindow *>(comp->create());
    window->show();

    int ret = app.exec();
#ifdef BOOTLOADER_ACCESS
    b.quit();
    b.wait(1000);
#endif
    delete window;
    return ret;
}
