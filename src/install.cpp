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

#include "install.h"
#include <QListView>
#include <QTreeView>
#include <QAbstractListModel>
#include <QDebug>
#include <QMessageBox>
#include <QNetworkInterface>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#define encodedQuery query(QUrl::FullyEncoded).toUtf8
#else
#define QUrlQuery QUrl
#endif

#ifdef BLACKBERRY
#define SAVE_DIR settings.value("installDir", "/accounts/1000/shared/misc/Sachesi/").toString()
#include <bb/cascades/pickers/FilePicker>
#else
#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#define SAVE_DIR settings.value("installDir", QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first()).toString()
#else
#include <QDesktopServices>
#define SAVE_DIR settings.value("installDir", QDesktopServices::storageLocation(QDesktopServices::DesktopLocation)).toString()
#endif
#endif

InstallNet::InstallNet( QObject* parent) : QObject(parent),
    manager(NULL), reply(NULL), cookieJar(NULL),
    _knownOS(""), _knownBattery(-1), _knownPIN(""),
    _wrongPass(false), _wrongPassBlock(false), _state(0),
    _dgProgress(-1), _curDGProgress(-1), _completed(false),
    _installing(false), _restoring(false), _backing(false),
    _hadPassword(true), currentBackupZip(NULL), _zipFile(NULL)
{
#ifdef WIN32
    WSAStartup(MAKEWORD(2,0), &wsadata);
#endif
    connectTimer = new QTimer();
    connectTimer->setInterval(3000);
    connectTimer->start();
    connect(connectTimer, SIGNAL(timeout()), this, SLOT(login()));
    QSettings settings("Qtness","Sachesi");
    setIp(settings.value("ip","169.254.0.1").toString());
    connect(&_back, SIGNAL(curModeChanged()), this, SIGNAL(backStatusChanged()));
    connect(&_back, SIGNAL(curSizeChanged()), this, SIGNAL(backCurProgressChanged()));
    connect(&_back, SIGNAL(numMethodsChanged()), this, SIGNAL(backMethodsChanged()));

    QByteArray hashedPass = settings.value("pass", "").toByteArray();

    if (hashedPass.isEmpty()) {
        _password = "";
    } else {
        int passSize = QByteArray::fromBase64(hashedPass.left(4))[0];
        hashedPass = QByteArray::fromBase64(hashedPass.mid(4));

        char * decPass = new char[passSize+1];
        for (int i = 0; i < passSize; i++) {
            decPass[i] = hashedPass[i] ^ ((0x40 + 5 * i - passSize) % 127);
        }
        decPass[passSize] = 0;
        _password = QString(decPass);
        delete decPass;
    }
    emit newPassword(_password);
    login();
}
InstallNet::~InstallNet()
{
#ifdef Q_WS_WIN32
    WSACleanup();
#endif
}

void InstallNet::setData(QString page, QString contentType) {
    request.setUrl(QUrl("https://" + ip() + "/cgi-bin/" + page));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/" + contentType);
}

void InstallNet::listApps()
{
    scanProps();
}

void InstallNet::scanProps()
{
    if (!completed())
        requestLogin();
    else
    {
        setData("dynamicProperties.cgi", "x-www-form-urlencoded");
        QUrlQuery postData;
        postData.addQueryItem("Get Dynamic Properties","Get Dynamic Properties");

        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}
bool InstallNet::selectInstallFolder()
{
    QSettings settings("Qtness", "Sachesi");
// TODO
#ifdef BLACKBERRY
	bb::cascades::pickers::FilePicker* filePicker = new bb::cascades::pickers::FilePicker();
	filePicker->setFilter(QStringList() << "*.bar");
	filePicker->setTitle("Install Folder of Bar Files");
	filePicker->setMode(bb::cascades::pickers::FilePickerMode::PickerMultiple);
	filePicker->open();
//	QObject::connect(filePicker, SIGNAL(fileSelected(const QStringList&)), this, SLOT(selectInstallFolderSlot(const QStringList&)));
	return true;
#else
    QFileDialog finder;
    finder.setFileMode(QFileDialog::Directory);
    finder.setDirectory(SAVE_DIR);
    finder.setWindowTitle("Install Folder of Bar Files");
    finder.setNameFilter("Blackberry Installable (*.bar)");
    QListView *l = finder.findChild<QListView*>("listView");
    if (l)
        l->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QTreeView *t = finder.findChild<QTreeView*>();
    if (t)
        t->setSelectionMode(QAbstractItemView::ExtendedSelection);
    if (finder.exec()) {
        QFileInfo fileInfo(finder.selectedFiles().first());
        settings.setValue("installDir", fileInfo.absolutePath());
        install(finder.selectedFiles());
        return true;
    }
#endif
    return false;
}

bool InstallNet::selectInstall()
{
    QSettings settings("Qtness", "Sachesi");
    QFileDialog finder;
    finder.setDirectory(SAVE_DIR);
    finder.setWindowTitle("Install Bar Files");
    finder.setNameFilter("Blackberry Installable (*.bar)");
    QListView *l = finder.findChild<QListView*>("listView");
    if (l)
        l->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QTreeView *t = finder.findChild<QTreeView*>();
    if (t)
        t->setSelectionMode(QAbstractItemView::ExtendedSelection);
    if (finder.exec()) {
        QFileInfo fileInfo(finder.selectedFiles().first());
        settings.setValue("installDir", fileInfo.absolutePath());
        install(finder.selectedFiles());
        return true;
    }
    return false;
}

void InstallNet::install(QStringList files)
{
    if (files.isEmpty())
        return;
    _fileNames = QStringList();
    setFirmwareUpdate(false);
    foreach(QString _fileName, files)
    {
        if (_fileName.startsWith("file:///"))
            _fileName.remove(0,8);
        if (QFileInfo(_fileName).isDir())
        {
            QStringList suffixOnly = QDir(_fileName).entryList(QStringList("*.bar"));
            foreach (QString suffix, suffixOnly)
            {
                if (suffix.contains("_sfi"))
                {
                    setNewLine("<b>Installing OS: " + suffix.split("-", QString::SkipEmptyParts).at(1)+"</b><br>");
                    setFirmwareUpdate(true);
                }
                if (suffix.contains(".wtr") || suffix.contains("omadm-") || suffix.startsWith("m5730") || suffix.startsWith("qc8960-"))
                {
                    setNewLine("<b>Installing Radio: " + suffix.split("-", QString::SkipEmptyParts).at(1)+"</b><br>");
                    setFirmwareUpdate(true);
                }
                _fileNames.append(_fileName + "/" + suffix);
            }
        } else if (_fileName.endsWith(".bar"))
        {
            QString suffix = _fileName.split("/").last();
            if (suffix.contains("_sfi"))
            {
                setNewLine("<b>Installing OS: " + suffix.split("-", QString::SkipEmptyParts).at(1)+"</b><br>");
                setFirmwareUpdate(true);
            }
            if (suffix.contains(".wtr") || suffix.contains("omadm-") || suffix.startsWith("m5730") || suffix.startsWith("qc8960-"))
            {
                setNewLine("<b>Installing Radio: " + suffix.split("-", QString::SkipEmptyParts).at(1)+"</b><br>");
                setFirmwareUpdate(true);
            }
            _fileNames.append(_fileName);
        }
    }
    if (_fileNames.isEmpty())
        return;
    install();
}

void InstallNet::install()
{
    setInstalling(true);
    if (!completed())
        requestLogin();
    else
    {
        QUrlQuery postData;
        int nfilesize = 0;
        _downgradePos = 0;
        _downgradeInfo = _fileNames;
        emit dgPosChanged();
        emit dgMaxPosChanged();
        for (int i = 0; i < _downgradeInfo.count(); i++)
        {
            nfilesize += QFile(_downgradeInfo.at(i)).size();
        }
        QString filesize;
        filesize.setNum(nfilesize);
        setData("update.cgi", "x-www-form-urlencoded");
        postData.addQueryItem("mode", firmwareUpdate() ? "os" : "bar");
        postData.addQueryItem("size", filesize);

        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}

void InstallNet::uninstall(QStringList packageids)
{
    if (packageids.isEmpty())
        return;
    setInstalling(true);
    if (!completed())
        requestLogin();
    else
    {
        QUrlQuery postData;
        _downgradePos = 0;
        _downgradeInfo = packageids;
        emit dgPosChanged();
        emit dgMaxPosChanged();
        setData("update.cgi", "x-www-form-urlencoded");
        postData.addQueryItem("mode", "app");
        postData.addQueryItem("size", "0");

        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}

bool InstallNet::uninstallMarked()
{
    QStringList marked;
    for (int i = 0; i < _appList.count(); i++) {
        if (static_cast< QList<Apps *> *>(appList().data)->at(i)->isMarked()) {
            marked.append(static_cast< QList<Apps *> *>(appList().data)->at(i)->packageId());
            static_cast< QList<Apps *> *>(appList().data)->at(i)->setIsMarked(false);
            static_cast< QList<Apps *> *>(appList().data)->at(i)->setType("");
        }
    }
    if (marked.isEmpty())
        return false;
    uninstall(marked);
    return true;
}

void InstallNet::selectBackup(int options)
{
    QFileDialog finder;
    finder.setAcceptMode(QFileDialog::AcceptSave);
    finder.setDirectory(QDir::homePath());
#ifdef _WIN32
    QFile linkSettings(QDir::homePath()+"/AppData/Roaming/Research In Motion/BlackBerry 10 Desktop/Settings.config");
    linkSettings.open(QIODevice::WriteOnly);
    QXmlStreamReader xml(&linkSettings);
    for (xml.readNext(); !xml.atEnd(); xml.readNext()) {
        if (xml.isStartElement()) {
            if (xml.name() == "Configuration" && xml.attributes().count() > 1 && xml.attributes().at(0).value() == "BackupFolderLocation") {
                finder.setDirectory(xml.attributes().at(1).value().toString());
            }
        }
    }
    linkSettings.close();
#endif
    finder.setWindowTitle("Create Backup");
    finder.setNameFilter("Blackberry Backup (*.bbb)");
    if (finder.exec())
        _fileNames = finder.selectedFiles();
    if (_fileNames.isEmpty())
        return;
    if (!_fileNames.first().endsWith(".bbb"))
        _fileNames.first().append(".bbb");
    _back.setMode(options);
    _back.setCurMode(0);
    backup();
}

void InstallNet::backup()
{
    setBacking(true);
    if (!completed())
        requestLogin();
    else {
        currentBackupZip = new QuaZip(_fileNames.first());
        currentBackupZip->open(QuaZip::mdCreate);
        if (!currentBackupZip->isOpen()) {
            QMessageBox::critical(NULL, "Error", "Unable to write backup. Please ensure you have permission to write to " + _fileNames.first());
            delete currentBackupZip;
            setBacking(false);
            return;
        }

        QuaZipFile* manifest;
        manifest = new QuaZipFile(currentBackupZip);
        manifest->open(QIODevice::WriteOnly, QuaZipNewInfo("Manifest.xml"), NULL, 0, 8);
        QString manifestXML = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<BlackBerry_Backup><Version>3.0</Version><Client platform=\"SachESI\" osversion=\"Microsoft Windows NT 6.1.7601 Service Pack 1\" dtmversion=\"2.0.0.0\"/>"
                "<SourceDevice pin=\"" + _knownPIN + "\"><Platform type=\"QNX\" version=\"10.0.0.0\"/></SourceDevice>"
                "<QnxOSDevice><Archives>";
        foreach(BackupCategory* cat, _back.categories) {
            if (_back.modeString().contains(cat->id))
                manifestXML.append("<Archive id=\"" + cat->id + "\" name=\"" + cat->name + "\" count=\"" + cat->count + "\" bytesize=\"" + cat->bytesize + "\" perimetertype=" + cat->perimetertype + "\"/>");
        }
        manifestXML.append("</Archives></QnxOSDevice></BlackBerry_Backup>");
        manifest->write(manifestXML.toStdString().c_str());
        manifest->close();
        delete manifest;

        setData("backup.cgi", "x-www-form-urlencoded");
        QUrlQuery postData;
        postData.addQueryItem("action", "backup");
        postData.addQueryItem("mode", _back.modeString());
        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}

void InstallNet::backupQuery() {
    if (!completed())
        requestLogin();
    else {
        QUrlQuery postData;
        setData("backup.cgi", "x-www-form-urlencoded");
        postData.addQueryItem("action", "backup");
        postData.addQueryItem("query", "list");

        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}

void InstallNet::selectRestore(int options)
{
    QFileDialog finder;
    QDir docs(QDir::homePath());
    if (docs.exists("Documents/Blackberry/Backup"))
        finder.setDirectory(QDir::homePath() + "/Documents/Blackberry/Backup");
    else if (docs.exists("My Documents/Blackberry/Backup"))
        finder.setDirectory(QDir::homePath() + "/My Documents/Blackberry/Backup");
    finder.setWindowTitle("Restore Backup");
    finder.setNameFilter("Blackberry Backup (*.bbb)");
    if (finder.exec())
        _fileNames = finder.selectedFiles();
    if (_fileNames.isEmpty())
        return;
    if (!QFile::exists(_fileNames.first()))
        return;
    currentBackupZip = new QuaZip(_fileNames.first());
    currentBackupZip->open(QuaZip::mdUnzip);
    if (!currentBackupZip->isOpen()) {
        QMessageBox::critical(NULL, "Error", "Could not open backup file.");
        delete currentBackupZip;
        return;
    }
    for (int i = 0; i < _back.numMethods(); i++) {
        if (options & (1 << i)) {
            // We want to restore this file
            currentBackupZip->setCurrentFile(QString("Archive/" + _back.stringFromMode(i) + ".tar"));
            if (!currentBackupZip->hasCurrentFile()) {
                // But this file doesn't exist?
                options &= ~(1 << i);
            } else {
                // Set the size from this file
                QuaZipFileInfo info;
                currentBackupZip->getCurrentFileInfo(&info);
                _back.setCurMaxSize(i, info.uncompressedSize);
                qint64 startSize = (_back.maxSize() > 1) ? _back.maxSize() : 0;
                _back.setMaxSize(startSize + info.uncompressedSize);
            }
        }
    }
    if (options) {
        _back.setMode(options);
        _back.setCurMode(0);
        restore();
    }
}

void InstallNet::restore()
{
    setRestoring(true);
    if (!completed())
        requestLogin();
    else
    {
        QUrlQuery postData;
        setData("backup.cgi", "x-www-form-urlencoded");
        postData.addQueryItem("action", "restore");
        postData.addQueryItem("mode", _back.modeString());
        postData.addQueryItem("totalsize", QString::number(_back.maxSize()));

        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}

void InstallNet::wipe() {
    if (QMessageBox::critical(NULL, "Loss of data", "Are you sure you want to wipe your device?\nThis will result in a permanent loss of data.", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;
    setData("wipe.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("wipe", "wipe");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::startRTAS() {
    setData("wipe.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("wipe", "start_rtas");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::newPin(QString pin) {
    setData("wipe.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("wipe", "pin");
    postData.addQueryItem("newpin", pin.left(8));
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::resignNVRAM() {
    setData("wipe.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("wipe", "re_sign");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::factorywipe() {
    if (QMessageBox::critical(NULL, "Loss of data", "Are you sure you want to wipe your device?\nThis will result in a permanent loss of data.", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;
    setData("wipe.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("wipe", "wipe");
    postData.addQueryItem("factorywipe", "1");
    postData.addQueryItem("nopoweroff", "1");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::reboot() {
    setData("reset.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("reset", "true");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::getPIN() {
    setData("wipe.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("wipe", "getpin");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::dumpLogs() {
    setData("support.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("facility", "dumplog");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::setActionProperty(QString name, QString value) {
    setData("dynamicProperties.cgi", "x-www-form-urlencoded");
    QUrlQuery postData;
    postData.addQueryItem("action", "set");
    postData.addQueryItem("name", name);
    postData.addQueryItem("value", value);
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::login()
{
    if (state() || wrongPassBlock())
        return;

    QStringList ips;

    int flags = QNetworkInterface::IsUp | QNetworkInterface::IsRunning | QNetworkInterface::CanBroadcast | QNetworkInterface::CanMulticast;
    foreach(QNetworkInterface inter, QNetworkInterface::allInterfaces())
    {
#ifdef _WIN32
        // TODO: Still getting bad result?!
		if (inter.addressEntries().count() != 2)
			continue;
#endif
		if (inter.humanReadableName().startsWith("VMware"))
			continue;
        if ((inter.flags() & flags) == flags && !inter.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            foreach(QNetworkAddressEntry addr, inter.addressEntries())
            {
                if (addr.ip().protocol() == QAbstractSocket::IPv4Protocol)
				{
                    QList<quint8> addrParts;
                    foreach(QString addrString, addr.ip().toString().split('.'))
                        addrParts.append(addrString.toInt());
                    if ((addrParts.last() % 4) != 2)
                        continue;
                    if (addrParts.at(0) == 169 && addrParts.at(1) == 254)
                        ips.append(QString("169.254.%1.%2").arg(addrParts.at(2)).arg(addrParts.at(3) - 1));
                }
            }
        }
    }
    ips.removeDuplicates();
    if (ips.isEmpty())
		return;
	setIp(ips.first());

    request = QNetworkRequest();

    request.setRawHeader("User-Agent", "QNXWebClient/1.0");
    if (manager == NULL)
        manager = new SslNetworkAccessManager();
    if (cookieJar == NULL) {
        cookieJar = new QNetworkCookieJar(this);
        manager->setCookieJar(cookieJar);
    }
    foreach(QString ip_addr, ips) {
        request.setUrl(QUrl("http://"+ip_addr+"/cgi-bin/discovery.cgi"));
        reply = manager->get(request);
    }

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(discoveryReply()));
}

void InstallNet::discoveryReply() {
    if (state() && reply->url().host() != ip())
        return;
    QByteArray data = reply->readAll();
    //qDebug() << "Message:\n" << QString(data).simplified().left(3000);
    QXmlStreamReader xml(data);
    xml.readNextStartElement(); // RimTabletResponse
    xml.readNextStartElement();
    if (xml.name() == "DeviceCharacteristics") {
        // Valid device
        if (!state()) {
            setIp(reply->url().host());
            setState(1);
        }
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement())
            {
                if (xml.name() == "BbPin") {
                    setKnownPIN(QString::number(xml.readElementText().toInt(),16).toUpper());
                } else if (xml.name() == "SystemMachine") {
                    setKnownName(xml.readElementText());
                    setNewLine(QString("Connected to %1 at %2.<br>").arg(knownName()).arg(ip()));
                } else if (xml.name() == "OsType") {
                    if (xml.readElementText() == "BlackBerry PlayBook OS") {
                        setKnownName("Playbook_QNX6.6.0");
                        setNewLine(QString("Connected to Playbook at %1.<br>").arg(ip()));
                    }
                } else if (xml.name() == "PlatformVersion") {
                    setKnownOS(xml.readElementText());
                } else if (xml.name() == "Power") {
                    setKnownBattery(xml.readElementText().toInt());
                } else if (xml.name() == "ModelName") {
                    setKnownHW(xml.readElementText());
                }
            }
        }
        request.setUrl(QUrl("https://"+ip()+":443/cgi-bin/login.cgi?request_version=1"));
        reply = manager->get(request);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
}

void InstallNet::requestLogin() {
    request.setUrl(QUrl("https://"+ip()+":443/cgi-bin/login.cgi?request_version=1"));
    reply = manager->get(request);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::installProgress(qint64 pread, qint64)
{
    _dlBytes = 50*pread;
    setDGProgress(qMin((int)50, (int)(_dlBytes / _dlTotal)));
}

void InstallNet::backupProgress(qint64 pread, qint64)
{
    _back.setCurSize(100*pread);
    _back.setProgress(qMin((int)100, (int)(_back.curSize() / _back.curMaxSize())));
}

void InstallNet::backupFileReady()
{
    if (reply->bytesAvailable() > 16384) {
        while (!reply->atEnd())
            _zipFile->write(reply->read(16384));
    }
}

void InstallNet::restoreProgress(qint64 pwrite, qint64) {
    _back.setCurSize(100*pwrite);
    _back.setProgress(qMin((int)100, (int)(_back.curSize() / _back.curMaxSize())));
}

void InstallNet::backupFileFinish()
{
    _zipFile->write(reply->readAll());
    _zipFile->close();
    delete _zipFile;

    _back.setCurMode(1);

    QUrlQuery postData;
    postData.addQueryItem("action", "backup");
    if (_back.curMode() != "complete") {
        _back.setProgress(0);
        postData.addQueryItem("type", _back.curMode());
    }
    setData("backup.cgi", "x-www-form-urlencoded");
    reply = manager->post(request, postData.encodedQuery());
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::restoreReply()
{
    QByteArray data = reply->readAll();
    //for (int s = 0; s < data.size(); s+=3000) qDebug() << "Message:\n" << QString(data).simplified().mid(s, 3000);
    if (data.size() == 0) {
        if (restoring()) {
            QMessageBox::information(NULL, "Restore Error", "There was an error loading the backup file.\nThe device encountered an unrecoverable bug.\nIt is not designed to restore this backup.");
            if (_zipFile) {
                if (_zipFile->isOpen())
                    _zipFile->close();
                delete _zipFile;
            }
            if (currentBackupZip) {
                if (currentBackupZip->isOpen())
                    currentBackupZip->close();
                delete currentBackupZip;
            }
            setRestoring(false);
        }
    }
    QUrlQuery postData;
    QString element;
    QXmlStreamReader xml(data);
    xml.readNextStartElement(); // RimTabletResponse
    xml.readNextStartElement();
    if (xml.name() == "AuthChallenge")
    { // We need to verify
        QString salt, challenge;
        int iCount = 0;
        while (xml.readNextStartElement()) {
            element = xml.readElementText();
            if (xml.name() == "Salt")
                salt = element;
            else if (xml.name() == "Challenge")
                challenge = element;
            else if (xml.name() == "ICount")
                iCount = element.toInt();
            else if (xml.name() == "ErrorDescription")
            {
                setWrongPassBlock(true);
                setCompleted(false);
                setState(false);
                return;
            }
        }
        QByteArray saltHex(salt.toLatin1());
        QByteArray challenger(challenge.toLatin1());
        QByteArray result = HashPass(challenger, QByteArray::fromHex(saltHex), iCount);

        request.setUrl(QUrl("https://"+ip()+":443/cgi-bin/login.cgi?challenge_data=" + result.toHex().toUpper() + "&request_version=1"));
        reply = manager->get(request);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
    else if (xml.name() == "Auth")
    { // We are authenticated
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement())
            {
                QString name = xml.name().toString();
                if (name == "Error")
                { //No we aren't!
                    return;
                }
                else if (name == "Status")
                {
                    if (xml.readElementText() == "Denied")
                    {
                        if (!data.contains("Attempts>0</"))
                            setWrongPass(true);
                        else {
                            setWrongPassBlock(true);
                            setCompleted(false);
                            setState(false);
                        }
                        return;
                    }
                }
            }
        }
        setCompleted(true);

        if (installing())
            install();
        else if (backing())
            backup();
        else if (restoring())
            restore();
        /*else if (_hadPassword)
            scanProps();*/
        //backupQuery();
    }
    else if (xml.name() == "DynamicProperties")
    {
        _appList.clear();
        _appRemList.clear();
        for (xml.readNext(); !xml.atEnd(); xml.readNext()) {
            if (xml.isStartElement())
            {
                QString name = xml.name().toString();
                if (name == "ErrorDescription")
                {
                    QMessageBox::information(NULL, "Error", xml.readElementText(), QMessageBox::Ok);
                } else if (name == "Application") {
                    Apps* newApp = new Apps();
                    while(!xml.atEnd())
                    {
                        xml.readNext();
                        if (xml.isStartElement()) {
                            if (xml.name() == "Name")
                            {
                                QString longName = xml.readElementText();
                                newApp->setName(longName);
                                if (longName.contains(".test"))
                                    longName = longName.split(".test").first();
                                else
                                    longName = longName.split(".gY").first();
                                QStringList newLineParts = longName.split('.');
                                if (newLineParts.last().isEmpty())
                                    newLineParts.removeLast();
                                longName = "";
                                for (int i = 0; i < newLineParts.size() - 1; i++)
                                    longName += newLineParts.at(i) + ".";
                                longName += "<b>" + newLineParts.last() + "</b>";
                                newApp->setFriendlyName(longName);
                            }
                            else if (xml.name() == "Type")
                                newApp->setType(xml.readElementText());
                            else if (xml.name() == "PackageId")
                                newApp->setPackageId(xml.readElementText());
                            else if (xml.name() == "PackageVersionId")
                                newApp->setVersionId(xml.readElementText());
                            else if (xml.name() == "PackageVersion")
                                newApp->setVersion(xml.readElementText());
                            else if (xml.name() == "Fingerprint")
                                newApp->setChecksum(xml.readElementText());
                        } else if (xml.isEndElement() && xml.name() == "Application")
                            break;
                    }
                    if (newApp->type() != "")
                        _appList.append(newApp);
                    else
                        _appRemList.append(newApp);
                }
                else if (name == "PlatformVersion")
                    setKnownOS(xml.readElementText());
                else if (name == "BatteryLevel")
                    setKnownBattery(xml.readElementText().toInt());
                /* // DEPRECATED by discovery.cgi
                else if (name == "DeviceName")
                {
                    // name that the user calls their phone
                }
                else if (name == "HardwareID")
                    setKnownHW(xml.readElementText());*/
            }
        }
        // No need to show this list anymore, I think
        /*
        QString appInfoList = "<b>Currently Installed Applications:</b><br>";
        foreach (Apps* apps, _appList)
        {
            appInfoList.append("  " + apps->friendlyName() + "<br>");
        }
        setNewLine(appInfoList);*/
        emit appListChanged();
    }
    else if (xml.name() == "RTASChallenge") {
        QFile rtasData("rtasdata.txt");
        rtasData.open(QIODevice::WriteOnly | QIODevice::Text);
        rtasData.write(QByteArray("Use these values for RLT:\n\n"));
        for (xml.readNext(); !xml.atEnd(); xml.readNext()) {
            if (xml.isStartElement())
            {
                if (xml.name() == "Challenge")
                    rtasData.write(QString("Challenge: "+xml.readElementText()+"\n").toLocal8Bit());
                else if (xml.name() == "ProcessorInfo")
                    rtasData.write(QString("Processor Info: "+xml.readElementText()+"\n").toLocal8Bit());
                else if (xml.name() == "ProcessorId")
                    rtasData.write(QString("Processor Id: "+xml.readElementText()+"\n").toLocal8Bit());
                else if (xml.name() == "BSN")
                    rtasData.write(QString("BSN: "+xml.readElementText()+"\n").toLocal8Bit());
                else if (xml.name() == "IMEI")
                    rtasData.write(QString("IMEI: "+xml.readElementText()+"\n\n").toLocal8Bit());
                else if (xml.name() == "Log")
                    rtasData.write(QString(xml.readElementText()+"\n").toLocal8Bit());
            }
        }
        rtasData.close();
        setCompleted(false);
        setState(false);
        QMessageBox::information(NULL, "Success", "RTAS has been started.\nSachesi will now terminate its connection.", QMessageBox::Ok);
        QProcess wordpad;
#ifdef _WIN32
        wordpad.startDetached("explorer rtasdata.txt");
#elif defined(__APPLE__)
        wordpad.startDetached("open rtasdata.txt");
#else
        wordpad.startDetached("xdg-open rtasdata.txt");
#endif
    }
    else if (xml.name() == "DevicePIN") {
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement())
            {
                if (xml.name() == "PIN")
                    _knownPIN = xml.readElementText().split('X').last();
            }
        }
    }
    else if (xml.name() == "Wipe") {
        for (xml.readNext(); !xml.atEnd(); xml.readNext()) {
            if (xml.isStartElement())
            {
                if (xml.name() == "ErrorDescription") {
                    QMessageBox::critical(NULL, "Error", xml.readElementText(), QMessageBox::Ok);
                }
            }
        }
    }
    else if (xml.name() == "re_pin" || xml.name() == "re_sign" || xml.name() == "start_rtas") {
        for (xml.readNext(); !xml.atEnd(); xml.readNext()) {
            if (xml.isStartElement())
            {
                if (xml.name() == "Log") {
                    QMessageBox::critical(NULL, "Error", xml.readElementText(), QMessageBox::Ok);
                }
            }
        }
    }
    else if (xml.name() == "DeleteRequest")
    {
        postData.addQueryItem("type", "bar");
        postData.addQueryItem("packageid", _downgradeInfo.at(_downgradePos));
        setData("update.cgi", "x-www-form-urlencoded");
        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
    else if (xml.name() == "DeleteProgress")
    {
        _downgradePos++;
        emit dgPosChanged();
        postData.addQueryItem("type", "bar");
        // Send another packageid if more to delete or update.
        if (_downgradePos == _downgradeInfo.count())
            postData.addQueryItem("status", "success");
        else
            postData.addQueryItem("packageid", _downgradeInfo.at(_downgradePos));
        setData("update.cgi", "x-www-form-urlencoded");
        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
    else if (xml.name() == "UpdateStart")
    {
        if (_downgradeInfo.at(_downgradePos).endsWith(".bar")) {
            compressedFile = new QFile(_downgradeInfo.at(_downgradePos));
            compressedFile->open(QIODevice::ReadOnly);
            _dlBytes = 0;
            _dlTotal = compressedFile->size();

            QString literal_name = compressedFile->fileName().split('/').last();
            QStringList fileParts = literal_name.split('-',QString::SkipEmptyParts);
            if (literal_name.contains("_sfi"))
                setCurrentInstallName("Sending " + fileParts.at(1) + " Core OS");
            else
                setCurrentInstallName("Sending " + fileParts.at(0));

            if (literal_name.contains(".wtr") || literal_name.contains("omadm-") || literal_name.startsWith("m5730") || literal_name.startsWith("qc8960-"))
                setData("update.cgi?type=radio", "octet-stream");
            else
                setData("update.cgi?type=bar", "octet-stream");

            reply = manager->post(request, compressedFile);
            compressedFile->setParent(reply);
            connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                    this, SLOT(restoreError(QNetworkReply::NetworkError)));
            connect(reply, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(installProgress(qint64,qint64)));
            connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
        }
        else {
            postData.addQueryItem("type", "bar");
            postData.addQueryItem("packageid", _downgradeInfo.at(_downgradePos));
            setData("update.cgi", "x-www-form-urlencoded");
            reply = manager->post(request, postData.encodedQuery());
            connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                    this, SLOT(restoreError(QNetworkReply::NetworkError)));
            connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
        }
    }
    else if (xml.name() == "UpdateSend")
    {
        for (xml.readNext(); !xml.atEnd(); xml.readNext()) {
            if (xml.isStartElement()) {
                if (xml.name() == "Status") {
                    if (xml.readElementText() == "Error") {
                        return;
                    }
                } else if (xml.name() == "ErrorDescription") {
                    QMessageBox::critical(NULL, "Error", xml.readElementText(), QMessageBox::Ok);
                }
            }
        }
        setData("update.cgi", "x-www-form-urlencoded");
        reply = manager->post(request, postData.encodedQuery());

        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
    else if (xml.name() == "UpdateProgress")
    {
        bool inProgress = false;
        while (xml.readNextStartElement()) {
            element = xml.readElementText();
            if (xml.name() == "Status")
            {
                if (element == "InProgress")
                    inProgress = true;
                else if (element == "Success")
                {
                    inProgress = false;
                    setCurrentInstallName(currentInstallName() + " Sent.");
                    _downgradePos++;
                    emit dgPosChanged();
                    if (_downgradePos == _downgradeInfo.count())
                    {
                        postData.addQueryItem("status","success");
                        setData("update.cgi", "x-www-form-urlencoded");
                        reply = manager->post(request, postData.encodedQuery());
                        compressedFile->close();
                    }
                    else
                    {
                        compressedFile->close();
                        compressedFile = new QFile(_downgradeInfo.at(_downgradePos));
                        compressedFile->open(QIODevice::ReadOnly);
                        _dlBytes = 0;
                        _dlTotal = compressedFile->size();
                        QString literal_name = compressedFile->fileName().split('/').last();
                        QStringList fileParts = literal_name.split('-',QString::SkipEmptyParts);
                        if (literal_name.contains("_sfi"))
                            setCurrentInstallName("Sending " + fileParts.at(1) + " Core OS");
                        else
                            setCurrentInstallName("Sending " + fileParts.at(0));

                        if (literal_name.contains(".wtr") || literal_name.contains("omadm-") || literal_name.startsWith("m5730") || literal_name.startsWith("qc8960-"))
                            setData("update.cgi?type=radio", "octet-stream");
                        else
                            setData("update.cgi?type=bar", "octet-stream");
                        request.setHeader(QNetworkRequest::ContentLengthHeader, compressedFile->size());
                        request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
                        reply = manager->post(request, compressedFile);
                        compressedFile->setParent(reply);
                        connect(reply, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(installProgress(qint64,qint64)));
                    }
                }
                else if (element == "Error")
                {
                    QString errorText = QString(data).split("ErrorDescription>").at(1);
                    errorText.chop(2);
                    setNewLine("<br>While sending: " + _downgradeInfo.at(_downgradePos).split("/").last()+"<br>");
                    setNewLine("&nbsp;&nbsp;Error: " + errorText +"<br>");
                }
            }
            else if (xml.name() == "Progress")
            {
                if ((_downgradePos == (_downgradeInfo.count() - 1)))
                    setDGProgress(50 + element.toInt()/2);
                else
                    setDGProgress(inProgress ? (50 + element.toInt()/2) : 0);
                bool resend = false;
                if (knownOS().startsWith("2."))
                    resend = !data.contains("100");
                else
                    resend = inProgress;
                if (resend)  // No 100%!
                {
                    setData("update.cgi", "x-www-form-urlencoded");
                    reply = manager->post(request, postData.encodedQuery());
                }
            }
        }
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
    else if (xml.name() == "Backup" || xml.name() == "BackupGet") {
        for (xml.readNext(); !xml.atEnd(); xml.readNext())
        {
            if (xml.isStartElement()) {
                if (xml.name() == "Status") {
                    if (xml.readElementText() == "Error") {
                        if (backing()) {
                            setBacking(false);
                            if (currentBackupZip != NULL) {
                                currentBackupZip->close();
                                delete currentBackupZip;
                                QFile::remove(_fileNames.first());
                            }
                        } else
                            _hadPassword = false;
                    }
                } else if (xml.name() == "ErrorDescription") {
                    QMessageBox::information(NULL, "Error", xml.readElementText().remove("HTTP_COOKIE="), QMessageBox::Ok);
                }
            }
        }
    }
    else if (xml.name() == "UpdateEnd") {
        setInstalling(false);
        setNewLine("Completed Update.");
        setDGProgress(-1);
        setCurDGProgress(-1);
    }
    else if (xml.name() == "BackupCheck")
    {
        if (_back.curMode() != "complete") {
            setData("backup.cgi", "x-www-form-urlencoded");
            postData.addQueryItem("action", "backup");
            postData.addQueryItem("type", _back.curMode());
            reply = manager->post(request, postData.encodedQuery());
            _zipFile = new QuaZipFile(currentBackupZip);
            _zipFile->open(QIODevice::WriteOnly, QuaZipNewInfo("Archive/" + _back.curMode() + ".tar"), NULL, 0, 8);
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)),this, SLOT(backupProgress(qint64, qint64)));
            connect(reply, SIGNAL(readyRead()), this, SLOT(backupFileReady()));
            connect(reply, SIGNAL(finished()), this, SLOT(backupFileFinish()));

        } else {
            setData("backup.cgi", "x-www-form-urlencoded");
            postData.addQueryItem("status", "success");
            reply = manager->post(request, postData.encodedQuery());
            connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
            currentBackupZip->close();
            delete currentBackupZip;
            setBacking(false);
        }
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
    }
    else if (xml.name() == "BackupList")
    {
        _back.clearModes();
        while(!xml.atEnd() && !xml.hasError()) {
            QXmlStreamReader::TokenType token = xml.readNext();
            if(token == QXmlStreamReader::StartElement && xml.attributes().count() > 3 &&  xml.attributes().at(0).name() == "id") {
                _back.addMode(xml.attributes());
            }
        }
    }
    else if (xml.name() == "BackupStart")
    {
        if (data.contains("Error")) {
            setBacking(false);
            setRestoring(false);
            return;
        }
        setData("backup.cgi", "x-www-form-urlencoded");
        postData.addQueryItem("action", "backup");
        postData.addQueryItem("type", _back.curMode());
        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
    }
    else if (xml.name() == "BackupStartActivity")
    {
        setData("backup.cgi", "x-www-form-urlencoded");
        postData.addQueryItem("type", _back.curMode());
        reply = manager->post(request, postData.encodedQuery());
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(restoreError(QNetworkReply::NetworkError)));

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == "Status" && xml.readElementText() == "InProgress") {
                    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
                }
                if (xml.name() == "Settings") {
                    _back.setCurMaxSize(0, xml.readElementText().toLongLong());
                }
                else if (xml.name() == "Media") {
                    _back.setCurMaxSize(1, xml.readElementText().toLongLong());
                }
                else if (xml.name().startsWith("App")) {
                    _back.setCurMaxSize(2, xml.readElementText().toLongLong());
                }
                else if (xml.name() == "TotalSize")
                {
                    _back.setMaxSize(xml.readElementText().toLongLong());
                    _zipFile = new QuaZipFile(currentBackupZip);
                    _zipFile->open(QIODevice::WriteOnly, QuaZipNewInfo("Archive/" + _back.curMode() + ".tar"), NULL, 0, 8);
                    connect(reply, SIGNAL(readyRead()), this, SLOT(backupFileReady()));
                    connect(reply, SIGNAL(finished()), this, SLOT(backupFileFinish()));
                    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),this, SLOT(backupProgress(qint64, qint64)));
                }
            }
        }
    }
    else if (xml.name() == "RestoreStart")
    {
        if (data.contains("Error")) {
            setBacking(false);
            setRestoring(false);
            return;
        }
        restoreSendFile();
    }
    else if (xml.name() == "RestoreSend")
    {
        if (_zipFile) {
            if (_zipFile->isOpen())
                _zipFile->close();
            delete _zipFile;
        }
        _back.setCurMode(1);
        if (_back.curMode() == "complete") {
            setData("backup.cgi", "x-www-form-urlencoded");
            postData.addQueryItem("status", "success");
            reply = manager->post(request, postData.encodedQuery());
            connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                    this, SLOT(restoreError(QNetworkReply::NetworkError)));
            connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));

            setRestoring(false);
            currentBackupZip->close();
            delete currentBackupZip;
        } else {
            restoreSendFile();
        }
    }
}

void InstallNet::restoreSendFile() {
    currentBackupZip->setCurrentFile(QString("Archive/" + _back.curMode() + ".tar"));
    _zipFile = new QuaZipFile(currentBackupZip);
    _zipFile->open(QIODevice::ReadOnly);
    setData("backup.cgi?action=restore&type="+_back.curMode()+"&size="+_back.curMaxSize(), "octet-stream");
    request.setHeader(QNetworkRequest::ContentLengthHeader, _zipFile->size());
    request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
    reply = manager->post(request, _zipFile);
    _zipFile->setParent(reply);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(restoreError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(restoreProgress(qint64, qint64)));
    connect(reply, SIGNAL(finished()), this, SLOT(restoreReply()));
}

void InstallNet::restoreError(QNetworkReply::NetworkError error)
{
    setCompleted(false);
    setRestoring(false);
    setBacking(false);
    setInstalling(false);
    setKnownOS("");
    setKnownBattery(-1);
    setState(0);
    if (installing())
    {
        setInstalling(false);
        setDGProgress(-1);
        setCurDGProgress(-1);
        _dlBytes = 0;
        _dlTotal = 0;
    }
    else return;
    if (error == 5) // On purpose
        return;
    setNewLine("Error: " + reply->errorString());
    qDebug() << "Error: " << error;
}

void InstallNet::logadd(QString logtxt)
{
    Q_UNUSED(logtxt);
    return;
}

QByteArray InstallNet::HashPass(QByteArray challenge, QByteArray salt, int iterations)
{
    /* Create Hashed Password */
    QByteArray hashedData = QByteArray(password().toLatin1());
    int count = 0;
    bool challenger = true;
    do {
        QByteArray buf(4 + salt.length() + hashedData.length(),0);
        QDataStream buffer(&buf,QIODevice::WriteOnly);
        buffer.setByteOrder(QDataStream::LittleEndian);
        buffer << qint32(count);
        buffer.writeRawData(salt, salt.length());
        buffer.writeRawData(hashedData, hashedData.length());
        if (!count) hashedData.resize(64);
        SHA512((const unsigned char*)buf.data(), buf.length(), (unsigned char *)hashedData.data());
        if ((count == iterations - 1) && challenger)
        {
            count = -1;
            challenger = false;
            hashedData.prepend(challenge);
        }
    } while (++count < iterations);
    return hashedData;
}

void InstallNet::disconnected()
{
    setState(0);
    setCompleted(false);
    setRestoring(false);
    setBacking(false);
    setInstalling(false);
}

void InstallNet::connected()
{
    if (state())
        requestConfigure();
}

QString InstallNet::appDeltaMsg()
{
    if (_appList.count() == 0)
        return "";
    QString delta = "<currentSoftware>";
    for (int i = 0; i < _appList.count(); i++) {
        delta.append("<package id=\"" + _appList.at(i)->packageId() + "\" name=\"" + _appList.at(i)->name() + "\" type=\"" + _appList.at(i)->type() + "\"><version id=\"" + _appList.at(i)->versionId() + "\">" + _appList.at(i)->version() + "</version><checkSum type=\"SHA512\">" + _appList.at(i)->checksum() + "</checkSum></package>");
    }
    delta.append("</currentSoftware>");
    return delta;
}

void InstallNet::exportInstalled()
{
    QFile installedTxt("installed.txt");
    installedTxt.open(QIODevice::WriteOnly | QIODevice::Text);
    installedTxt.write("Installed Applications:\n");
    for (int i = 0; i < _appList.count(); i++) {
        if (_appList.at(i)->type() != "") {
            QString appLine = _appList.at(i)->friendlyName().remove("<b>").remove("</b>").leftJustified(45);
            appLine.append(_appList.at(i)->version() + "\n");
            installedTxt.write(appLine.toStdString().c_str());
        }
    }
    if (_appRemList.count()) {
        installedTxt.write("\n\nRemoved Applications:\n");
        for (int i = 0; i < _appRemList.count(); i++) {
            QString appLine = _appRemList.at(i)->friendlyName().remove("<b>").remove("</b>").leftJustified(45);
            appLine.append(_appRemList.at(i)->version() + "\n");
            installedTxt.write(appLine.toStdString().c_str());
        }
    }
    installedTxt.close();

    QProcess wordpad;
#ifdef _WIN32
    wordpad.startDetached("explorer installed.txt");
#elif defined(__APPLE__)
    wordpad.startDetached("open installed.txt");
#else
    wordpad.startDetached("xdg-open installed.txt");
#endif
}

//Network Manager
SslNetworkAccessManager::SslNetworkAccessManager()
{
}

QNetworkReply* SslNetworkAccessManager::createRequest(Operation op, const QNetworkRequest& req, QIODevice* outgoingData)
{
    QNetworkReply* reply = QNetworkAccessManager::createRequest(op, req, outgoingData);
    reply->ignoreSslErrors();
    return reply;
}
