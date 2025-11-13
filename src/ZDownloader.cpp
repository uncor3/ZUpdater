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

#include "ZDownloader.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QTimer>
#include <math.h>

static const QString PARTIAL_DOWN(".part");

ZDownloader::ZDownloader(UpdateProcedure updateProcedure, QWidget *parent)
    : QWidget(parent), m_ui(new Ui::ZDownloader),
      m_updateProcedure(updateProcedure)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    /* Initialize private members */
    m_manager = new QNetworkAccessManager(this);

    m_fileName = "";
    m_startTime = 0;

    /* Set download directory */
    QString dl =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dl.isEmpty())
        dl = QDir::homePath();
    m_downloadDir.setPath(dl);

    /* Make the window look like a modal dialog */
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    /* Configure the appearance and behavior of the buttons */
    m_ui->openButton->setEnabled(false);
    m_ui->openButton->setVisible(false);
    connect(m_ui->stopButton, SIGNAL(clicked()), this, SLOT(cancelDownload()));
    connect(m_ui->openButton, SIGNAL(clicked()), this, SLOT(installUpdate()));

    /* Resize to fit */
    setFixedSize(minimumSizeHint());
}

ZDownloader::~ZDownloader() { delete m_ui; }

/**
 * Begins downloading the file at the given \a url
 */
void ZDownloader::startDownload(const QUrl &url)
{
    /* Reset UI */
    m_ui->progressBar->setValue(0);
    m_ui->stopButton->setText(tr("Stop"));
    m_ui->downloadLabel->setText(tr("Downloading updates"));
    m_ui->timeLabel->setText(tr("Time remaining") + ": " + tr("unknown"));

    /* Configure the network request */
    QNetworkRequest request(url);

    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    /* 10s timeout */
    request.setTransferTimeout(10000);
#endif

    if (!m_userAgentString.isEmpty())
        request.setRawHeader("User-Agent", m_userAgentString.toUtf8());

    /* Start download */
    m_reply = m_manager->get(request);
    m_startTime = QDateTime::currentDateTime().toSecsSinceEpoch();

    /* Ensure that downloads directory exists */
    if (!m_downloadDir.exists())
        m_downloadDir.mkpath(".");

    /* Remove old downloads */
    QFile::remove(m_downloadDir.filePath(m_fileName));
    QFile::remove(m_downloadDir.filePath(m_fileName + PARTIAL_DOWN));

    /* Update UI when download progress changes or download finishes */
    connect(m_reply, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));
    connect(m_reply, SIGNAL(downloadProgress(qint64, qint64)), this,
            SLOT(updateProgress(qint64, qint64)));
    // call finished with the original URL when reply finishes
    connect(m_reply, &QNetworkReply::finished, this,
            [this, url]() { finished(url); });

    showNormal();
}

/**
 * Changes the name of the downloaded file
 */
void ZDownloader::setFileName(const QString &file)
{
    m_fileName = file;

    if (m_fileName.isEmpty())
        m_fileName = "ZUpdate.bin";
}

/**
 * Changes the user-agent string used to communicate with the remote HTTP server
 */
void ZDownloader::setUserAgentString(const QString &agent)
{
    m_userAgentString = agent;
}

void ZDownloader::finished(const QUrl &url)
{
    if (m_reply->error() != QNetworkReply::NoError) {
        QFile::remove(m_downloadDir.filePath(m_fileName + PARTIAL_DOWN));
        return;
    }

    /* Rename file */
    QFile::rename(m_downloadDir.filePath(m_fileName + PARTIAL_DOWN),
                  m_downloadDir.filePath(m_fileName));

    /* Notify application */
    emit downloadFinished(url, m_downloadDir.filePath(m_fileName));

    /* Install the update */
    m_reply->close();
    installUpdate();
    setVisible(false);
}

/**
 * Opens the downloaded file.
 * \note If the downloaded file is not found, then the function will alert the
 *       user about the error.
 */
void ZDownloader::openDownload()
{
    if (!m_fileName.isEmpty())
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(m_downloadDir.filePath(m_fileName)));

    else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Cannot find downloaded update!"),
                              QMessageBox::Close);
    }
}

/**
 * Instructs the OS to open the downloaded file.
 *
 */
void ZDownloader::installUpdate()
{
    /* Update labels */
    m_ui->stopButton->setText(tr("Close"));
    m_ui->downloadLabel->setText(tr("Download complete!"));
    m_ui->timeLabel->setText(tr("The installer will open separately") + "...");

    /* Ask the user to install the download */
    QMessageBox box;
    box.setIcon(QMessageBox::Question);
    box.setDefaultButton(QMessageBox::Ok);
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setInformativeText(m_updateProcedure.boxInformativeText);

    QString text = m_updateProcedure.boxText;
    box.setText("<h3>" + text + "</h3>");

    /* User wants to install the download */
    if (box.exec() == QMessageBox::Ok) {
        if (m_updateProcedure.openFile) {
            if (m_updateProcedure.quitApp) {
                // QProcess::startDetached(m_downloadDir.filePath(m_fileName),
                //                         QStringList());
                openDownload();
                QTimer::singleShot(0, []() { return qApp->quit(); });
                return;
            }
            openDownload();
        } else if (m_updateProcedure.openFileDir) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(m_downloadDir.path()));
        }
    } else {
        m_ui->openButton->setEnabled(true);
        m_ui->openButton->setVisible(true);
        m_ui->timeLabel->setText(tr("Click the \"Open\" button to "
                                    "apply the update"));
    }
}

/**
 * Prompts the user if he/she wants to cancel the download and cancels the
 * download if the user agrees to do that.
 */
void ZDownloader::cancelDownload()
{
    if (!m_reply->isFinished()) {
        QMessageBox box;
        box.setWindowTitle(tr("Updater"));
        box.setIcon(QMessageBox::Question);
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

        QString text = tr("Are you sure you want to cancel the download?");
        box.setText(text);

        if (box.exec() == QMessageBox::Yes) {
            hide();
            m_reply->abort();
        }
    } else {
        hide();
    }
}

/**
 * Writes the downloaded data to the disk
 */
void ZDownloader::saveFile(qint64 received, qint64 total)
{
    Q_UNUSED(received);
    Q_UNUSED(total);

    /* Check if we need to redirect */
    QUrl url =
        m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!url.isEmpty()) {
        startDownload(url);
        return;
    }

    /* Save downloaded data to disk */
    QFile file(m_downloadDir.filePath(m_fileName + PARTIAL_DOWN));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(m_reply->readAll());
        file.close();
    }
}

/**
 * Calculates the appropiate size units (bytes, KB or MB) for the received
 * data and the total download size. Then, this function proceeds to update the
 * dialog controls/UI.
 */
void ZDownloader::calculateSizes(qint64 received, qint64 total)
{
    QString totalSize;
    QString receivedSize;

    if (total < 1024)
        totalSize = tr("%1 bytes").arg(total);

    else if (total < 1048576)
        totalSize = tr("%1 KB").arg(round(total / 1024));

    else
        totalSize = tr("%1 MB").arg(round(total / 1048576));

    if (received < 1024)
        receivedSize = tr("%1 bytes").arg(received);

    else if (received < 1048576)
        receivedSize = tr("%1 KB").arg(received / 1024);

    else
        receivedSize = tr("%1 MB").arg(received / 1048576);

    m_ui->downloadLabel->setText(tr("Downloading updates") + " (" +
                                 receivedSize + " " + tr("of") + " " +
                                 totalSize + ")");
}

/**
 * Get response filename.
 */
void ZDownloader::metaDataChanged()
{
    QString filename = "";
    QVariant variant =
        m_reply->header(QNetworkRequest::ContentDispositionHeader);
    if (variant.isValid()) {
        QString contentDisposition =
            QByteArray::fromPercentEncoding(variant.toByteArray()).constData();
        QRegularExpression regExp(R"(filename=(\S+))");
        QRegularExpressionMatch match = regExp.match(contentDisposition);
        if (match.hasMatch()) {
            filename = match.captured(1);
        }
        setFileName(filename);
    }
}

/**
 * Uses the \a received and \a total parameters to get the download progress
 * and update the progressbar value on the dialog.
 */
void ZDownloader::updateProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        m_ui->progressBar->setMinimum(0);
        m_ui->progressBar->setMaximum(100);
        m_ui->progressBar->setValue((received * 100) / total);

        calculateSizes(received, total);
        calculateTimeRemaining(received, total);
        saveFile(received, total);
    }

    else {
        m_ui->progressBar->setMinimum(0);
        m_ui->progressBar->setMaximum(0);
        m_ui->progressBar->setValue(-1);
        m_ui->downloadLabel->setText(tr("Downloading Updates") + "...");
        m_ui->timeLabel->setText(
            QString("%1: %2").arg(tr("Time Remaining")).arg(tr("Unknown")));
    }
}

/**
 * Uses two time samples (from the current time and a previous sample) to
 * calculate how many bytes have been downloaded.
 *
 * Then, this function proceeds to calculate the appropiate units of time
 * (hours, minutes or seconds) and constructs a user-friendly string, which
 * is displayed in the dialog.
 */
void ZDownloader::calculateTimeRemaining(qint64 received, qint64 total)
{
    uint difference =
        QDateTime::currentDateTime().toSecsSinceEpoch() - m_startTime;

    if (difference > 0) {
        QString timeString;
        qreal timeRemaining = (total - received) / (received / difference);

        if (timeRemaining > 7200) {
            timeRemaining /= 3600;
            int hours = int(timeRemaining + 0.5);

            if (hours > 1)
                timeString = tr("about %1 hours").arg(hours);
            else
                timeString = tr("about one hour");
        }

        else if (timeRemaining > 60) {
            timeRemaining /= 60;
            int minutes = int(timeRemaining + 0.5);

            if (minutes > 1)
                timeString = tr("%1 minutes").arg(minutes);
            else
                timeString = tr("1 minute");
        }

        else if (timeRemaining <= 60) {
            int seconds = int(timeRemaining + 0.5);

            if (seconds > 1)
                timeString = tr("%1 seconds").arg(seconds);
            else
                timeString = tr("1 second");
        }

        m_ui->timeLabel->setText(tr("Time remaining") + ": " + timeString);
    }
}

/**
 * Rounds the given \a input to two decimal places
 */
qreal ZDownloader::round(const qreal &input)
{
    return static_cast<qreal>(roundf(static_cast<float>(input) * 100) / 100);
}

QString ZDownloader::downloadDir() const
{
    return m_downloadDir.absolutePath();
}

void ZDownloader::setDownloadDir(const QString &downloadDir)
{
    if (m_downloadDir.absolutePath() != downloadDir)
        m_downloadDir.setPath(downloadDir);
}
