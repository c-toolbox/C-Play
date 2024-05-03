/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "_debug.h"
#include "application.h"
#include "haction.h"
#include "mpvobject.h"
#include "playercontroller.h"
#include "playlistitem.h"
#include "playlistmodel.h"
#include "tracksmodel.h"
#include "trackballcameracontroller.h"

#include "audiosettings.h"
#include "gridsettings.h"
#include "imagesettings.h"
#include "locationsettings.h"
#include "mousesettings.h"
#include "playbacksettings.h"
#include "playlistsettings.h"
#include "userinterfacesettings.h"

#include "worker.h"
#include <iostream>
#include <sgct/sgct.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QStyle>
#include <QThread>
#include <QQmlEngine>
#include <QMimeDatabase>

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KColorSchemeManager>
#include <KConfig>
#include <KConfigGroup>
#include <KFileMetaData/Properties>
#include <KI18n/KLocalizedString>
#include <KLocalizedString>
#include <KShortcutsDialog>

static QApplication *createApplication(int &argc, char **argv, const QString &applicationName)
{
    Q_INIT_RESOURCE(images);

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setOrganizationName("C-Play");
    QApplication::setApplicationName(applicationName);
    QApplication::setOrganizationDomain("github.com/c-toolbox/C-Play");
    QApplication::setApplicationDisplayName("C-Play : Cluster Video Player");
    QApplication::setApplicationVersion(Application::version());

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));

    QApplication *app = new QApplication(argc, argv);
    app->setWindowIcon(QIcon(":/C_play_transparent.png"));
    return app;
}

Application::Application(int &argc, char **argv, const QString &applicationName)
    : m_app(createApplication(argc, argv, applicationName))
    , m_collection(this)
{
    m_config = KSharedConfig::openConfig("C-Play/cplay.conf");
    m_shortcuts = new KConfigGroup(m_config, "Shortcuts");
    m_schemes = new KColorSchemeManager(this);
    m_systemDefaultStyle = m_app->style()->objectName();

    if (UserInterfaceSettings::useBreezeIconTheme()) {
        QIcon::setThemeName("breeze");
    }

    if (UserInterfaceSettings::guiStyle() != QStringLiteral("System")) {
        QApplication::setStyle(UserInterfaceSettings::guiStyle());
    }

    // Qt sets the locale in the QGuiApplication constructor, but libmpv
    // requires the LC_NUMERIC category to be set to "C", so change it back.
    //std::setlocale(LC_NUMERIC, "C");
    std::locale::global(std::locale("C"));

    setupWorkerThread();
    setupAboutData();
    setupCommandLineParser();
    registerQmlTypes();
    setupQmlSettingsTypes();

    m_engine = new QQmlApplicationEngine(this);
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    auto onObjectCreated = [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    };
    QObject::connect(m_engine, &QQmlApplicationEngine::objectCreated,
                     m_app, onObjectCreated, Qt::QueuedConnection);
    QObject::connect(m_engine, &QQmlApplicationEngine::quit, m_app, &QApplication::quit);
    m_engine->addImportPath("qrc:/qml");

    setupQmlContextProperties();

    m_engine->load(url);

    //QObject::connect(&renderThread, &QThread::finished, m_app, &QApplication::quit);
    //QObject::connect(&renderThread, &QThread::finished, &renderThread, &QThread::deleteLater);
}

Application* Application::_instance = nullptr;

void Application::create(int &argc, char **argv, const QString &applicationName)
{
    _instance = new Application(argc, argv, applicationName);
}

Application& Application::instance() {
    if (_instance == nullptr) {
        throw std::logic_error("Using the instance before it was created or set");
    }
    return *_instance;
}

Application::~Application()
{
    delete m_engine;
    delete _instance;
    _instance = nullptr;
}

int Application::run()
{
    QEventLoop loop;
    connect(&renderThread, &QThread::finished, &loop, &QEventLoop::quit);
    renderThread.render();
    int returnCode = m_app->exec();
    sgct::Log::Info("Qt Application exited");
    renderThread.terminate();
    loop.exec();
    return returnCode;
}

void Application::setupWorkerThread()
{
    auto worker = Worker::instance();
    auto thread = new QThread();
    worker->moveToThread(thread);
    QObject::connect(thread, &QThread::finished,
                     worker, &Worker::deleteLater);
    QObject::connect(thread, &QThread::finished,
                     thread, &QThread::deleteLater);
    thread->start();
}

void Application::setupAboutData()
{
    m_aboutData = KAboutData(QStringLiteral("C-Play"),
                             i18n("C-Play : Cluster Video Player"),
                             Application::version());
    m_aboutData.setShortDescription(i18n("A configurable cluster video player, based on MPV, SGCT and Haruna projects."));
    m_aboutData.setLicense(KAboutLicense::GPL_V3);
    m_aboutData.setCopyrightStatement(i18n("(c) Erik Sundén 2021-2024"));

    m_aboutData.setHomepage(QStringLiteral("https://c-toolbox.github.io/C-Play/"));
    m_aboutData.setBugAddress(QStringLiteral("https://github.com/c-toolbox/C-Play/issues").toUtf8());
    m_aboutData.setDesktopFileName("org.ctoolbox.cplay");

    m_aboutData.addAuthor(i18n("Contact/owner: Erik Sundén"),
                        i18n("Creator of C-Play"),
                        QStringLiteral("eriksunden85@gmail.com"));

    KAboutData::setApplicationData(m_aboutData);
}

void Application::setupCommandLineParser()
{
    QCommandLineParser parser;
    m_aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("file"), i18n("File to open"));
    parser.process(*m_app);
    m_aboutData.processCommandLine(&parser);

    for (auto i = 0; i < parser.positionalArguments().size(); ++i) {
        addArgument(i, parser.positionalArguments().at(i));
    }
}

void Application::registerQmlTypes()
{
    qmlRegisterType<MpvObject>("mpv", 1, 0, "MpvObject");
    qmlRegisterType<TrackballCameraController>("TrackballCameraController", 1, 0, "TrackballCameraController");
    qRegisterMetaType<PlayListModel*>();
    qRegisterMetaType<PlayListItem*>();
    qRegisterMetaType<QAction*>();
    qRegisterMetaType<TracksModel*>();
    qRegisterMetaType<KFileMetaData::PropertyMap>("KFileMetaData::PropertyMap");
}

void Application::setupQmlSettingsTypes()
{
    auto audioProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return AudioSettings::self(); };
    qmlRegisterSingletonType<AudioSettings>("org.ctoolbox.cplay", 1, 0, "AudioSettings", audioProvider);

    auto gridProvider = [](QQmlEngine*, QJSEngine*) -> QObject* { return GridSettings::self(); };
    qmlRegisterSingletonType<GridSettings>("org.ctoolbox.cplay", 1, 0, "GridSettings", gridProvider);

    auto imageProvider = [](QQmlEngine*, QJSEngine*) -> QObject* { return ImageSettings::self(); };
    qmlRegisterSingletonType<ImageSettings>("org.ctoolbox.cplay", 1, 0, "ImageSettings", imageProvider);

    auto locationProvider = [](QQmlEngine*, QJSEngine*) -> QObject* { return LocationSettings::self(); };
    qmlRegisterSingletonType<LocationSettings>("org.ctoolbox.cplay", 1, 0, "LocationSettings", locationProvider);

    auto mouseProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return MouseSettings::self(); };
    qmlRegisterSingletonType<MouseSettings>("org.ctoolbox.cplay", 1, 0, "MouseSettings", mouseProvider);

    auto playbackProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return PlaybackSettings::self(); };
    qmlRegisterSingletonType<PlaybackSettings>("org.ctoolbox.cplay", 1, 0, "PlaybackSettings", playbackProvider);

    auto playlistProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return PlaylistSettings::self(); };
    qmlRegisterSingletonType<PlaylistSettings>("org.ctoolbox.cplay", 1, 0, "PlaylistSettings", playlistProvider);

    auto uiProvider = [](QQmlEngine*, QJSEngine*) -> QObject* { return UserInterfaceSettings::self(); };
    qmlRegisterSingletonType<UserInterfaceSettings>("org.ctoolbox.cplay", 1, 0, "UserInterfaceSettings", uiProvider);
}

void Application::setupQmlContextProperties()
{
    m_engine->rootContext()->setContextProperty(QStringLiteral("app"), this);
    qmlRegisterUncreatableType<Application>("Application", 1, 0, "Application",
                                            QStringLiteral("Application should not be created in QML"));

    m_engine->rootContext()->setContextProperty(QStringLiteral("playerController"), new PlayerController(this));
}

QUrl Application::configFilePath()
{
    auto configPath = QStandardPaths::writableLocation(m_config->locationType());
    auto configFilePath = configPath.append(QStringLiteral("/")).append(m_config->name());
    QUrl url(configFilePath);
    url.setScheme("file");
    return url;
}

QUrl Application::configFolderPath()
{
    auto configPath = QStandardPaths::writableLocation(m_config->locationType());
    auto configFilePath = configPath.append(QStringLiteral("/")).append(m_config->name());
    QFileInfo fileInfo(configFilePath);
    QUrl url(fileInfo.absolutePath());
    url.setScheme("file");
    return url;
}

QString Application::version()
{
    return QStringLiteral("2.0.0");
}

bool Application::hasYoutubeDl()
{
    return !QStandardPaths::findExecutable(QStringLiteral("youtube-dl")).isEmpty();

}

QUrl Application::parentUrl(const QString &path)
{
    QUrl url(path);
    if (!url.isValid()) {
        return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    }
    QFileInfo fileInfo;
    if (url.isLocalFile()) {
        fileInfo.setFile(url.toLocalFile());
    } else {
        fileInfo.setFile(url.toString());
    }
    QUrl parentFolderUrl("file:///" + fileInfo.absolutePath());
    parentFolderUrl.setScheme("file");

    return parentFolderUrl;
}

QUrl Application::pathToUrl(const QString &path)
{
    QString correctedFilePath = path;
    correctedFilePath.replace("file:///", "");
    QUrl url("file:///" + correctedFilePath);
    if (!url.isValid()) {
        return QUrl();
    }
    url.setScheme("file");

    return url;
}

bool Application::isYoutubePlaylist(const QString &path)
{
    return path.contains("youtube.com/playlist?list");
}

QString Application::formatTime(const double time)
{
    QTime t(0, 0, 0);
    QString formattedTime = t.addSecs(static_cast<qint64>(time)).toString("hh:mm:ss");
    return formattedTime;
}

void Application::hideCursor()
{
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

void Application::showCursor()
{
    QApplication::setOverrideCursor(Qt::ArrowCursor);
}

int Application::getFadeDurationCurrentTime(bool restart) {
    if (restart) {
        fadeDurationTimer.start();
        return 0;
    }
    else {
        return fadeDurationTimer.elapsed();
    }
}

int Application::getFadeDurationSetting() {
    return PlaybackSettings::fadeDuration();
}

QString Application::argument(int key)
{
    return m_args[key];
}

void Application::addArgument(int key, const QString &value)
{
    m_args.insert(key, value);
}

QAction *Application::action(const QString &name)
{
    auto resultAction = m_collection.action(name);

    if (!resultAction) {
        setupActions(name);
        resultAction = m_collection.action(name);
    }

    return resultAction;
}

QString Application::getFileContent(const QString &file)
{
    QFile f(file);
    f.open(QIODevice::ReadOnly);
    QString content = f.readAll();
    f.close();
    return content;
}

QString Application::mimeType(const QString &file)
{
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(file);
    return mime.name();
}

QStringList Application::availableGuiStyles()
{
    return QStyleFactory::keys();
}

void Application::setGuiStyle(const QString &style)
{
    if (style == "Default") {
        QApplication::setStyle(m_systemDefaultStyle);
        return;
    }
    QApplication::setStyle(style);
}

QAbstractItemModel *Application::colorSchemesModel()
{
    return m_schemes->model();
}

void Application::activateColorScheme(const QString &name)
{
    m_schemes->activateScheme(m_schemes->indexForScheme(name));
}

void Application::configureShortcuts()
{
    KShortcutsDialog dlg(KShortcutsEditor::ApplicationAction, KShortcutsEditor::LetterShortcutsAllowed, nullptr);
    connect(&dlg, &KShortcutsDialog::accepted, this, [ = ](){
        m_collection.writeSettings(m_shortcuts);
        m_config->sync();
    });
    dlg.setModal(true);
    dlg.addCollection(&m_collection);
    dlg.configure(false);
}

void Application::aboutApplication()
{
    static QPointer<QDialog> dialog;
    if (!dialog) {
        dialog = new KAboutApplicationDialog(KAboutData::applicationData(), KAboutApplicationDialog::Option::HideLibraries);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    dialog->show();
}

void Application::setupActions(const QString &actionName)
{
    if (actionName == QStringLiteral("play_pause")) {
        auto action = new HAction();
        action->setText(i18n("Play/Pause"));
        action->setIcon(QIcon::fromTheme("media-playback-pause"));
        m_collection.setDefaultShortcut(action, Qt::Key_Space);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("stop_rewind")) {
        auto action = new HAction();
        action->setText(i18n("Stop/Rewind"));
        action->setIcon(QIcon::fromTheme("media-playback-stop"));
        m_collection.setDefaultShortcut(action, Qt::Key_Backspace);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("file_quit")) {
        auto action = new HAction();
        action->setText(i18n("Quit"));
        action->setIcon(QIcon::fromTheme("application-exit"));
        connect(action, &QAction::triggered, m_engine, &QQmlApplicationEngine::quit);
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Q"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("options_configure_keybinding")) {
        auto action = new HAction();
        action->setText(i18n("Configure Keyboard Shortcuts"));
        action->setIcon(QIcon::fromTheme("configure-shortcuts"));
        connect(action, &QAction::triggered, this, &Application::configureShortcuts);
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Shift+K"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("configure")) {
        auto action = new HAction();
        action->setText(i18n("Configure"));
        action->setIcon(QIcon::fromTheme("configure"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Shift+C"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("togglePlaylist")) {
        auto action = new HAction();
        action->setText(i18n("Playlist"));
        action->setIcon(QIcon::fromTheme("view-media-playlist"));
        m_collection.setDefaultShortcut(action, Qt::Key_P);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleSections")) {
        auto action = new HAction();
        action->setText(i18n("Sections"));
        action->setIcon(QIcon::fromTheme("office-chart-pie"));
        m_collection.setDefaultShortcut(action, Qt::Key_S);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("openFile")) {
        auto action = new HAction();
        action->setText(i18n("Open File"));
        action->setIcon(QIcon::fromTheme("folder-videos-symbolic"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+O"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("saveAsCPlayFile")) {
        auto action = new HAction();
        action->setText(i18n("Save As C-Play file"));
        action->setIcon(QIcon::fromTheme("document-save-as"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+S"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("aboutCPlay")) {
        auto action = new HAction();
        action->setText(i18n("About C-Play"));
        action->setIcon(QIcon::fromTheme("help-about-symbolic"));
        m_collection.addAction(actionName, action);
        connect(action, &QAction::triggered, this, &Application::aboutApplication);
    }
    if (actionName == QStringLiteral("playNext")) {
        auto action = new HAction();
        action->setText(i18n("Play Next"));
        action->setIcon(QIcon::fromTheme("media-skip-forward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+N"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("playPrevious")) {
        auto action = new HAction();
        action->setText(i18n("Play Previous"));
        action->setIcon(QIcon::fromTheme("media-skip-backward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+B"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeUp")) {
        auto action = new HAction();
        action->setText(i18n("Volume Up"));
        action->setIcon(QIcon::fromTheme("audio-volume-high"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift++"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeDown")) {
        auto action = new HAction();
        action->setText(i18n("Volume Down"));
        action->setIcon(QIcon::fromTheme("audio-volume-low"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+-"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeFadeUp")) {
        auto action = new HAction();
        action->setText(i18n("Volume Fade Up"));
        action->setIcon(QIcon::fromTheme("audio-volume-high"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+Up"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeFadeDown")) {
        auto action = new HAction();
        action->setText(i18n("Volume Fade Down"));
        action->setIcon(QIcon::fromTheme("audio-volume-low"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+Down"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("mute")) {
        auto action = new HAction();
        action->setText(i18n("Volume Mute"));
        action->setIcon(QIcon::fromTheme("audio-on"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+M"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("visibilityFadeUp")) {
        auto action = new HAction();
        action->setText(i18n("Visibility Fade Up"));
        action->setIcon(QIcon::fromTheme("view-visible"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+PgUp"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("visibilityFadeDown")) {
        auto action = new HAction();
        action->setText(i18n("Visibility Fade Down"));
        action->setIcon(QIcon::fromTheme("view-hidden"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+PgDown"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekForwardSmall")) {
        auto action = new HAction();
        action->setText(i18n("Seek Small Step Forward"));
        action->setIcon(QIcon::fromTheme("media-seek-forward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+Right"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekBackwardSmall")) {
        auto action = new HAction();
        action->setText(i18n("Seek Small Step Backward"));
        action->setIcon(QIcon::fromTheme("media-seek-backward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Shift+Left"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekForwardMedium")) {
        auto action = new HAction();
        action->setText(i18n("Seek Medium Step Forward"));
        action->setIcon(QIcon::fromTheme("media-seek-forward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Right"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekBackwardMedium")) {
        auto action = new HAction();
        action->setText(i18n("Seek Medium Step Backward"));
        action->setIcon(QIcon::fromTheme("media-seek-backward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Left"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekForwardBig")) {
        auto action = new HAction();
        action->setText(i18n("Seek Big Step Forward"));
        action->setIcon(QIcon::fromTheme("media-seek-forward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Shift+Right"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekBackwardBig")) {
        auto action = new HAction();
        action->setText(i18n("Seek Big Step Backward"));
        action->setIcon(QIcon::fromTheme("media-seek-backward"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Shift+Left"));
        m_collection.addAction(actionName, action);
    } 
    if (actionName == QStringLiteral("toggleMenuBar")) {
        auto action = new HAction();
        action->setText(i18n("Toggle Menu Bar"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Shift+M"));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleHeader")) {
        auto action = new HAction();
        action->setText(i18n("Toggle Header"));
        m_collection.setDefaultShortcut(action, QKeySequence("Ctrl+Shift+H"));
        m_collection.addAction(actionName, action);
    }
    m_collection.readSettings(m_shortcuts);
}


SyncHelper* SyncHelper::_instance = nullptr;

SyncHelper::SyncHelper() {}

SyncHelper::~SyncHelper()
{
    delete _instance;
    _instance = nullptr;
}

SyncHelper& SyncHelper::instance() {
    if(!_instance){
        _instance = new SyncHelper();
    }
    return *_instance;
}

