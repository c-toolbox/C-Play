/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "application.h"
#include "_debug.h"
#include "haction.h"
#include "layerqtitem.h"
#include "layersmodel.h"
#include "mpvobject.h"
#include "playercontroller.h"
#include "playlistitem.h"
#include "playlistmodel.h"
#include "slidesmodel.h"
#include "slidesqtitem.h"
#include "tracksmodel.h"

#include "audiosettings.h"
#include "gridsettings.h"
#include "imagesettings.h"
#include "locationsettings.h"
#include "mousesettings.h"
#include "playbacksettings.h"
#include "playlistsettings.h"
#include "presentationsettings.h"
#include "userinterfacesettings.h"

#include "worker.h"
#include <iostream>
#include <sgct/sgct.h>
#include <sgct/version.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeDatabase>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QThread>
#include <QtGlobal>

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KColorSchemeManager>
#include <KConfig>
#include <KConfigGroup>
#include <KFileItem>
#include <KFileMetaData/Properties>
#include <KI18n/KLocalizedString>
#include <KLocalizedString>
#include <KShortcutsDialog>

static QApplication *createApplication(int &argc, char **argv, const QString &applicationName) {
    Q_INIT_RESOURCE(images);

    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setOrganizationName(QStringLiteral("C-Play"));
    QApplication::setApplicationName(applicationName);
    QApplication::setOrganizationDomain(QStringLiteral("github.com/c-toolbox/C-Play"));
    QApplication::setApplicationDisplayName(QStringLiteral("C-Play"));
    QApplication::setApplicationVersion(Application::version());

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#else
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    QApplication *app = new QApplication(argc, argv);
    app->setWindowIcon(QIcon(QStringLiteral(":/C_play_transparent.png")));
    return app;
}

Application::Application(int &argc, char **argv, const QString &applicationName)
    : m_app(createApplication(argc, argv, applicationName)), m_collection(this), m_slidesModel(new SlidesModel(this)) {
    m_config = KSharedConfig::openConfig(QStringLiteral("C-Play/cplay.conf"));
    m_shortcuts = new KConfigGroup(m_config, QStringLiteral("Shortcuts"));
    m_schemes = new KColorSchemeManager(this);
    m_systemDefaultStyle = m_app->style()->objectName();

    if (UserInterfaceSettings::useBreezeIconTheme()) {
        QIcon::setThemeName(QStringLiteral("breeze"));
    }

    if (UserInterfaceSettings::guiStyle() != QStringLiteral("System")) {
        QApplication::setStyle(UserInterfaceSettings::guiStyle());
    }

    // Qt sets the locale in the QGuiApplication constructor, but libmpv
    // requires the LC_NUMERIC category to be set to "C", so change it back.
    // std::setlocale(LC_NUMERIC, "C");
    std::locale::global(std::locale("C"));

    setupWorkerThread();
    setupAboutData();
    setupCommandLineParser();
    registerQmlTypes();
    setupQmlSettingsTypes();

    m_engine = new QQmlApplicationEngine(this);
    QObject::connect(m_engine, &QQmlApplicationEngine::quit, m_app, &QApplication::quit);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    setupQmlContextProperties();

    m_engine->loadFromModule("org.ctoolbox.cplay", "Main");
#else
    const QUrl url(QStringLiteral("qrc:/qt5/main.qml"));
    auto onObjectCreated = [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    };
    QObject::connect(m_engine, &QQmlApplicationEngine::objectCreated,
                     m_app, onObjectCreated, Qt::QueuedConnection);
    m_engine->addImportPath(QStringLiteral("qrc:/qt5"));

    setupQmlContextProperties();

    m_engine->load(url);
#endif
}

Application *Application::_instance = nullptr;

void Application::create(int &argc, char **argv, const QString &applicationName) {
    _instance = new Application(argc, argv, applicationName);
}

Application &Application::instance() {
    if (_instance == nullptr) {
        throw std::logic_error("Using the instance before it was created or set");
    }
    return *_instance;
}

Application::~Application() {
    delete m_engine;
    delete _instance;
    _instance = nullptr;
}

int Application::run() {
    QEventLoop loop;
    connect(&renderThread, &QThread::finished, &loop, &QEventLoop::quit);
    renderThread.render();
    int returnCode = m_app->exec();
    sgct::Log::Info("Qt Application exited");
    renderThread.terminate();
    loop.exec();
    return returnCode;
}

void Application::setupWorkerThread() {
    auto worker = Worker::instance();
    auto thread = new QThread();
    worker->moveToThread(thread);
    QObject::connect(thread, &QThread::finished,
                     worker, &Worker::deleteLater);
    QObject::connect(thread, &QThread::finished,
                     thread, &QThread::deleteLater);
    thread->start();
}

void Application::setupAboutData() {
    m_aboutData = KAboutData(QStringLiteral("C-Play"),
                             QStringLiteral("C-Play"),
                             Application::version());
    m_aboutData.setShortDescription(QStringLiteral("A media player for immersive content and cluster environments."));
    m_aboutData.setLicense(KAboutLicense::GPL_V3);
    m_aboutData.setCopyrightStatement(QString::fromUtf8("(c) Erik Sundén 2021-2024"));

    m_aboutData.setHomepage(QStringLiteral("https://c-toolbox.github.io/C-Play/"));
    m_aboutData.setBugAddress(QStringLiteral("https://github.com/c-toolbox/C-Play/issues").toUtf8());
    m_aboutData.setDesktopFileName(QStringLiteral("org.ctoolbox.cplay"));

    m_aboutData.addAuthor(QString::fromUtf8("Contact/owner: Erik Sundén"),
                          QStringLiteral("Creator of C-Play"),
                          QStringLiteral("eriksunden85@gmail.com"));

    KAboutData::setApplicationData(m_aboutData);
}

void Application::setupCommandLineParser() {
    QCommandLineParser parser;
    m_aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("File to open"));
    parser.process(*m_app);
    m_aboutData.processCommandLine(&parser);

    for (auto i = 0; i < parser.positionalArguments().size(); ++i) {
        addArgument(i, parser.positionalArguments().at(i));
    }
}

void Application::registerQmlTypes() {
    qmlRegisterType<MpvObject>("org.ctoolbox.cplay", 1, 0, "MpvObject");
    qmlRegisterType<LayerQtItem>("org.ctoolbox.cplay", 1, 0, "LayerQtItem");
    qmlRegisterType<SlidesQtItem>("org.ctoolbox.cplay", 1, 0, "SlidesQtItem");

    qRegisterMetaType<PlayListModel *>();
    qRegisterMetaType<PlayListItem *>();
    qRegisterMetaType<QAction *>();
    qRegisterMetaType<TracksModel *>();
    qRegisterMetaType<LayersTypeModel *>();
#ifdef NDI_SUPPORT
    qRegisterMetaType<NDISendersModel *>();
#endif
    qRegisterMetaType<LayersModel *>();
    qRegisterMetaType<SlidesModel *>();
    qRegisterMetaType<KFileMetaData::PropertyMultiMap>("KFileMetaData::PropertyMultiMap");
}

void Application::setupQmlSettingsTypes() {
    auto audioProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return AudioSettings::self(); };
    qmlRegisterSingletonType<AudioSettings>("org.ctoolbox.cplay", 1, 0, "AudioSettings", audioProvider);

    auto gridProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return GridSettings::self(); };
    qmlRegisterSingletonType<GridSettings>("org.ctoolbox.cplay", 1, 0, "GridSettings", gridProvider);

    auto imageProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return ImageSettings::self(); };
    qmlRegisterSingletonType<ImageSettings>("org.ctoolbox.cplay", 1, 0, "ImageSettings", imageProvider);

    auto layerProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return PresentationSettings::self(); };
    qmlRegisterSingletonType<LocationSettings>("org.ctoolbox.cplay", 1, 0, "PresentationSettings", layerProvider);

    auto locationProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return LocationSettings::self(); };
    qmlRegisterSingletonType<LocationSettings>("org.ctoolbox.cplay", 1, 0, "LocationSettings", locationProvider);

    auto mouseProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return MouseSettings::self(); };
    qmlRegisterSingletonType<MouseSettings>("org.ctoolbox.cplay", 1, 0, "MouseSettings", mouseProvider);

    auto playbackProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return PlaybackSettings::self(); };
    qmlRegisterSingletonType<PlaybackSettings>("org.ctoolbox.cplay", 1, 0, "PlaybackSettings", playbackProvider);

    auto playlistProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return PlaylistSettings::self(); };
    qmlRegisterSingletonType<PlaylistSettings>("org.ctoolbox.cplay", 1, 0, "PlaylistSettings", playlistProvider);

    auto uiProvider = [](QQmlEngine *, QJSEngine *) -> QObject * { return UserInterfaceSettings::self(); };
    qmlRegisterSingletonType<UserInterfaceSettings>("org.ctoolbox.cplay", 1, 0, "UserInterfaceSettings", uiProvider);
}

void Application::setupQmlContextProperties() {
    m_engine->rootContext()->setContextProperty(QStringLiteral("app"), this);
    qmlRegisterUncreatableType<Application>("Application", 1, 0, "Application",
                                            QStringLiteral("Application should not be created in QML"));

    m_engine->rootContext()->setContextProperty(QStringLiteral("playerController"), new PlayerController(this));
}

QUrl Application::configFilePath() {
    auto configPath = QStandardPaths::writableLocation(m_config->locationType());
    auto configFilePath = configPath.append(QStringLiteral("/")).append(m_config->name());
    QUrl url(configFilePath);
    url.setScheme(QStringLiteral("file"));
    return url;
}

QUrl Application::configFolderPath() {
    auto configPath = QStandardPaths::writableLocation(m_config->locationType());
    auto configFilePath = configPath.append(QStringLiteral("/")).append(m_config->name());
    QFileInfo fileInfo(configFilePath);
    QUrl url(fileInfo.absolutePath());
    url.setScheme(QStringLiteral("file"));
    return url;
}

QString Application::version() {
    return QStringLiteral(CPLAY_VERSION);
}

bool Application::hasYoutubeDl() {
    return !QStandardPaths::findExecutable(QStringLiteral("youtube-dl")).isEmpty();
}

QUrl Application::parentUrl(const QString &path) {
    QUrl url(path);
    if (!url.isValid()) {
        return QUrl(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    }
    QFileInfo fileInfo;
    if (url.isLocalFile()) {
        fileInfo.setFile(url.toLocalFile());
    } else {
        fileInfo.setFile(url.toString());
    }
    QUrl parentFolderUrl(QStringLiteral("file:///") + fileInfo.absolutePath());
    parentFolderUrl.setScheme(QStringLiteral("file"));

    return parentFolderUrl;
}

QUrl Application::pathToUrl(const QString &path) {
    QString correctedFilePath = path;
    correctedFilePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QUrl url(QStringLiteral("file:///") + correctedFilePath);
    if (!url.isValid()) {
        return QUrl();
    }
    url.setScheme(QStringLiteral("file"));

    return url;
}

bool Application::isYoutubePlaylist(const QString &path) {
    return path.contains(QStringLiteral("youtube.com/playlist?list"));
}

QString Application::formatTime(const double time) {
    QTime t(0, 0, 0);
    QString formattedTime = t.addSecs(static_cast<qint64>(time)).toString(QStringLiteral("hh:mm:ss"));
    return formattedTime;
}

void Application::hideCursor() {
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

void Application::showCursor() {
    QApplication::setOverrideCursor(Qt::ArrowCursor);
}

int Application::getFadeDurationCurrentTime(bool restart) {
    if (restart) {
        fadeDurationTimer.start();
        return 0;
    } else {
        return fadeDurationTimer.elapsed();
    }
}

int Application::getFadeDurationSetting() {
    return PlaybackSettings::fadeDuration();
}

void Application::setStartupFile(std::string filePath) {
    m_startupFileFromCmd = QString::fromStdString(filePath);
}

SlidesModel *Application::slidesModel() {
    return m_slidesModel;
}

QString Application::argument(int key) {
    return m_args[key];
}

void Application::addArgument(int key, const QString &value) {
    m_args.insert(key, value);
}

QAction *Application::action(const QString &name) {
    auto resultAction = m_collection.action(name);

    if (!resultAction) {
        setupActions(name);
        resultAction = m_collection.action(name);
    }

    return resultAction;
}

QString Application::getFileContent(const QString &file) {
    QFile f(file);
    f.open(QIODevice::ReadOnly);
    QString content = QString::fromUtf8(f.readAll());
    f.close();
    return content;
}

QString Application::mimeType(QUrl url) {
    KFileItem fileItem(url, KFileItem::NormalMimeTypeDetermination);
    return fileItem.mimetype();
}

QStringList Application::availableGuiStyles() {
    return QStyleFactory::keys();
}

void Application::setGuiStyle(const QString &style) {
    if (style == QStringLiteral("Default")) {
        QApplication::setStyle(m_systemDefaultStyle);
        return;
    }
    QApplication::setStyle(style);
}

QAbstractItemModel *Application::colorSchemesModel() {
    return m_schemes->model();
}

void Application::activateColorScheme(const QString &name) {
    m_schemes->activateScheme(m_schemes->indexForScheme(name));
}

void Application::configureShortcuts() {
    KShortcutsDialog dlg(KShortcutsEditor::ApplicationAction, KShortcutsEditor::LetterShortcutsAllowed, nullptr);
    connect(&dlg, &KShortcutsDialog::accepted, this, [=]() {
        m_collection.writeSettings(m_shortcuts);
        m_config->sync();
    });
    dlg.setModal(true);
    dlg.addCollection(&m_collection);
    dlg.configure(false);
}

void Application::updateAboutOtherText(const QString &mpvVersion, const QString &ffmpegVersion) {
    QStringList ffmpeg_version_num = ffmpegVersion.split(QStringLiteral("-"), Qt::SkipEmptyParts);
    QString ffmpeg_version_clean;
    for (const QChar c : std::as_const(ffmpeg_version_num[0])) {
        if (!c.isLetter()) {
            ffmpeg_version_clean.append(c);
        }
    }
    QStringList mpv_version_num = mpvVersion.split(QStringLiteral("-"), Qt::SkipEmptyParts);
    QString mpv_version_clean;
    for (const QChar c : std::as_const(mpv_version_num[0])) {
        if (!c.isLetter()) {
            mpv_version_clean.append(c);
        }
    }
    QString otherText;
    otherText += QStringLiteral("Using media playback engine MPV ") + mpv_version_clean + QStringLiteral(" with FFmpeg ") + ffmpeg_version_clean + QStringLiteral(".\n");
    otherText += QStringLiteral("Master UI compiled with Qt ") + QStringLiteral(QT_VERSION_STR) + QStringLiteral(" and based on Haruna project.\n");
    otherText += QStringLiteral("SGCT ") + QString::fromStdString(std::string(sgct::Version)) + QStringLiteral(" for cluster environment and client rendering.");
    m_aboutData.setOtherText(otherText);
    KAboutData::setApplicationData(m_aboutData);
}

QString Application::getStartupFile() {
    if (m_startupFileFromCmd.isEmpty())
        return PlaylistSettings::playlistToLoadOnStartup();
    else
        return m_startupFileFromCmd;
}

void Application::aboutApplication() {
    static QPointer<QDialog> dialog;
    if (!dialog) {
        dialog = new KAboutApplicationDialog(KAboutData::applicationData(), KAboutApplicationDialog::Option::HideLibraries);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    dialog->show();
}

void Application::setupActions(const QString &actionName) {
    if (actionName == QStringLiteral("play_pause")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Play/Pause"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        m_collection.setDefaultShortcut(action, Qt::Key_Space);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("stop_rewind")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Stop/Rewind"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
        m_collection.setDefaultShortcut(action, Qt::Key_Backspace);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("file_quit")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Quit"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
        connect(action, &QAction::triggered, m_engine, &QQmlApplicationEngine::quit);
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Q")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("options_configure_keybinding")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Configure Keyboard Shortcuts"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
        connect(action, &QAction::triggered, this, &Application::configureShortcuts);
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+K")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("configure")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Configure"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+C")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("togglePlaylist")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Playlist"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("format-list-unordered")));
        m_collection.setDefaultShortcut(action, Qt::Key_P);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleSections")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Sections"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("drive-partition")));
        m_collection.setDefaultShortcut(action, Qt::Key_S);
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleSlides")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Slides"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+S")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleLayers")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Layers"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("dialog-layers")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+L")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("openFile")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Open File"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("folder-videos-symbolic")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+O")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("saveAsCPlayFile")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Save As C-Play file"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+S")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("aboutCPlay")) {
        auto action = new HAction();
        action->setText(QStringLiteral("About C-Play"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("help-about-symbolic")));
        m_collection.addAction(actionName, action);
        connect(action, &QAction::triggered, this, &Application::aboutApplication);
    }
    if (actionName == QStringLiteral("playNext")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Play Next"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-skip-forward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+N")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("playPrevious")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Play Previous"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-skip-backward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+B")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeUp")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Volume Up"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift++")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeDown")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Volume Down"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-low")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+-")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeFadeUp")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Volume Fade Up"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+Up")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("volumeFadeDown")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Volume Fade Down"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-low")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+Down")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("mute")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Volume Mute"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("audio-on")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+M")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("visibilityFadeUp")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Visibility Fade Up"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("view-visible")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+PgUp")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("visibilityFadeDown")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Visibility Fade Down"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("view-hidden")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+PgDown")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekForwardSmall")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Seek Small Step Forward"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-seek-forward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+Right")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekBackwardSmall")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Seek Small Step Backward"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-seek-backward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Shift+Left")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekForwardMedium")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Seek Medium Step Forward"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-seek-forward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Right")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekBackwardMedium")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Seek Medium Step Backward"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-seek-backward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Left")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekForwardBig")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Seek Big Step Forward"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-seek-forward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+Right")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("seekBackwardBig")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Seek Big Step Backward"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("media-seek-backward")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+Left")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleMenuBar")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Toggle Menu Bar"));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+M")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("toggleHeader")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Toggle Header"));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+H")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("slidePrevious")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Slide backwards/previous"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
        m_collection.setDefaultShortcut(action, QKeySequence(QKeySequence::MoveToPreviousChar));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("slideNext")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Slide forwards/next"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
        m_collection.setDefaultShortcut(action, QKeySequence(QKeySequence::MoveToNextChar));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("layerCopy")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Copy Layer"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+C")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("layerPaste")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Paste Layer"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
        action->setEnabled(false);
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+V")));
        m_collection.addAction(actionName, action);
    }
    if (actionName == QStringLiteral("layerPasteProperties")) {
        auto action = new HAction();
        action->setText(QStringLiteral("Paste Properties onto Layer"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
        action->setEnabled(false);
        m_collection.setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+V")));
        m_collection.addAction(actionName, action);
    }
    m_collection.readSettings(m_shortcuts);
}

SyncHelper *SyncHelper::_instance = nullptr;

SyncHelper::SyncHelper() {}

SyncHelper::~SyncHelper() {
    delete _instance;
    _instance = nullptr;
}

SyncHelper &SyncHelper::instance() {
    if (!_instance) {
        _instance = new SyncHelper();
    }
    return *_instance;
}
