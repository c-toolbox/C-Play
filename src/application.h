/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QObject>
#include <QElapsedTimer>

#include <KAboutData>
#include <KActionCollection>
#include <KSharedConfig>
#include "renderthread.h"

class KActionCollection;
class KConfigDialog;
class KColorSchemeManager;
class QAction;

class Application : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* colorSchemesModel READ colorSchemesModel CONSTANT)
    Q_PROPERTY(QUrl configFilePath READ configFilePath CONSTANT)
    Q_PROPERTY(QUrl configFolderPath READ configFolderPath CONSTANT)

public:
    explicit Application(int &argc, char **argv, const QString &applicationName);
    static void create(int &argc, char **argv, const QString &applicationName);
    static Application& instance();
    ~Application();

    int run();
    QUrl configFilePath();
    QUrl configFolderPath();
    Q_INVOKABLE QUrl parentUrl(const QString &path);
    Q_INVOKABLE QUrl pathToUrl(const QString &path);
    Q_INVOKABLE QString argument(int key);
    Q_INVOKABLE void addArgument(int key, const QString &value);
    Q_INVOKABLE QAction *action(const QString &name);
    Q_INVOKABLE QString getFileContent(const QString &file);
    Q_INVOKABLE QString mimeType(const QString &file);
    Q_INVOKABLE QStringList availableGuiStyles();
    Q_INVOKABLE void setGuiStyle(const QString &style);
    Q_INVOKABLE void activateColorScheme(const QString &name);
    Q_INVOKABLE void configureShortcuts();

    static QString version();
    Q_INVOKABLE static bool hasYoutubeDl();
    Q_INVOKABLE static bool isYoutubePlaylist(const QString &path);
    Q_INVOKABLE static QString formatTime(const double time);
    Q_INVOKABLE static void hideCursor();
    Q_INVOKABLE static void showCursor();

    int getFadeDurationCurrentTime(bool restart);
    int getFadeDurationSetting();

private:
    void setupWorkerThread();
    void setupAboutData();
    void setupCommandLineParser();
    void registerQmlTypes();
    void setupQmlSettingsTypes();
    void setupQmlContextProperties();
    void aboutApplication();
    void setupActions(const QString &actionName);
    QAbstractItemModel *colorSchemesModel();
    QApplication *m_app;
    QQmlApplicationEngine *m_engine;
    KAboutData m_aboutData;
    KActionCollection m_collection;
    KSharedConfig::Ptr m_config;
    KConfigGroup *m_shortcuts;
    QMap<int, QString> m_args;
    KColorSchemeManager *m_schemes;
    QString m_systemDefaultStyle;
    RenderThread renderThread;
    QElapsedTimer fadeDurationTimer;
    static Application* _instance;
};

class SyncHelper
{
public:
    struct SyncVariables {
        std::string loadedFile;
        std::string overlayFile;
        bool loadFile;
        bool paused;
        double timePosition;
        double timeThreshold;
        bool timeDirty;
        float alpha;
        bool sbs3DVideo;
        bool syncOn;
        int loopMode;
        int gridToMapOn;
        int contrast;
        int brightness;
        int gamma;
        int saturation;
        double radius;
        double fov;
        double angle;
        double rotateX;
        double rotateY;
        double rotateZ;
        double translateX;
        double translateY;
        double translateZ;
    };

    SyncHelper();
    ~SyncHelper();

    static SyncHelper& instance();

    SyncVariables variables;
private:
    static SyncHelper* _instance;

};

#endif // APPLICATION_H
