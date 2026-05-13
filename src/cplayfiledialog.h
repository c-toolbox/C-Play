/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAYFILEDIALOG_H
#define CPLAYFILEDIALOG_H

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QUrl>

class QFileDialog;

class CPlayFileDialog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* parentWindow READ parentWindow WRITE setParentWindow NOTIFY parentWindowChanged)
    Q_PROPERTY(FileMode fileMode READ fileMode WRITE setFileMode NOTIFY fileModeChanged)
    Q_PROPERTY(bool useFolder READ useFolder WRITE setUseFolder NOTIFY useFolderChanged)
    Q_PROPERTY(QUrl currentFolder READ currentFolder WRITE setCurrentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QUrl currentFile READ currentFile WRITE setCurrentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY nameFiltersChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QUrl selectedFile READ selectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QUrl selectedFolder READ selectedFolder NOTIFY selectedFolderChanged)

public:
    enum FileMode {
        OpenFile,
        SaveFile
    };
    Q_ENUM(FileMode)

    explicit CPlayFileDialog(QObject* parent = nullptr);
    ~CPlayFileDialog() override;

    QObject* parentWindow() const;
    void setParentWindow(QObject* parentWindow);

    FileMode fileMode() const;
    void setFileMode(FileMode fileMode);

    bool useFolder() const;
    void setUseFolder(bool useFolder);

    QUrl currentFolder() const;
    void setCurrentFolder(const QUrl& currentFolder);

    QUrl currentFile() const;
    void setCurrentFile(const QUrl& currentFile);

    QStringList nameFilters() const;
    void setNameFilters(const QStringList& nameFilters);

    QString title() const;
    void setTitle(const QString& title);

    QUrl selectedFile() const;
    QUrl selectedFolder() const;

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();

Q_SIGNALS:
    void parentWindowChanged();
    void fileModeChanged();
    void useFolderChanged();
    void currentFolderChanged();
    void currentFileChanged();
    void nameFiltersChanged();
    void titleChanged();
    void selectedFileChanged();
    void selectedFolderChanged();
    void accepted();
    void rejected();

private:
    void openDialog();
    void openDialog(bool useNativeDialog);
    void openNonNativeDialog();

    QObject* m_parentWindow = nullptr;
    FileMode m_fileMode = OpenFile;
    bool m_useFolder = false;
    QUrl m_currentFolder;
    QUrl m_currentFile;
    QStringList m_nameFilters;
    QString m_title;
    QUrl m_selectedFile;
    QFileDialog* m_dialog = nullptr;
    bool m_openingNativeDialog = false;
    QElapsedTimer m_openTimer;
};

#endif // CPLAYFILEDIALOG_H
