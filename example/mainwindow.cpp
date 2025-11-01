#include "mainwindow.h"
#include "../src/ZUpdater.h"
#include "./ui_mainwindow.h"

#ifndef APP_VERSION
#define APP_VERSION "0.1"
#endif

#ifndef APP_NAME
#define APP_NAME "ZUpdater Example"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Example usage with customization
    ZUpdater *updater =
        new ZUpdater("uncor3/libtest", APP_VERSION, APP_NAME,
                     {
                         true,   // openFile
                         false,  // openFileDir
                         false,  // quitApp
                         "TODO", // boxInformativeText
                         "Update will continue by opening the file" // boxText
                     },
                     false, // isPortable - set to true if running portable
                            // version on Windows
                     false, // isPackageManaged - set to true if installed via
                            // package manager on Linux
                     this);

    // Optional: Customize messages
    updater->setUpdateAvailableMessage("New version found!");
    updater->setNoUpdateMessage("You're all set!");
    updater->setCheckingMessage("Looking for updates...");
    updater->setErrorMessage("Oops! Couldn't check for updates.");
    updater->setDownloadPromptMessage(
        "Do you want to download and install it?");

    updater->checkForUpdates();
}

MainWindow::~MainWindow() { delete ui; }
