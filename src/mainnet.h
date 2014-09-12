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

#pragma once

#include <QtNetwork>
#include <QObject>
#include "splitter.h"
#include "installer.h"

class DownloadInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(int     id          MEMBER id          NOTIFY idChanged)
    Q_PROPERTY(int     maxId       MEMBER maxId       NOTIFY appsChanged)
    Q_PROPERTY(int     curProgress MEMBER curProgress NOTIFY sizeChanged)
    Q_PROPERTY(int     progress    MEMBER progress    NOTIFY sizeChanged)
    Q_PROPERTY(int     size        MEMBER size        NOTIFY sizeChanged)
    Q_PROPERTY(qint64  totalSize   MEMBER totalSize   NOTIFY appsChanged)
    Q_PROPERTY(QString curName     MEMBER curName     NOTIFY idChanged)
public:
    DownloadInfo(QObject* parent = 0)
        : QObject(parent)
        , baseDir("")
        , id(0), maxId(0)
        , progress(-1), curProgress(0)
        , size(0), totalSize(0)
    {
    }

    void resetApps() {
        maxId = 0;
        size = 0;
        totalSize = 0;
        progress = -1;
        apps.clear();
        curName.clear();
    }

    void setApps(QList<Apps*> newApps, QString& version) {
        resetApps();
        baseDir = QDir::currentPath() + "/" + version;

        // Check which apps user wanted
        for (Apps* newApp : newApps) {
            if (!newApp->isMarked())
                continue;
            apps.append(*newApp);
        }

        // Check which apps user doesn't already have
        for(int i = 0; i < apps.count(); i++) {
            QString fileName = baseDir + "/" + apps.at(i).name();
            // Either check signature or size to confirm?
            if (QFile::exists(fileName)) {
                apps.removeAt(i--);
            } else {
                // NOTE: Guide only! Maybe we should ask server for real filesizes beforehand
                totalSize += apps.at(i).size();
            }
        }
        maxId = apps.count();
        nextFile(0);
        emit appsChanged();
    }

    QString getFilename(int i = -1) {
        if (i == -1)
            i = id;
        if (i >= 0 && i < maxId)
            return baseDir + "/" + apps.at(i).name();
        else
            return "";
    }

    QString getUrl(int i = -1) {
        if (i == -1)
            i = id;
        if (i >= 0 && i < maxId)
            return apps.at(i).packageId();
        else
            return "";
    }
    int getSize(int i = -1) {
        if (i == -1)
            i = id;
        if (i >= 0 && i < maxId)
            return apps.at(i).size();
        else
            return 0;
    }
    void progressSize(qint64 bytes) {
        size += bytes;
        curSize += bytes;
        curProgress = 100*curSize / apps.at(id).size();
        progress = 100*size / totalSize;
        emit sizeChanged();
    }

    bool nextFile(int i = -1) {
        curSize = 0;
        if (i == -1)
            id++;
        else
            id = i;

        if (id >= 0 && id < maxId)
            curName = apps.at(id).friendlyName();

        emit idChanged();

        return (id < maxId);
    }

    QString baseDir;
    QList<Apps> apps;
    int id, maxId;
    int progress, curProgress;
    qint64 curSize, size, totalSize;
    QString curName;
signals:
    void idChanged();
    void sizeChanged();
    void appsChanged();
};

class MainNet : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString softwareRelease MEMBER _softwareRelease NOTIFY softwareReleaseChanged) // from reverse lookup
    Q_PROPERTY(QString updateMessage MEMBER _updateMessage NOTIFY updateMessageChanged)
    Q_PROPERTY(QQmlListProperty<Apps> updateAppList READ updateAppList NOTIFY updateMessageChanged)
    Q_PROPERTY(int updateAppCount READ updateAppCount NOTIFY updateMessageChanged)
    Q_PROPERTY(int updateCheckedCount READ updateCheckedCount NOTIFY updateCheckedCountChanged)
    Q_PROPERTY(QString error MEMBER _error NOTIFY errorChanged)
    Q_PROPERTY(QString multiscanVersion MEMBER _multiscanVersion NOTIFY updateMessageChanged)
    Q_PROPERTY(bool    downloading MEMBER _downloading WRITE setDownloading NOTIFY downloadingChanged)
    Q_PROPERTY(bool    hasPotentialLinks MEMBER _hasPotentialLinks NOTIFY hasPotentialLinksChanged)
    Q_PROPERTY(bool    hasBootAccess READ hasBootAccess CONSTANT)
    Q_PROPERTY(bool    multiscan MEMBER _multiscan WRITE setMultiscan NOTIFY multiscanChanged)
    Q_PROPERTY(int     scanning MEMBER _scanning WRITE setScanning NOTIFY scanningChanged)
    Q_PROPERTY(int     dlProgress MEMBER _dlProgress WRITE setDLProgress NOTIFY dlProgressChanged)
    Q_PROPERTY(int     maxId MEMBER _maxId NOTIFY maxIdChanged)
    Q_PROPERTY(int     splitting MEMBER _splitting NOTIFY splittingChanged)
    Q_PROPERTY(int     splitProgress MEMBER _splitProgress WRITE setSplitProgress NOTIFY splitProgressChanged)

public:
    MainNet(InstallNet* installer, QObject* parent = 0);
    ~MainNet();
    Q_INVOKABLE void updateDetailRequest(QString delta, QString carrier, QString country, int device, int variant, int mode, int server/*, int version*/);
    Q_INVOKABLE void downloadLinks(int downloadDevice = 0);
    Q_INVOKABLE void splitAutoloader(QUrl, int options);
    Q_INVOKABLE void combineAutoloader(QList<QUrl> selectedFiles);
    Q_INVOKABLE void extractImage(int type, int options);
    Q_INVOKABLE void grabLinks(int downloadDevice);
    Q_INVOKABLE void grabPotentialLinks(QString softwareRelease, QString osVersion);
    Q_INVOKABLE void abortDL(QNetworkReply::NetworkError error = (QNetworkReply::NetworkError)0);
    Q_INVOKABLE void abortSplit();
    Q_INVOKABLE void reverseLookup(int device, int variant, int server, QString OSver, bool skip);

    Q_INVOKABLE QString nameFromVariant(unsigned int device, unsigned int variant);
    Q_INVOKABLE QString hwidFromVariant(unsigned int device, unsigned int variant);
    Q_INVOKABLE unsigned int variantCount(unsigned int device);
    bool    hasBootAccess()  const { return
#ifdef BOOTLOADER_ACCESS
                true;
#else
                false;
#endif
                                   }
    void    setMultiscan(const bool &multiscan);
    void    setScanning(const int &scanning);
    void    setDLProgress(const int &progress);
    void    setDownloading(const bool &downloading);
    QQmlListProperty<Apps> updateAppList() {
        return QQmlListProperty<Apps>(this, &_updateAppList, &appendApps, &appsSize, &appsAt, &clearApps);
    }

    int updateAppCount() const { return _updateAppList.count(); }
    int updateCheckedCount() const {
        int checked = 0;
        for (Apps* app: _updateAppList)
            if (app->isMarked())
                checked++;
        return checked;
    }
    DownloadInfo* currentDownload;
public slots:
    void    setSplitProgress(const int &progress);
    void    capNetworkReply(QNetworkReply* reply);
    void    confirmNewSRSkip();
    void    confirmNewSR();
signals:
    void softwareReleaseChanged();
    void updateMessageChanged();
    void updateCheckedCountChanged();
    void errorChanged();
    void multiscanChanged();
    void scanningChanged();
    void downloadingChanged();
    void hasPotentialLinksChanged();
    void hasBootAccessChanged();
    void dlProgressChanged();
    void maxIdChanged();
    void splittingChanged();
    void splitProgressChanged();
    void killSplit();
private slots:
    void reverseLookupReply();
    void serverReply();
    void showFirmwareData(QByteArray data, QString variant);
    void serverError(QNetworkReply::NetworkError error);
    void downloadFinish();
    void cancelSplit();
// Blackberry
	void extractImageSlot(const QStringList& selectedFiles);

private:
    // Utils:
    InstallNet* _i;
    QString convertLinks(int downloadDevice, QString prepend);
    QString fixVariantName(QString name, QString replace, int type);
    void fixApps(int downloadDevice);
    QString NPCFromLocale(int country, int carrier);

    QThread* splitThread;
    QNetworkReply *replydl;
    QNetworkAccessManager *manager;
    QList<Apps*> _updateAppList;
    QString _updateMessage;
    QString _softwareRelease;
    QString _versionRelease;
    QString _error;
    QString _multiscanVersion;
    bool _downloading, _multiscan;
    bool _hasPotentialLinks;
    int _scanning;
    QFile _currentFile;
    int _maxId, _dlBytes, _dlTotal;
    int _splitting, _splitProgress;
    int _dlProgress;
    int _options;
    int _type;
};
