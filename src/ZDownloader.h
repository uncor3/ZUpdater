/*
 * Copyright (c) 2014-2021 Alex Spataru <https://github.com/alex-spataru>
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

#ifndef DOWNLOAD_DIALOG_H
#define DOWNLOAD_DIALOG_H

#include "ui_ZDownloader.h"
#include <QDialog>
#include <QDir>
#include <QString>

struct UpdateProcedure {
    bool openFile;
    bool openFileDir;
    bool quitApp;
    QString boxInformativeText;
    QString boxText;
};

class QAuthenticator;
class QNetworkReply;
class QNetworkAccessManager;
class QDialog;
namespace Ui
{
class ZDownloader;
}

class ZDownloader : public QWidget
{
    Q_OBJECT

signals:
    void downloadFinished(const QUrl &url, const QString &filepath);

public:
    explicit ZDownloader(UpdateProcedure updateProcedure, QWidget *parent = 0);
    ~ZDownloader();

    QString downloadDir() const;
    void setDownloadDir(const QString &downloadDir);

public slots:
    void startDownload(const QUrl &url);
    void setFileName(const QString &file);
    void setUserAgentString(const QString &agent);

private slots:
    void finished(const QUrl &url);
    void metaDataChanged();
    void openDownload();
    void installUpdate();
    void cancelDownload();
    void saveFile(qint64 received, qint64 total);
    void calculateSizes(qint64 received, qint64 total);
    void updateProgress(qint64 received, qint64 total);
    void calculateTimeRemaining(qint64 received, qint64 total);

private:
    qreal round(const qreal &input);
    UpdateProcedure m_updateProcedure;

private:
    uint m_startTime;
    QDir m_downloadDir;
    QString m_fileName;
    Ui::ZDownloader *m_ui;
    QNetworkReply *m_reply;
    QString m_userAgentString;
    QNetworkAccessManager *m_manager;
};

#endif