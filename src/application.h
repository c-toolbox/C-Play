/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QQmlApplicationEngine>

#include "renderthread.h"
#include <KAboutData>
#include <KActionCollection>
#include <KSharedConfig>

class KActionCollection;
class KConfigDialog;
class KColorSchemeManager;
class QAction;
class SlidesModel;

#ifdef NDI_SUPPORT
class NDISendersModel;
class PortAudioModel;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#ifndef OPAQUE_PTR_SlidesModel
#define OPAQUE_PTR_SlidesModel
Q_DECLARE_OPAQUE_POINTER(SlidesModel *)
#ifdef NDI_SUPPORT
Q_DECLARE_OPAQUE_POINTER(NDISendersModel*)
Q_DECLARE_OPAQUE_POINTER(PortAudioModel*)
#endif
#endif
#endif

class Application : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *colorSchemesModel READ colorSchemesModel CONSTANT)
    Q_PROPERTY(QUrl configFilePath READ configFilePath CONSTANT)
    Q_PROPERTY(QUrl configFolderPath READ configFolderPath CONSTANT)

public:
    explicit Application(int &argc, char **argv, const QString &applicationName);
    static void create(int &argc, char **argv, const QString &applicationName);
    static Application &instance();
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
    Q_INVOKABLE QStringList availableGuiStyles();
    Q_INVOKABLE void setGuiStyle(const QString &style);
    Q_INVOKABLE void activateColorScheme(const QString &name);
    Q_INVOKABLE void configureShortcuts();
    Q_INVOKABLE void updateAboutOtherText(const QString &mpvVersion, const QString &ffmpegVersion);
    Q_INVOKABLE QString getStartupFile();

    static QString version();
    Q_INVOKABLE static bool hasYoutubeDl();
    Q_INVOKABLE static bool isYoutubePlaylist(const QString &path);
    Q_INVOKABLE static QString formatTime(const double time);
    Q_INVOKABLE static void hideCursor();
    Q_INVOKABLE static void showCursor();
    Q_INVOKABLE static QString mimeType(QUrl url);

    int getFadeDurationCurrentTime(bool restart);
    int getFadeDurationSetting();
    void setStartupFile(std::string filePath);

    Q_PROPERTY(SlidesModel *slides
                   READ slidesModel
                       NOTIFY slidesModelChanged)
    SlidesModel *slidesModel();

#ifdef NDI_SUPPORT
    Q_PROPERTY(NDISendersModel* ndiSendersModel
        READ ndiSendersModel
        WRITE setNdiSendersModel
        NOTIFY ndiSendersModelChanged)

    NDISendersModel* ndiSendersModel();
    void setNdiSendersModel(NDISendersModel* model);

    Q_PROPERTY(PortAudioModel* portAudioModel
        READ portAudioModel
        WRITE setPortAudioModel
        NOTIFY portAudioModelChanged)

    PortAudioModel* portAudioModel();
    void setPortAudioModel(PortAudioModel* model);
#endif

Q_SIGNALS:
    void actionsUpdated();
    void slidesModelChanged();
#ifdef NDI_SUPPORT
    void ndiSendersModelChanged();
    void portAudioModelChanged();
#endif

private:
    void setupWorkerThread();
    void setupAboutData();
    void setupCommandLineParser();
    void registerQmlTypes();
    void setupQmlSettingsTypes();
    void setupQmlContextProperties();
    void aboutApplication();
    void setupActions(const QString &actionName);

    SlidesModel *m_slidesModel;
#ifdef NDI_SUPPORT
    NDISendersModel* m_ndiSendersModel;
    PortAudioModel* m_portAudioModel;
#endif
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
    QString m_startupFileFromCmd;
    RenderThread renderThread;
    QElapsedTimer fadeDurationTimer;
    static Application *_instance;
};

class SyncHelper {
public:
    struct SyncVariables {
        std::string loadedFile;
        std::string overlayFile;
        std::string bgImageFile;
        std::string fgImageFile;
        bool loadFile;
        bool overlayFileDirty;
        bool bgImageFileDirty;
        bool fgImageFileDirty;
        bool paused;
        double timePosition;
        double timeThreshold;
        bool timeThresholdEnabled;
        bool timeThresholdOnLoopOnly;
        double timeThresholdOnLoopCheckTime;
        bool timeDirty;
        bool syncOn;
        float alpha;
        float alphaBg;
        float alphaFg;
        int gridToMapOn;
        int gridToMapOnBg;
        int gridToMapOnFg;
        int stereoscopicMode;
        int stereoscopicModeBg;
        int stereoscopicModeFg;
        int eofMode;
        int viewMode;
        double radius;
        double fov;
        double angle;
        double rotateX;
        double rotateY;
        double rotateZ;
        double translateX;
        double translateY;
        double translateZ;
        double planeWidth;
        double planeHeight;
        double planeElevation;
        double planeDistance;
        int planeConsiderAspectRatio;
        bool eqDirty;
        int eqContrast;
        int eqBrightness;
        int eqGamma;
        int eqSaturation;
        bool loopTimeDirty;
        bool loopTimeEnabled;
        double loopTimeA;
        double loopTimeB;
    };

    struct ConfigurationVariables {
        std::string confAll;
        std::string confMasterOnly;
        std::string confNodesOnly;
    };

    SyncHelper();
    ~SyncHelper();

    static SyncHelper &instance();

    SyncVariables variables = {
        /*loadedFile*/ "",
        /*overlayFile*/ "",
        /*bgImageFile*/ "",
        /*fgImageFile*/ "",
        /*loadFile*/ false,
        /*overlayFileDirty*/ false,
        /*bgImageFileDirty*/ false,
        /*fgImageFileDirty*/ false,
        /*paused*/ false,
        /*timePosition*/ 0.0,
        /*timeThreshold*/ 1.0,
        /*timeThresholdEnabled*/ false,
        /*timeThresholdOnLoopOnly*/ false,
        /*timeThresholdOnLoopCheckTime*/ 1.0,
        /*timeDirty*/ false,
        /*syncOn*/ true,
        /*alpha*/ 1.f,
        /*alphaBg*/ 1.f,
        /*alphaFg*/ 0.f,
        /*gridToMapOn*/ 0,
        /*gridToMapOnBg*/ 0,
        /*gridToMapOnFg*/ 0,
        /*stereoscopicMode*/ 0,
        /*stereoscopicModeBg*/ 0,
        /*stereoscopicModeFg*/ 0,
        /*eofMode*/ 0,
        /*viewMode*/ 0,
        /*radius*/ 740,
        /*fov*/ 165,
        /*angle*/ 27.f,
        /*rotateX*/ 0,
        /*rotateY*/ 0,
        /*rotateZ*/ 0,
        /*translateX*/ 0,
        /*translateY*/ 0,
        /*translateZ*/ 0,
        /*planeWidth*/ 0,
        /*planeHeight*/ 0,
        /*planeElevation*/ 0,
        /*planeElevation*/ 0,
        /*planeConsiderAspectRatio*/ 0,
        /*eqDirty*/ false,
        /*eqContrast*/ 0,
        /*eqBrightness*/ 0,
        /*eqGamma*/ 0,
        /*eqSaturation*/ 0,
        /*loopTimeDirty*/ false,
        /*loopTimeEnabled*/ false,
        /*loopTimeA*/ 0,
        /*loopTimeB*/ 0};

    ConfigurationVariables configuration = {
        /*confAll*/ "./data/mpv-conf/default/all.json",
        /*confMasterOnly*/ "./data/mpv-conf/default/master-only.json",
        /*confNodesOnly*/ "./data/mpv-conf/default/nodes-only.json"};

private:
    static SyncHelper *_instance;
};

#endif // APPLICATION_H
