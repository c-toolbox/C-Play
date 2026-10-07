#include "playercontroller.h"
#include "_debug.h"
#include "application.h"
#include "configmodel.h"
#include "httpserverthread.h"
#include "imagesettings.h"
#include "loggingsettings.h"
#include "locationsettings.h"
#include "mpvobject.h"
#include "playbacksettings.h"
#include "slidesmodel.h"
#include "presentationsettings.h"
#include "telemetry/nodetelemetry.h"
#include "telemetry/nodeloaderverifier.h"
#include "userinterfacesettings.h"
#include "layers/controllayer.h"
#include "layersmodel.h"
#include "playlist/playlistmodel.h"
#include "tracksmodel.h"
#include "layers/imagelayer.h"
#include "utils/imagesequenceutils.h"
#include "tcpcontrolsettings.h"
#include "tcpcontrolcommandadapter.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QRegularExpression>

#ifdef SAIL_SUPPORT
#include <sail-c++/sail-c++.h>
#endif

#include <thread>

namespace {
QString formatMemoryBudgetText(std::uint64_t bytes) {
    const double bytesPerMb = 1024.0 * 1024.0;
    const double bytesPerGb = bytesPerMb * 1024.0;
    const double budgetGb = static_cast<double>(bytes) / bytesPerGb;
    if (budgetGb >= 1.0) {
        return QObject::tr("%1 GB").arg(budgetGb, 0, 'f', 2);
    }

    const double budgetMb = static_cast<double>(bytes) / bytesPerMb;
    return QObject::tr("%1 MB").arg(budgetMb, 0, 'f', 0);
}
}

#pragma warning(disable : 4996)

PlayerController::PlayerController(QObject *parent)
    : QObject(parent), 
    m_mpv(nullptr),
    m_slidesModel(nullptr),
    httpServer(new HttpServerThread(this)),
    m_backgroundFile(QStringLiteral("")),
    m_foregroundFile(QStringLiteral("")),
    m_viewModeOnMaster(0)
{
    connect(this, &PlayerController::mpvChanged,
            this, &PlayerController::setupConnections);

    // Per-node telemetry rows are pushed from SGCT's network thread; the manager emits
    // telemetryChanged on the GUI thread (queued), which we forward to QML.
    connect(&NodeTelemetryManager::instance(), &NodeTelemetryManager::telemetryChanged,
            this, &PlayerController::nodeTelemetryChanged);

    // Node load-failure rows are pushed from SGCT's network thread; the verifier emits
    // loadFailuresChanged on the GUI thread (queued), which we forward to QML.
    connect(&NodeLoaderVerifier::instance(), &NodeLoaderVerifier::loadFailuresChanged,
            this, &PlayerController::nodeLoadFailuresChanged);

    setupHttpServer();

    auto *tcpManager = Application::instance().tcpControlManager();
    auto *tcpAdapter = new TcpControlCommandAdapter(this);
    connect(tcpManager, &TcpControlManager::controlMessageReceived, tcpAdapter,
        [tcpAdapter](const QString &, const QByteArray &payload) { tcpAdapter->parseMessage(payload); });
    const auto tcpError = [this](const QString &error) {
        m_tcpControlError = error;
        Q_EMIT tcpControlErrorChanged();
        qWarning().noquote() << "TCP control:" << error;
    };
    connect(tcpAdapter, &TcpControlCommandAdapter::protocolError, this, tcpError);
    connect(tcpManager, &TcpControlManager::serverError, this,
        [tcpError](const QString &id, const QString &error) { tcpError(id + QStringLiteral(": ") + error); });
    connect(tcpAdapter, &TcpControlCommandAdapter::commandReceived, this,
            [this, tcpError](const QString &operation, const QString &parameter) {
        if (!ConfigModel::instance().isMaster() || !m_mpv || !m_slidesModel) {
            tcpError(tr("TCP command rejected: player is not ready"));
            return;
        }
        m_tcpControlError.clear();
        Q_EMIT tcpControlErrorChanged();
        QString dispatchParameter = parameter;
        if (operation == QStringLiteral("LoadFromAudioTracks") || operation == QStringLiteral("LoadFromPlaylist")
            || operation == QStringLiteral("LoadFromSections") || operation == QStringLiteral("LoadFromSlides")) {
            int index = -1;
            if (!resolveTcpControlSelection(operation, parameter, index)) {
                tcpError(tr("TCP selection does not exist: %1").arg(parameter));
                return;
            }
            dispatchParameter = QString::number(index);
        }
        DispatchControlOperation(operation, dispatchParameter);
    });
    connect(this, &PlayerController::mpvChanged, this, &PlayerController::applyTcpControlSettings);
    connect(this, &PlayerController::slidesModelChanged, this, &PlayerController::applyTcpControlSettings);
    connect(&ConfigModel::instance(), &ConfigModel::runtimeRoleChanged,
            this, &PlayerController::applyTcpControlSettings);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, tcpManager,
            [tcpManager] { tcpManager->setActive(false); });

    // Migrate the original single-server settings once. Never replace an existing
    // multi-server file (including a malformed file that the operator must repair).
    if (!QFileInfo::exists(tcpManager->configurationPath()) && TcpControlSettings::enabled()) {
        QVariantMap legacy {
            {QStringLiteral("name"), tr("Legacy TCP control")}, {QStringLiteral("host"), TcpControlSettings::host()},
            {QStringLiteral("port"), TcpControlSettings::port()}, {QStringLiteral("enabled"), true}, {QStringLiteral("acceptControlCommands"), true},
            {QStringLiteral("autoReconnect"), TcpControlSettings::autoReconnect()},
            {QStringLiteral("connectTimeoutMs"), TcpControlSettings::connectTimeoutMs()},
            {QStringLiteral("retryInitialMs"), TcpControlSettings::retryInitialMs()},
            {QStringLiteral("retryMaximumMs"), TcpControlSettings::retryMaximumMs()},
            {QStringLiteral("stableConnectionMs"), TcpControlSettings::stableConnectionMs()},
            {QStringLiteral("framing"), TcpControlSettings::framingMode()},
            {QStringLiteral("receiveTerminatorHex"), TcpControlSettings::receiveTerminatorHex()},
            {QStringLiteral("sendTerminatorHex"), TcpControlSettings::sendTerminatorHex()},
            {QStringLiteral("maximumMessageBytes"), TcpControlSettings::maximumMessageBytes()},
            {QStringLiteral("maximumPendingWriteBytes"), TcpControlSettings::maximumPendingWriteBytes()}
        };
        tcpManager->saveServer(legacy);
    } else if (!QFileInfo::exists(tcpManager->configurationPath())
        && QFileInfo::exists(QStringLiteral("./data/predefined-tcp-control.json"))) {
        tcpManager->loadConfiguration(QStringLiteral("./data/predefined-tcp-control.json"));
    }
    setBackgroundImageFile(ImageSettings::imageToLoadAsBackground());
    setBackgroundGridMode(ImageSettings::gridToMapOnForBackground());
    setBackgroundStereoMode(ImageSettings::stereoModeForBackground());

    setForegroundImageFile(ImageSettings::imageToLoadAsForeground());
    setForegroundGridMode(ImageSettings::gridToMapOnForForeground());
    setForegroundStereoMode(ImageSettings::stereoModeForForeground());

    applyNoSignalImages();

    setRewindMediaOnEOF(PlaybackSettings::rewindOnEOFwhenPause());

    setNodeWindowsOnTop(UserInterfaceSettings::windowOnTopAtStartup());
    setNodeWindowOpacityContentBased(UserInterfaceSettings::windowOpacityContentBased());

    // Restore the persisted node-telemetry toggle (master only; the manager ignores it on nodes).
    NodeTelemetryManager::instance().setIntervalMs(LoggingSettings::nodeTelemetryInterval());
    if (LoggingSettings::nodeTelemetryEnabled())
        NodeTelemetryManager::instance().setEnabled(true);

    // Set up the Control layer dispatch callback
    ControlLayer::setDispatchCallback([this](const std::string& operation, const std::string& parameter) {
        DispatchControlOperation(QString::fromStdString(operation), QString::fromStdString(parameter));
    });
}

void PlayerController::applyTcpControlSettings() {
    Application::instance().tcpControlManager()->setActive(
        ConfigModel::instance().isMaster() && m_mpv && m_slidesModel);
}

bool PlayerController::resolveTcpControlSelection(const QString &operation, const QString &parameter, int &index) const {
    QList<QStringList> names;
    if (operation == QStringLiteral("LoadFromAudioTracks")) {
        auto *model = m_mpv->audioTracksModel();
        if (!model)
            return false;
        for (int i = 0; i < model->rowCount(); ++i)
            names.append(QStringList { model->data(model->index(i, 0), Qt::DisplayRole).toString() });
    } else if (operation == QStringLiteral("LoadFromPlaylist")) {
        auto *model = m_mpv->playlistModel();
        if (!model)
            return false;
        for (int i = 0; i < model->getPlayListSize(); ++i)
            names.append(QStringList { model->mediaTitle(i), model->listTitle(i), model->fileName(i) });
    } else if (operation == QStringLiteral("LoadFromSections")) {
        auto *model = m_mpv->playSectionsModel();
        if (!model)
            return false;
        for (int i = 0; i < model->getNumberOfSections(); ++i)
            names.append(QStringList { model->sectionTitle(i) });
    } else if (operation == QStringLiteral("LoadFromSlides")) {
        for (int i = 0; i < m_slidesModel->numberOfSlides(); ++i) {
            auto *slide = m_slidesModel->slide(i);
            names.append(slide ? QStringList { slide->getLayersName() } : QStringList());
        }
    } else {
        return false;
    }
    bool numeric = false;
    index = parameter.toInt(&numeric);
    if (numeric)
        return index >= 0 && index < names.size() && !names.at(index).isEmpty();
    for (int i = 0; i < names.size(); ++i) {
        if (names.at(i).contains(parameter)) {
            index = i;
            return true;
        }
    }
    return false;
}

void PlayerController::setupConnections() {
    if (!m_mpv) {
        return;
    }

    connect(m_mpv, &MpvObject::fileLoaded, this, &PlayerController::backgroundVisibilityChanged);
    connect(m_mpv, &MpvObject::visibilityChanged, this, &PlayerController::backgroundVisibilityChanged);
    connect(this, &PlayerController::backgroundImageChanged, this, &PlayerController::backgroundVisibilityChanged);

    // When the main video's mute state changes (e.g. via the mute action), all audio
    // sources in layers need to be muted/unmuted as well.
    connect(m_mpv, &MpvObject::muteChanged, this, [this]() {
        if (m_slidesModel) {
            m_slidesModel->runUpdateMuteOnLayers(m_mpv->mute());
        }
    });

    Q_EMIT backgroundVisibilityChanged();
}

void PlayerController::setupHttpServer() {
    httpServer->setupHttpServer();

    connect(httpServer, &HttpServerThread::finished, httpServer, &QObject::deleteLater);
    connect(httpServer, &HttpServerThread::quitCPlay, this, &PlayerController::QuitCPlay);
    connect(httpServer, &HttpServerThread::pauseMedia, this, &PlayerController::Pause);
    connect(httpServer, &HttpServerThread::playMedia, this, &PlayerController::Play);
    connect(httpServer, &HttpServerThread::rewindMedia, this, &PlayerController::Rewind);
    connect(httpServer, &HttpServerThread::seekInMedia, this, &PlayerController::Seek);
    connect(httpServer, &HttpServerThread::setAutoPlay, this, &PlayerController::SetAutoPlay);
    connect(httpServer, &HttpServerThread::setPosition, this, &PlayerController::SetPosition);
    connect(httpServer, &HttpServerThread::setSpeed, this, &PlayerController::SetSpeed);
    connect(httpServer, &HttpServerThread::setVolume, this, &PlayerController::SetVolume);
    connect(httpServer, &HttpServerThread::setViewMode, this, &PlayerController::setViewModeOnClients);
    connect(httpServer, &HttpServerThread::setBackgroundVisibility, this, &PlayerController::setBackgroundVisibility);
    connect(httpServer, &HttpServerThread::setForegroundVisibility, this, &PlayerController::setForegroundVisibility);
    connect(httpServer, &HttpServerThread::setSyncVolumeVisibilityFading, this, &PlayerController::SetSyncVolumeVisibilityFading);
    connect(httpServer, &HttpServerThread::fadeVolumeDown, this, &PlayerController::FadeVolumeDown);
    connect(httpServer, &HttpServerThread::fadeVolumeUp, this, &PlayerController::FadeVolumeUp);
    connect(httpServer, &HttpServerThread::fadeImageDown, this, &PlayerController::FadeImageDown);
    connect(httpServer, &HttpServerThread::fadeImageUp, this, &PlayerController::FadeImageUp);
    connect(httpServer, &HttpServerThread::loadFromAudioTracks, this, &PlayerController::LoadFromAudioTracks);
    connect(httpServer, &HttpServerThread::loadFromPlaylist, this, &PlayerController::LoadFromPlaylist);
    connect(httpServer, &HttpServerThread::loadFromSections, this, &PlayerController::LoadFromSections);
    connect(httpServer, &HttpServerThread::loadFromSlides, this, &PlayerController::LoadFromSlides);
    connect(httpServer, &HttpServerThread::selectFromSlides, this, &PlayerController::SelectFromSlides);
    connect(httpServer, &HttpServerThread::spinPitchUp, this, &PlayerController::SpinPitchUp);
    connect(httpServer, &HttpServerThread::spinPitchDown, this, &PlayerController::SpinPitchDown);
    connect(httpServer, &HttpServerThread::spinYawLeft, this, &PlayerController::SpinYawLeft);
    connect(httpServer, &HttpServerThread::spinYawRight, this, &PlayerController::SpinYawRight);
    connect(httpServer, &HttpServerThread::spinRollCCW, this, &PlayerController::SpinRollCCW);
    connect(httpServer, &HttpServerThread::spinRollCW, this, &PlayerController::SpinRollCW);
    connect(httpServer, &HttpServerThread::orientationAndSpinReset, this, &PlayerController::OrientationAndSpinReset);
    connect(httpServer, &HttpServerThread::runSurfaceTransition, this, &PlayerController::RunSurfaceTransition);
    connect(httpServer, &HttpServerThread::slidePrevious, this, &PlayerController::SlidePrevious);
    connect(httpServer, &HttpServerThread::slideNext, this, &PlayerController::SlideNext);
    connect(httpServer, &HttpServerThread::resetSlidesToStart, this, &PlayerController::ResetSlidesToStart);

    httpServer->start();
}

void PlayerController::QuitCPlay() {
    Q_EMIT quitCPlay();
}

void PlayerController::Next() {
    Q_EMIT next();
}

void PlayerController::Previous() {
    Q_EMIT previous();
}

void PlayerController::Pause() {
    Q_EMIT pause();
}

void PlayerController::PlayPause() {
    Q_EMIT playpause();
}

void PlayerController::Stop() {
    Q_EMIT stop();
}

void PlayerController::Play() {
    Q_EMIT play();
}

void PlayerController::Rewind() {
    if (m_mpv) {
        m_mpv->performRewind();
    }
}

void PlayerController::Seek(int timeInSec) {
    if (m_mpv) {
        m_mpv->seek(timeInSec);
    }
}

void PlayerController::SetAutoPlay(bool value) {
    if (m_mpv) {
        m_mpv->setAutoPlay(value);
    }
}

void PlayerController::LoadFromAudioTracks(int idx) {
    if (m_mpv) {
        m_mpv->setAudioId(idx + 1); // AudioID starts from 1.
    }
}

void PlayerController::LoadFromPlaylist(int idx) {
    Q_EMIT loadFromPlaylist(idx);
}

void PlayerController::LoadFromSections(int idx) {
    Q_EMIT loadFromSections(idx);
}

void PlayerController::LoadFromSlides(int idx) {
    if (m_slidesModel != nullptr && idx < m_slidesModel->numberOfSlides() && idx >= 0) {
        m_slidesModel->setSlideFadeTime(PresentationSettings::fadeDurationToNextSlide());
        m_slidesModel->setSelectedSlideIdx(idx);
        m_slidesModel->setTriggeredSlideIdx(idx);
    }
}

void PlayerController::SelectFromSlides(int idx) {
    if (m_slidesModel != nullptr && idx < m_slidesModel->numberOfSlides() && idx >= 0) {
        m_slidesModel->setSelectedSlideIdx(idx);
    }
}

void PlayerController::SetPosition(double pos) {
    if (m_mpv) {
        m_mpv->setPosition(pos);
    }
}

void PlayerController::SetSpeed(double factor) {
    if (m_mpv) {
        m_mpv->setSpeed(factor);
    }
}

void PlayerController::SetVolume(int level) {
    if (m_mpv) {
        m_mpv->setVolume(level);
    }
}

void PlayerController::SetSyncVolumeVisibilityFading(bool value) {
    if (m_mpv) {
        m_mpv->setSyncVolumeVisibilityFading(value);
    }
}

void PlayerController::FadeVolumeDown() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeDown();
    }
}

void PlayerController::FadeVolumeUp() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeUp();
    }
}

void PlayerController::FadeImageDown() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageDown();
    }
}

void PlayerController::FadeImageUp() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageUp();
    }
}

void PlayerController::SpinPitchUp(bool run) {
    Q_EMIT spinPitchUp(run);
}

void PlayerController::SpinPitchDown(bool run) {
    Q_EMIT spinPitchDown(run);
}

void PlayerController::SpinYawLeft(bool run) {
    Q_EMIT spinYawLeft(run);
}

void PlayerController::SpinYawRight(bool run) {
    Q_EMIT spinYawRight(run);
}

void PlayerController::SpinRollCW(bool run) {
    Q_EMIT spinRollCW(run);
}

void PlayerController::SpinRollCCW(bool run) {
    Q_EMIT spinRollCCW(run);
}

void PlayerController::OrientationAndSpinReset() {
    Q_EMIT orientationAndSpinReset();
}

void PlayerController::RunSurfaceTransition() {
    Q_EMIT runSurfaceTransition();
}

void PlayerController::SlidePrevious() {
    if (m_slidesModel != nullptr) {
        Q_EMIT m_slidesModel->previousSlide();
    }
}
void PlayerController::SlideNext() {
    if (m_slidesModel != nullptr) {
        Q_EMIT m_slidesModel->nextSlide();
    }
}

void PlayerController::ResetSlidesToStart() {
    Q_EMIT resetSlidesToStart();
}

void PlayerController::DispatchControlOperation(const QString &operation, const QString &parameter) {
    if (operation == QStringLiteral("Play")) {
        Play();
    } else if (operation == QStringLiteral("Pause")) {
        Pause();
    } else if (operation == QStringLiteral("Stop")) {
        Stop();
    } else if (operation == QStringLiteral("Rewind")) {
        Rewind();
    } else if (operation == QStringLiteral("Seek")) {
        Seek(parameter.toInt());
    } else if (operation == QStringLiteral("SetPosition")) {
        SetPosition(parameter.toDouble());
    } else if (operation == QStringLiteral("FadeVolumeDown")) {
        FadeVolumeDown();
    } else if (operation == QStringLiteral("FadeVolumeUp")) {
        FadeVolumeUp();
    } else if (operation == QStringLiteral("FadeImageDown")) {
        FadeImageDown();
    } else if (operation == QStringLiteral("FadeImageUp")) {
        FadeImageUp();
    } else if (operation == QStringLiteral("FadeVolumeDown")) {
        FadeVolumeDown();
    } else if (operation == QStringLiteral("FadeVolumeUp")) {
        FadeVolumeUp();
    } else if (operation == QStringLiteral("FadeImageDown")) {
        FadeImageDown();
    } else if (operation == QStringLiteral("FadeImageUp")) {
        FadeImageUp();
    } else if (operation == QStringLiteral("LoadFromAudioTracks")) {
        // Parameter is audio track name; resolve to index
        if (m_mpv) {
            int idx = parameter.toInt();
            // If parameter is not a number, try to find by name
            bool isNumber = false;
            parameter.toInt(&isNumber);
            if (!isNumber) {
                TracksModel* audioModel = m_mpv->audioTracksModel();
                if (audioModel) {
                    for (int i = 0; i < audioModel->rowCount(); ++i) {
                        QModelIndex mi = audioModel->index(i, 0);
                        QString title = audioModel->data(mi, Qt::DisplayRole).toString();
                        if (title == parameter) {
                            idx = i;
                            break;
                        }
                    }
                }
            }
            LoadFromAudioTracks(idx);
        }
    } else if (operation == QStringLiteral("LoadFromPlaylist")) {
        // Parameter is playlist item title; resolve to index
        if (m_mpv) {
            PlayListModel* plModel = m_mpv->playlistModel();
            if (plModel) {
                bool isNumber = false;
                int idx = parameter.toInt(&isNumber);
                if (!isNumber) {
                    for (int i = 0; i < plModel->getPlayListSize(); ++i) {
                        QString title = plModel->mediaTitle(i);
                        QString listTitle = plModel->listTitle(i);
                        QString fileName = plModel->fileName(i);
                        if (title == parameter || listTitle == parameter || fileName == parameter) {
                            idx = i;
                            break;
                        }
                    }
                }
                LoadFromPlaylist(idx);
            }
        }
    } else if (operation == QStringLiteral("LoadFromSections")) {
        // Parameter is section title; resolve to index
        if (m_mpv) {
            PlaySectionsModel* secModel = m_mpv->playSectionsModel();
            if (secModel) {
                bool isNumber = false;
                int idx = parameter.toInt(&isNumber);
                if (!isNumber) {
                    for (int i = 0; i < secModel->getNumberOfSections(); ++i) {
                        if (secModel->sectionTitle(i) == parameter) {
                            idx = i;
                            break;
                        }
                    }
                }
                LoadFromSections(idx);
            }
        }
    } else if (operation == QStringLiteral("LoadFromSlides")) {
        // Parameter is slide name; resolve to index
        if (m_slidesModel) {
            bool isNumber = false;
            int idx = parameter.toInt(&isNumber);
            if (!isNumber) {
                for (int i = 0; i < m_slidesModel->numberOfSlides(); ++i) {
                    LayersModel* s = m_slidesModel->slide(i);
                    if (s && s->getLayersName() == parameter) {
                        idx = i;
                        break;
                    }
                }
            }
            LoadFromSlides(idx);
        }
    } else if (operation == QStringLiteral("SetSpeed")) {
        SetSpeed(parameter.toDouble());
    } else if (operation == QStringLiteral("SetVolume")) {
        SetVolume(parameter.toInt());
    } else if (operation == QStringLiteral("SetSyncVolumeVisibilityFading")) {
        SetSyncVolumeVisibilityFading(parameter == QStringLiteral("true") || parameter == QStringLiteral("1"));
    } else if (operation == QStringLiteral("SpinPitchUp")) {
        SpinPitchUp(parameter != QStringLiteral("false") && parameter != QStringLiteral("0"));
    } else if (operation == QStringLiteral("SpinPitchDown")) {
        SpinPitchDown(parameter != QStringLiteral("false") && parameter != QStringLiteral("0"));
    } else if (operation == QStringLiteral("SpinYawLeft")) {
        SpinYawLeft(parameter != QStringLiteral("false") && parameter != QStringLiteral("0"));
    } else if (operation == QStringLiteral("SpinYawRight")) {
        SpinYawRight(parameter != QStringLiteral("false") && parameter != QStringLiteral("0"));
    } else if (operation == QStringLiteral("SpinRollCW")) {
        SpinRollCW(parameter != QStringLiteral("false") && parameter != QStringLiteral("0"));
    } else if (operation == QStringLiteral("SpinRollCCW")) {
        SpinRollCCW(parameter != QStringLiteral("false") && parameter != QStringLiteral("0"));
    } else if (operation == QStringLiteral("OrientationAndSpinReset")) {
        OrientationAndSpinReset();
    } else if (operation == QStringLiteral("RunSurfaceTransition")) {
        RunSurfaceTransition();
    } else if (operation == QStringLiteral("SetBackgroundVisibility")) {
        setBackgroundVisibility(parameter.toFloat());
    } else if (operation == QStringLiteral("SetForegroundVisibility")) {
        setForegroundVisibility(parameter.toFloat());
    } else if (operation == QStringLiteral("SetNodeWindowsOpacity")) {
        setNodeWindowsOpacity(parameter.toFloat());
    }
}

QString PlayerController::returnRelativeOrAbsolutePath(const QString &path) {
    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    QStringList pathsToConsider;
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());

    // Assuming filePath is absolute
    // Looking for shortest relative path
    QString shortFilePath = filePath;
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            QString newRelativePath = foundDir.relativeFilePath(filePath);
            if (newRelativePath.length() < shortFilePath.length())
                shortFilePath = newRelativePath;
        }
    }
    return shortFilePath;
}

QString PlayerController::checkAndCorrectPath(const QString &path) {
    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    if (fileInfo.exists())
        return filePath;
    else if (fileInfo.isRelative()) { // Go through search list in order
        QStringList searchPaths;
        searchPaths.append(LocationSettings::cPlayMediaLocation());
        searchPaths.append(LocationSettings::cPlayFileLocation());
        searchPaths.append(LocationSettings::univiewVideoLocation());

        for (int i = 0; i < searchPaths.size(); i++) {
            QString newFilePath = QDir::cleanPath(searchPaths[i] + QDir::separator() + filePath);
            QFileInfo newFilePathInfo(newFilePath);
            if (newFilePathInfo.exists())
                return newFilePath;
        }

        // Maybe from network share?
        filePath = path;
        filePath.replace(QStringLiteral("file://"), QStringLiteral("\\\\"));
        fileInfo = QFileInfo(filePath);
        if (fileInfo.exists())
            return filePath;
    }
    return QStringLiteral("");
}

QString PlayerController::returnFileBaseName(const QString &path) {
    QFileInfo fileInfo(path);
    return fileInfo.baseName();
}

QString PlayerController::returnFileName(const QString& path) {
    QFileInfo fileInfo(path);
    return fileInfo.fileName();
}

QString PlayerController::returnFileExtension(const QString& path) {
    QFileInfo fileInfo(path);
    return fileInfo.suffix();
}

float PlayerController::backgroundVisibility() {
    return SyncHelper::instance().variables.alphaBg;
}

void PlayerController::setBackgroundVisibility(float value) {
    SyncHelper::instance().variables.alphaBg = value;

    Q_EMIT backgroundVisibilityChanged();
}

QString PlayerController::backgroundImageFile() {
    return m_backgroundFile;
}

QUrl PlayerController::backgroundImageFileUrl() {
    if (SyncHelper::instance().variables.bgImageFile.empty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.bgImageFile));
}

void PlayerController::setBackgroundImageFile(const QString &path) {
    if (path.isEmpty()) {
        m_backgroundFile = QStringLiteral("");
        SyncHelper::instance().variables.bgImageFile = "";
        SyncHelper::instance().variables.bgImageFileDirty = true;
        setBackgroundVisibility(0.f);
        Q_EMIT backgroundImageChanged();
        return;
    }

    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    QString absolutePath;
    if (fileInfo.exists()) { // isAbsolute
        m_backgroundFile = returnRelativeOrAbsolutePath(path);
        absolutePath = path;
    } else if (fileInfo.isRelative()) { // isRelative
        QString bgFilePath = checkAndCorrectPath(path);
        if (bgFilePath.isEmpty()) {
            return;
        }
        m_backgroundFile = path;
        absolutePath = bgFilePath;
    } else
        return;

    absolutePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    std::string str = absolutePath.toStdString();
    if (SyncHelper::instance().variables.bgImageFile != str) {
        SyncHelper::instance().variables.bgImageFile = str;
        SyncHelper::instance().variables.bgImageFileDirty = true;
        setBackgroundVisibility(1.f);
    }

    Q_EMIT backgroundImageChanged();
}

int PlayerController::backgroundGridMode() {
    return SyncHelper::instance().variables.gridToMapOnBg;
}

void PlayerController::setBackgroundGridMode(int value) {
    SyncHelper::instance().variables.gridToMapOnBg = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;
}

int PlayerController::backgroundStereoMode() {
    return SyncHelper::instance().variables.stereoscopicModeBg;
}

void PlayerController::setBackgroundStereoMode(int value) {
    SyncHelper::instance().variables.stereoscopicModeBg = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;
}

float PlayerController::foregroundVisibility() {
    return SyncHelper::instance().variables.alphaFg;
}

void PlayerController::setForegroundVisibility(float value) {
    SyncHelper::instance().variables.alphaFg = value;

    Q_EMIT foregroundVisibilityChanged();
}

QString PlayerController::foregroundImageFile() {
    return m_foregroundFile;
}

QUrl PlayerController::foregroundImageFileUrl() {
    if (SyncHelper::instance().variables.fgImageFile.empty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.fgImageFile));
}

void PlayerController::setForegroundImageFile(const QString &path) {
    if (path.isEmpty()) {
        m_foregroundFile = QStringLiteral("");
        SyncHelper::instance().variables.fgImageFile = "";
        SyncHelper::instance().variables.fgImageFileDirty = true;
        Q_EMIT foregroundImageChanged();
        return;
    }

    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    QString absolutePath;
    if (fileInfo.exists()) { // isAbsolute
        m_foregroundFile = returnRelativeOrAbsolutePath(path);
        absolutePath = path;
    } else if (fileInfo.isRelative()) { // isRelative
        QString fgFilePath = checkAndCorrectPath(path);
        if (fgFilePath.isEmpty()) {
            return;
        }
        m_foregroundFile = path;
        absolutePath = fgFilePath;
    } else
        return;

    absolutePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    std::string str = absolutePath.toStdString();
    if (SyncHelper::instance().variables.fgImageFile != str) {
        SyncHelper::instance().variables.fgImageFile = str;
        SyncHelper::instance().variables.fgImageFileDirty = true;
    }

    Q_EMIT foregroundImageChanged();
}

int PlayerController::foregroundGridMode() {
    return SyncHelper::instance().variables.gridToMapOnFg;
}

void PlayerController::setForegroundGridMode(int value) {
    SyncHelper::instance().variables.gridToMapOnFg = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;
}

int PlayerController::foregroundStereoMode() {
    return SyncHelper::instance().variables.stereoscopicModeFg;
}

void PlayerController::setForegroundStereoMode(int value) {
    SyncHelper::instance().variables.stereoscopicModeFg = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;
}

QString PlayerController::noSignalImageForCapture() {
    return ImageSettings::noSignalImageForCapture();
}

void PlayerController::setNoSignalImageForCapture(const QString &path) {
    QString stored;
    if (!storableNoSignalImagePath(path, stored))
        return;
    ImageSettings::setNoSignalImageForCapture(stored);
    ImageSettings::self()->save();
    applyNoSignalImages();
}

QString PlayerController::noSignalImageForDirectShow() {
    return ImageSettings::noSignalImageForDirectShow();
}

void PlayerController::setNoSignalImageForDirectShow(const QString &path) {
    QString stored;
    if (!storableNoSignalImagePath(path, stored))
        return;
    ImageSettings::setNoSignalImageForDirectShow(stored);
    ImageSettings::self()->save();
    applyNoSignalImages();
}

bool PlayerController::storableNoSignalImagePath(const QString &path, QString &stored) {
    stored.clear();
    if (path.isEmpty())
        return true;
    const QString absolutePath = checkAndCorrectPath(path);
    if (absolutePath.isEmpty())
        return false;
    stored = returnRelativeOrAbsolutePath(absolutePath);
    return true;
}

void PlayerController::applyNoSignalImages() {
    auto resolve = [this](const QString &stored) {
        return stored.isEmpty() ? std::string() : checkAndCorrectPath(stored).toStdString();
    };
    auto &vars = SyncHelper::instance().variables;
#ifdef CAPTURE_SUPPORT
    const std::string capture = resolve(ImageSettings::noSignalImageForCapture());
    BaseLayer::setNoSignalImageFile(BaseLayer::CAPTURE, capture);
    vars.noSignalImageCapture = capture;
    vars.noSignalImageCaptureDirty = true;
#endif
#ifdef DIRECTSHOW_SUPPORT
    const std::string directShow = resolve(ImageSettings::noSignalImageForDirectShow());
    BaseLayer::setNoSignalImageFile(BaseLayer::DIRECTSHOW, directShow);
    vars.noSignalImageDirectShow = directShow;
    vars.noSignalImageDirectShowDirty = true;
#endif
    (void)resolve;
    (void)vars;
}

float PlayerController::backgroundVisibilityOnMaster() {
    if (getViewModeOnMaster() == 1) {
        return 0.f;
    } else if (getViewModeOnMaster() == 2) {
        return 1.f;
    }

    if (backgroundVisibility() > 0.f) {
        if (SyncHelper::instance().variables.loadedFile.empty()) {
            return backgroundVisibility();
        } else {
            return 1.f - SyncHelper::instance().variables.alpha;
        }
    }

    return 0.f;
}

void PlayerController::setViewModeOnMaster(int value) {
    // 0 = Same as nodes
    // 1 = Show media always
    // 2 = Show background always
    m_viewModeOnMaster = value;

    Q_EMIT backgroundVisibilityChanged();
}

int PlayerController::getViewModeOnMaster() {
    return m_viewModeOnMaster;
}

void PlayerController::setViewModeOnClients(int value) {
    // 0 = Auto 2D/3D switch
    // 1 = Force 2D for all
    SyncHelper::instance().variables.viewMode = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;

    Q_EMIT viewModeOnClientsChanged();
}

int PlayerController::getViewModeOnClients() {
    return SyncHelper::instance().variables.viewMode;
}

int PlayerController::masterNdiOnNodes() {
    return SyncHelper::instance().variables.masterNdiOnNodes;
}

void PlayerController::setMasterNdiOnNodes(int value) {
    // 0 = nodes render their own layers, 1-3 = nodes render the master "C-Play" NDI stream
    if (SyncHelper::instance().variables.masterNdiOnNodes == value)
        return;

    SyncHelper::instance().variables.masterNdiOnNodes = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;

    Q_EMIT masterNdiOnNodesChanged();
}

QString PlayerController::masterNdiName() {
    return QString::fromStdString(SyncHelper::instance().variables.masterNdiName);
}

void PlayerController::setMasterNdiName(const QString &value) {
    // Receivers need the full name of the master sender, which includes the machine name.
    const std::string name = value.toStdString();
    if (SyncHelper::instance().variables.masterNdiName == name)
        return;

    SyncHelper::instance().variables.masterNdiName = name;
    SyncHelper::instance().variables.playerControllerNeedSync = true;

    Q_EMIT masterNdiNameChanged();
}

bool PlayerController::rewindMediaOnEOF() {
    return m_rewindMediaOnEOF;
}

void PlayerController::setRewindMediaOnEOF(bool value) {
    m_rewindMediaOnEOF = value;

    Q_EMIT rewindMediaOnEOFChanged();
}

bool PlayerController::nodeWindowsOnTop() {
    return SyncHelper::instance().variables.windowOnTop;
}

void PlayerController::setNodeWindowsOnTop(bool value) {
    // The node window on-top feature only works on windowed (non-fullscreen) node
    // windows: a fullscreen window acquires an exclusive video mode and is already at
    // the top of its display, so floating/Z-ordering it above other applications has no
    // visible effect. When the cluster configuration has no windowed node windows, keep
    // the nodes in normal Z-order instead of applying a floating state that would be
    // silently ignored.
    if (!ConfigModel::instance().nodeWindowTransparencySupported()) {
        value = false;
    }

    SyncHelper::instance().variables.windowOnTop = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;

    Q_EMIT nodeWindowOnTopChanged();
}

float PlayerController::nodeWindowsOpacity() {
    return SyncHelper::instance().variables.windowOpacity;
}

void PlayerController::setNodeWindowsOpacity(float value) {
    // The node window transparency/fading feature only works on windowed (non-fullscreen)
    // node windows: a fullscreen window acquires an exclusive video mode with an opaque
    // pixel format, where neither whole-window fades nor per-pixel alpha have any effect.
    // When the cluster configuration has no windowed node windows, keep the nodes fully
    // visible instead of applying a fade that would be silently ignored.
    if (!ConfigModel::instance().nodeWindowTransparencySupported()) {
        value = 1.0f;
    }

    SyncHelper::instance().variables.windowOpacity = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;

    Q_EMIT nodeWindowOpacityChanged();
}

bool PlayerController::nodeWindowOpacityContentBased() {
    return SyncHelper::instance().variables.windowOpacityContentBased;
}

void PlayerController::setNodeWindowOpacityContentBased(bool value) {
    SyncHelper::instance().variables.windowOpacityContentBased = value;
    SyncHelper::instance().variables.playerControllerNeedSync = true;

    Q_EMIT nodeWindowOpacityContentBasedChanged();
}

bool PlayerController::syncProperties() {
    return SyncHelper::instance().variables.syncOn;
}

void PlayerController::setSyncProperties(bool value) {
    SyncHelper::instance().variables.syncOn = value;
    Q_EMIT syncPropertiesChanged();
}

void PlayerController::takeNodeScreenshot(const QString& screenshotPath) {
    SyncHelper::instance().variables.screenshotPath = screenshotPath.toStdString();
    SyncHelper::instance().variables.takeScreenshot = true;
    SyncHelper::instance().variables.playerControllerNeedSync = true;
}

void PlayerController::setCaptureBackBuffer(bool backBuffer) {
    SyncHelper::instance().variables.captureBackBuffer = backBuffer;
}

QVariantList PlayerController::nodeTelemetry() const {
    return NodeTelemetryManager::instance().telemetryList();
}

QVariantList PlayerController::nodeLoadFailures() const {
    return NodeLoaderVerifier::instance().loadFailuresList();
}

void PlayerController::setNodeTelemetryEnabled(bool enabled) {
    LoggingSettings::setNodeTelemetryEnabled(enabled);
    LoggingSettings::self()->save();
    NodeTelemetryManager::instance().setEnabled(enabled);
}

bool PlayerController::nodeTelemetryEnabled() const {
    return LoggingSettings::nodeTelemetryEnabled();
}

void PlayerController::setNodeTelemetryInterval(int ms) {
    const int clamped = qBound(100, ms, 5000);
    if (LoggingSettings::nodeTelemetryInterval() == clamped)
        return;
    LoggingSettings::setNodeTelemetryInterval(clamped);
    LoggingSettings::self()->save();
    NodeTelemetryManager::instance().setIntervalMs(clamped);
}

int PlayerController::nodeTelemetryInterval() const {
    return LoggingSettings::nodeTelemetryInterval();
}

QString PlayerController::supportedImageNameFilters() const {
    QStringList exts = {
        QStringLiteral("*.bmp"),
        QStringLiteral("*.png"),
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.tga"),
    };
    QSet<QString> extSet;
    for (const QString &extension : exts) {
        extSet.insert(extension);
    }
#ifdef SAIL_SUPPORT
    // Query SAIL for all supported file extensions at runtime
    try {
        std::vector<sail::codec_info> codecs = sail::codec_info::list();
        for (const auto &codec : codecs) {
            for (const auto &ext : codec.extensions()) {
                QString qExt = QStringLiteral("*.") + QString::fromStdString(ext).toLower();
                if (!extSet.contains(qExt)) {
                    extSet.insert(qExt);
                    exts << qExt;
                }
            }
        }
    } catch (...) {
        // If SAIL enumeration fails, just use the base extensions
    }
#endif

    return QStringLiteral("Image files (") + exts.join(QStringLiteral(" ")) + QStringLiteral(")");
}

QStringList PlayerController::supportedImageDecoderNames() const {
    QStringList decoders;
    decoders << QStringLiteral("Auto");
#ifdef WUFFS_SUPPORT
    decoders << QStringLiteral("Wuffs");
#endif
#ifdef SAIL_SUPPORT
    decoders << QStringLiteral("Sail");
#endif
    decoders << QStringLiteral("Sgct");
    return decoders;
}

QString PlayerController::imageRingBufferGpuMemoryText(int percent) const {
    const int gpuMemoryKB = ImageLayer::lastKnownGpuMemoryKB();
    if (gpuMemoryKB <= 0) {
        return tr("GPU memory not detected yet");
    }

    if (percent < 1) percent = 1;
    if (percent > 90) percent = 90;

    const std::uint64_t budgetBytes =
        (static_cast<std::uint64_t>(gpuMemoryKB) * 1024ULL * static_cast<std::uint64_t>(percent)) / 100ULL;
    return formatMemoryBudgetText(budgetBytes);
}

MpvObject *PlayerController::mpv() const {
    return m_mpv;
}

void PlayerController::setMpv(MpvObject *mpv) {
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
    httpServer->setMpv(mpv);
    // The telemetry manager builds the master's own row from this instance.
    NodeTelemetryManager::instance().setMasterMpv(mpv);
    Q_EMIT mpvChanged();
}

SlidesModel* PlayerController::slidesModel() const {
    return m_slidesModel;
}

void PlayerController::setSlidesModel(SlidesModel* sm) {
    if (m_slidesModel == sm) {
        return;
    }
    m_slidesModel = sm;
    httpServer->setSlidesModel(sm);
    Q_EMIT slidesModelChanged();
}
