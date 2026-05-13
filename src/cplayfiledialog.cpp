/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cplayfiledialog.h"

#include "userinterfacesettings.h"

#include <QFileDialog>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>
#include <QWidget>

CPlayFileDialog::CPlayFileDialog(QObject* parent)
    : QObject(parent) {
}

CPlayFileDialog::~CPlayFileDialog() {
    close();
}

QObject* CPlayFileDialog::parentWindow() const {
    return m_parentWindow;
}

void CPlayFileDialog::setParentWindow(QObject* parentWindow) {
    if (m_parentWindow == parentWindow)
        return;

    m_parentWindow = parentWindow;
    Q_EMIT parentWindowChanged();
}

CPlayFileDialog::FileMode CPlayFileDialog::fileMode() const {
    return m_fileMode;
}

void CPlayFileDialog::setFileMode(FileMode fileMode) {
    if (m_fileMode == fileMode)
        return;

    m_fileMode = fileMode;
    Q_EMIT fileModeChanged();
}

bool CPlayFileDialog::useFolder() const {
    return m_useFolder;
}

void CPlayFileDialog::setUseFolder(bool useFolder) {
    if (m_useFolder == useFolder)
        return;

    m_useFolder = useFolder;
    Q_EMIT useFolderChanged();
}

QUrl CPlayFileDialog::currentFolder() const {
    return m_currentFolder;
}

void CPlayFileDialog::setCurrentFolder(const QUrl& currentFolder) {
    if (m_currentFolder == currentFolder)
        return;

    m_currentFolder = currentFolder;
    Q_EMIT currentFolderChanged();
}

QUrl CPlayFileDialog::currentFile() const {
    return m_currentFile;
}

void CPlayFileDialog::setCurrentFile(const QUrl& currentFile) {
    if (m_currentFile == currentFile)
        return;

    m_currentFile = currentFile;
    Q_EMIT currentFileChanged();
}

QStringList CPlayFileDialog::nameFilters() const {
    return m_nameFilters;
}

void CPlayFileDialog::setNameFilters(const QStringList& nameFilters) {
    if (m_nameFilters == nameFilters)
        return;

    m_nameFilters = nameFilters;
    Q_EMIT nameFiltersChanged();
}

QString CPlayFileDialog::title() const {
    return m_title;
}

void CPlayFileDialog::setTitle(const QString& title) {
    if (m_title == title)
        return;

    m_title = title;
    Q_EMIT titleChanged();
}

QUrl CPlayFileDialog::selectedFile() const {
    return m_selectedFile;
}

QUrl CPlayFileDialog::selectedFolder() const {
    return m_selectedFile;
}

void CPlayFileDialog::open() {
    if (thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(this, &CPlayFileDialog::open, Qt::QueuedConnection);
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        openDialog();
    });
}

void CPlayFileDialog::close() {
    if (m_dialog) {
        m_dialog->close();
        m_dialog->deleteLater();
        m_dialog = nullptr;
    }
}

void CPlayFileDialog::openDialog() {
    openDialog(UserInterfaceSettings::useNativeFileDialogs());
}

void CPlayFileDialog::openDialog(bool useNativeDialog) {
    if (m_dialog) {
        m_openingNativeDialog = false;
    }
    else {
        m_openingNativeDialog = useNativeDialog;
    }

    close();

    QWidget* parentWidget = nullptr;
    if (auto* quickWindow = qobject_cast<QQuickWindow*>(m_parentWindow)) {
        parentWidget = QWidget::find(quickWindow->winId());
    }

    try {
        m_dialog = new QFileDialog(parentWidget, m_title);
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
        if (!useNativeDialog) {
            m_dialog->setOption(QFileDialog::DontUseNativeDialog, true);
        }
        if (m_useFolder) {
            m_dialog->setFileMode(QFileDialog::Directory);
            m_dialog->setOption(QFileDialog::ShowDirsOnly, true);
            m_dialog->setAcceptMode(QFileDialog::AcceptOpen);
        }
        else {
            m_dialog->setFileMode(QFileDialog::ExistingFile);
            m_dialog->setAcceptMode(m_fileMode == SaveFile ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
        }
        if (!m_useFolder && !m_nameFilters.isEmpty()) {
            m_dialog->setNameFilters(m_nameFilters);
        }
        if (m_currentFolder.isLocalFile()) {
            m_dialog->setDirectory(m_currentFolder.toLocalFile());
        }
        else if (!m_currentFolder.isEmpty()) {
            m_dialog->setDirectory(m_currentFolder.toString());
        }
        if (!m_useFolder && m_currentFile.isLocalFile()) {
            m_dialog->selectFile(m_currentFile.toLocalFile());
        }
        else if (!m_useFolder && !m_currentFile.isEmpty()) {
            m_dialog->selectFile(m_currentFile.toString());
        }
    }
    catch (...) {
        m_dialog = nullptr;
        if (useNativeDialog) {
            openNonNativeDialog();
        }
        else {
            Q_EMIT rejected();
        }
        return;
    }

    if (!m_dialog) {
        if (useNativeDialog) {
            openNonNativeDialog();
        }
        else {
            Q_EMIT rejected();
        }
        return;
    }

    QPointer<QFileDialog> dialog = m_dialog;
    connect(m_dialog, &QFileDialog::accepted, this, [this, dialog]() {
        if (!dialog)
            return;

        const QList<QUrl> urls = dialog->selectedUrls();
        const QUrl selected = urls.isEmpty() ? QUrl::fromLocalFile(dialog->selectedFiles().value(0)) : urls.first();
        if (m_selectedFile != selected) {
            m_selectedFile = selected;
            Q_EMIT selectedFileChanged();
            Q_EMIT selectedFolderChanged();
        }

        m_dialog = nullptr;
        Q_EMIT accepted();
    });
    connect(m_dialog, &QFileDialog::rejected, this, [this]() {
        if (m_openingNativeDialog && m_openTimer.isValid() && m_openTimer.elapsed() < 500) {
            m_dialog = nullptr;
            openNonNativeDialog();
            return;
        }

        m_dialog = nullptr;
        Q_EMIT rejected();
    });

    try {
        m_openTimer.restart();
        m_dialog->open();
    }
    catch (...) {
        close();
        if (useNativeDialog) {
            openNonNativeDialog();
        }
        else {
            Q_EMIT rejected();
        }
        return;
    }

    if (useNativeDialog) {
        QPointer<CPlayFileDialog> self(this);
        QPointer<QFileDialog> nativeDialog(m_dialog);
        QTimer::singleShot(250, this, [self, nativeDialog]() {
            if (!self || !nativeDialog || !self->m_openingNativeDialog)
                return;

            if (!nativeDialog->isVisible()) {
                self->close();
                self->openNonNativeDialog();
            }
        });
    }
}

void CPlayFileDialog::openNonNativeDialog() {
    QTimer::singleShot(0, this, [this]() {
        openDialog(false);
    });
}
