/*
 * Copyright (c) 2025 Uncore <https://github.com/uncor3>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ZDownloader.h"
#include <QtConcurrent>
#include <QtCore>
#include <QtNetwork>

struct Platform {
    enum Type { Windows, MacOS, Linux, Unknown };
};

struct Architecture {
    enum Type { x86_64, ARM64, ARM, Unknown };
};

class ZUpdater : public QObject
{
    Q_OBJECT
public:
    ZUpdater(const QString &repoOwnerSlashName, const QString &currentVersion,
             const QString &applicationName, UpdateProcedure updateProcedure,
             bool isPortable = false, bool isPackageManagerManaged = false,
             QObject *parent = nullptr);
    ~ZUpdater();

    void checkForUpdates();

    // Message customization methods
    void setUpdateAvailableMessage(const QString &msg);
    void setNoUpdateMessage(const QString &msg);
    void setCheckingMessage(const QString &msg);
    void setErrorMessage(const QString &msg);
    void setDownloadPromptMessage(const QString &msg);

    // Platform/Architecture info getters
    Platform::Type platform() const { return m_platform; }
    Architecture::Type architecture() const { return m_architecture; }
    bool isPortable() const { return m_isPortable; }
    bool isPackageManagerManaged() const { return m_isPackageManagerManaged; }
    static Platform::Type detectPlatform();
    static Architecture::Type detectArchitecture();

private:
    bool compareVersions(const QString &currentVersion,
                         const QString &latestVersion);
    void showDownloadMessageBox(const QVariantMap &downloadProfile);
    void download(const QVariantMap &downloadProfile);
    void showPackageManagerManagedUpdateMessage(const QJsonObject &obj);
    QJsonObject getMatchingAsset(const QString &assetPattern,
                                 const QJsonArray &assets);
    QString detectAssetPattern();
    void checkUpdatesInternal(QJsonDocument jsonDoc);

    QString m_repoOwnerSlashName;
    QString m_currentVersion;
    QString m_applicationName;
    bool m_isPortable;
    bool m_isPackageManagerManaged;
    UpdateProcedure m_updateProcedure;

    Platform::Type m_platform;
    Architecture::Type m_architecture;

    QNetworkAccessManager *m_networkManager;

    // Customizable messages
    QString m_updateAvailableMsg;
    QString m_noUpdateMsg;
    QString m_checkingMsg;
    QString m_errorMsg;
    QString m_downloadPromptMsg;
};
