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

#include "ZUpdater.h"
#include "ZDownloader.h"
#include <QDesktopServices>
#include <QMessageBox>
#include <QScrollArea>

ZUpdater::ZUpdater(const QString &repoOwnerSlashName,
                   const QString &currentVersion,
                   const QString &applicationName,
                   UpdateProcedure updateProcedure, bool isPortable,
                   bool isPackageManagerManaged, QObject *parent)
    : QObject(parent), m_repoOwnerSlashName(repoOwnerSlashName),
      m_currentVersion(currentVersion), m_applicationName(applicationName),
      m_networkManager(new QNetworkAccessManager(this)),
      m_isPortable(isPortable),
      m_isPackageManagerManaged(isPackageManagerManaged),
      m_updateProcedure(updateProcedure)
{
    // Detect platform and architecture
    m_platform = detectPlatform();
    m_architecture = detectArchitecture();

    qDebug() << "Platform:" << m_platform << "Architecture:" << m_architecture
             << "Portable:" << m_isPortable;
}

ZUpdater::~ZUpdater() {}

void ZUpdater::setUpdateAvailableMessage(const QString &msg)
{
    m_updateAvailableMsg = msg;
}

void ZUpdater::setNoUpdateMessage(const QString &msg) { m_noUpdateMsg = msg; }

void ZUpdater::setCheckingMessage(const QString &msg) { m_checkingMsg = msg; }

void ZUpdater::setErrorMessage(const QString &msg) { m_errorMsg = msg; }

void ZUpdater::setDownloadPromptMessage(const QString &msg)
{
    m_downloadPromptMsg = msg;
}

Platform::Type ZUpdater::detectPlatform()
{
#if defined(Q_OS_WIN)
    return Platform::Windows;
#elif defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    return Platform::MacOS;
#elif defined(Q_OS_LINUX)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

Architecture::Type ZUpdater::detectArchitecture()
{
#if defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_AMD64)
    return Architecture::x86_64;
#elif defined(Q_PROCESSOR_ARM_64) || defined(Q_PROCESSOR_AARCH64)
    return Architecture::ARM64;
#elif defined(Q_PROCESSOR_ARM)
    return Architecture::ARM;
#else
    return Architecture::Unknown;
#endif
}

void ZUpdater::checkForUpdates()
{
    if (m_platform == Platform::Unknown ||
        m_architecture == Architecture::Unknown) {
        qWarning() << "Unknown platform or architecture";
        return;
    }

    QString updateUrl = QString("https://api.github.com/repos/%1/releases")
                            .arg(m_repoOwnerSlashName);
    QNetworkRequest request((QUrl(updateUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "ZUpdater");

    QNetworkReply *reply = m_networkManager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        qDebug() << "Update check reply received";
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to fetch updates:" << reply->errorString();
            reply->deleteLater();
            return;
        }
        QByteArray response = reply->readAll();
        reply->deleteLater();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        qDebug() << "Received update data:" << jsonDoc;
        checkUpdatesInternal(jsonDoc);
    });
}

void ZUpdater::checkUpdatesInternal(QJsonDocument jsonDoc)
{
    if (!jsonDoc.isArray()) {
        qWarning() << "Invalid response format";
        return;
    }
    QJsonArray releases = jsonDoc.array();
    if (releases.isEmpty()) {
        qInfo() << "No releases found";
        return;
    }

    /*
      find the latest release & compare versions
    */
    QJsonObject latestVersionObj;
    QString latestVersion;
    for (const QJsonValue &releaseVal : releases) {
        if (!releaseVal.isObject())
            continue;
        QJsonObject releaseObj = releaseVal.toObject();
        QString tagName = releaseObj.value("tag_name").toString();
        if (tagName.isEmpty())
            continue;

        // Extract version number from tag
        QRegularExpression re("v?(\\d+(?:\\.\\d+)*)");
        QRegularExpressionMatch match = re.match(tagName);
        if (!match.hasMatch())
            continue;
        QString version = match.captured(1);
        if (version.isEmpty())
            continue;

        if (compareVersions(m_currentVersion, version)) {
            latestVersionObj = releaseObj;
            latestVersion = version;
            qDebug() << "Found newer release version:" << version;
            break; // Just process the latest release for now
        }
    }

    if (latestVersionObj.isEmpty()) {
        qWarning() << "latestVersionObj is emty";
        return;
    }

    QJsonValue asset = latestVersionObj.value("assets");
    if (!asset.isArray() || asset.toArray().isEmpty()) {
        qWarning() << "No assets found for the latest release";
        return;
    }
    QString assetPattern = detectAssetPattern();
    qDebug() << "Looking for asset matching pattern:" << assetPattern;

    // Linux scenario
    if (m_platform == Platform::Linux) {
        if (m_isPackageManagerManaged)
            return showPackageManagerManagedUpdateMessage(latestVersionObj);

        QJsonObject obj = getMatchingAsset(assetPattern, asset.toArray());

        if (obj.isEmpty()) {
            qWarning()
                << "No matching asset found for the platform/architecture";
            return;
        }
        QVariantMap downloadProfile;
        downloadProfile["body"] =
            latestVersionObj.value("body").toString().replace("\n", "<br/>");
        downloadProfile["tag_name"] =
            latestVersionObj.value("tag_name").toString();
        downloadProfile["browser_download_url"] =
            obj.value("browser_download_url").toString();
        downloadProfile["file_name"] = obj.value("name").toString();

        qDebug() << "Download url:"
                 << obj.value("browser_download_url").toString();

        return showDownloadMessageBox(downloadProfile);
    }

    // Windows and macOS scenario
    QJsonObject obj = getMatchingAsset(assetPattern, asset.toArray());
    if (obj.isEmpty()) {
        qWarning() << "No matching asset found for the platform/architecture";
        return;
    }

    if (m_platform == Platform::MacOS && m_isPackageManagerManaged) {
        return showPackageManagerManagedUpdateMessage(latestVersionObj);
    }

    QVariantMap downloadProfile;

    downloadProfile["body"] =
        latestVersionObj.value("body").toString().replace("\n", "<br/>");
    downloadProfile["tag_name"] = latestVersionObj.value("tag_name").toString();
    downloadProfile["browser_download_url"] =
        obj.value("browser_download_url").toString();
    downloadProfile["file_name"] = obj.value("name").toString();

    showDownloadMessageBox(downloadProfile);
    return;
}

void ZUpdater::showPackageManagerManagedUpdateMessage(const QJsonObject &obj)
{
    QString changeLog = obj.value("body").toString().replace("\n", "<br/>");
    QString version = obj.value("tag_name").toString();
    QString htmlUrl = obj.value("html_url").toString();

    QMessageBox box;
    box.setTextFormat(Qt::RichText);
    box.setIcon(QMessageBox::Information);
    box.setStandardButtons(QMessageBox::Ok);

    QString title =
        "<h3>" + QString("Version %1 is available!").arg(version) + "</h3>";

    if (!changeLog.isEmpty()) {
        m_packageManagerManagedMsg +=
            "<br/><br/><strong>Change log:</strong><br/>" + changeLog;
    }

    box.setText(title);
    box.setInformativeText(m_packageManagerManagedMsg);
    box.exec();
    return;
}

QJsonObject ZUpdater::getMatchingAsset(const QString &assetPattern,
                                       const QJsonArray &assets)
{
    for (const QJsonValue &a : assets) {
        if (!a.isObject())
            continue;
        QJsonObject obj = a.toObject();
        QString name = obj.value("name").toString();
        if (name.isEmpty())
            continue;

        QRegularExpression assetRegex(
            assetPattern, QRegularExpression::CaseInsensitiveOption);
        if (assetRegex.match(name).hasMatch()) {
            return obj;
        }
    }

    return QJsonObject();
}

QString ZUpdater::detectAssetPattern()
{
    QString pattern;

    if (m_platform == Platform::Windows) {
        QString arch =
            (m_architecture == Architecture::x86_64) ? "x86_64" : "arm64";
        if (m_isPortable) {
            pattern = QString(".*-Windows_%1\\.portable\\.zip$").arg(arch);
        } else {
            pattern = QString(".*-Windows_%1\\.msi$").arg(arch);
        }
    } else if (m_platform == Platform::MacOS) {
        if (m_architecture == Architecture::x86_64) {
            pattern = ".*-Apple_Intel\\.dmg$";
        } else if (m_architecture == Architecture::ARM64) {
            pattern = ".*-Apple_Silicon\\.dmg$";
        }
    } else if (m_platform == Platform::Linux) {
        QString arch =
            (m_architecture == Architecture::x86_64) ? "x86_64" : "arm64";
        pattern = QString(".*-Linux_%1\\.appimage$").arg(arch);
    }

    return pattern;
}

bool ZUpdater::compareVersions(const QString &currentVersion,
                               const QString &latestVersion)
{
    // Parse version strings into numeric components
    auto parseVersion = [](const QString &version) -> QVector<int> {
        QVector<int> components;
        QStringList parts = version.split('.', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            bool ok;
            int num = part.toInt(&ok);
            if (ok) {
                components.append(num);
            } else {
                // If not a number, treat as 0
                components.append(0);
            }
        }
        return components;
    };

    QVector<int> current = parseVersion(currentVersion);
    QVector<int> latest = parseVersion(latestVersion);

    // Pad with zeros to make them equal length
    int maxSize = qMax(current.size(), latest.size());
    while (current.size() < maxSize)
        current.append(0);

    while (latest.size() < maxSize)
        latest.append(0);

    // Compare component by component (properly handles 1.1.10 vs 1.1.1)
    for (int i = 0; i < maxSize; ++i) {
        if (latest[i] > current[i])
            return true; // Latest is newer
        else if (latest[i] < current[i])
            return false; // Current is newer
        // If equal, continue to next component
    }

    return false; // Versions are equal
}

void ZUpdater::showDownloadMessageBox(const QVariantMap &downloadProfile)
{
    QString changeLog = downloadProfile.value("body").toString();
    QString version = downloadProfile.value("tag_name").toString();
    QString url = downloadProfile.value("browser_download_url").toString();

    QMessageBox box;
    box.setTextFormat(Qt::RichText);
    box.setIcon(QMessageBox::Information);

    QString text = tr("Would you like to download the update now?");
    text += "<br/><br/>";

    QString title = "<h3>" +
                    tr("Version %1 of %2 has been released!")
                        .arg(version)
                        .arg(m_applicationName) +
                    "</h3>";

    if (!changeLog.isEmpty())
        text += tr("<strong>Change log:</strong><br/>%1").arg(changeLog);

    box.setText(title);
    box.setInformativeText(text);
    box.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    box.setDefaultButton(QMessageBox::Yes);
    if (box.exec() == QMessageBox::Yes) {
        download(downloadProfile);
    }
}

void ZUpdater::download(const QVariantMap &downloadProfile)
{
    QString name = downloadProfile.value("file_name").toString();
    QString url = downloadProfile.value("browser_download_url").toString();

    ZDownloader *downloader = new ZDownloader(m_updateProcedure);

    downloader->setFileName(name);
    downloader->show();
    downloader->startDownload(url);
}

void ZUpdater::setPackageManagerManagedMessage(const QString &msg)
{
    m_packageManagerManagedMsg = msg;
}