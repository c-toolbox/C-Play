/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "presentercontroller.h"
#include "layersmodel.h"
#include "layersrendererqtitem.h"
#include "slidesmodel.h"

#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>

namespace {
// Wide enough for a sharp preview on a presenter screen, small enough to keep the presentation file light.
constexpr int kThumbnailMaxWidth = 640;
constexpr int kThumbnailJpegQuality = 85;

QString findAppModeBrowser() {
    QStringList candidates;
#ifdef Q_OS_WIN
    for (const auto &env : {"ProgramFiles(x86)", "ProgramFiles", "LOCALAPPDATA"}) {
        const QString base = qEnvironmentVariable(env);
        if (base.isEmpty())
            continue;
        candidates << base + QStringLiteral("/Microsoft/Edge/Application/msedge.exe")
                   << base + QStringLiteral("/Google/Chrome/Application/chrome.exe");
    }
#endif
    for (const auto &name : {"msedge", "microsoft-edge", "google-chrome", "chrome", "chromium", "chromium-browser"}) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!path.isEmpty())
            candidates << path;
    }
    for (const auto &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}
}

PresenterController::PresenterController(QObject *parent)
    : QObject(parent) {
}

void PresenterController::setSlidesModel(SlidesModel *slides) {
    m_slides = slides;
}

void PresenterController::setServer(bool running, int port) {
    m_serverRunning = running;
    m_port = port;
    Q_EMIT serverChanged();
}

LayersRendererQtItem *PresenterController::layersRenderer() const {
    return m_layersRenderer;
}

void PresenterController::setLayersRenderer(LayersRendererQtItem *renderer) {
    if (m_layersRenderer == renderer)
        return;
    if (m_layersRenderer)
        disconnect(m_layersRenderer, nullptr, this, nullptr);
    m_layersRenderer = renderer;
    if (m_layersRenderer)
        connect(m_layersRenderer, &LayersRendererQtItem::snapshotReady, this, &PresenterController::onSnapshotReady);
    Q_EMIT layersRendererChanged();
}

bool PresenterController::serverRunning() const {
    return m_serverRunning;
}

QString PresenterController::localUrl() const {
    return localUrlOf(QStringLiteral("presenter"));
}

QString PresenterController::localUrlOf(const QString &view) const {
    return QStringLiteral("http://127.0.0.1:%1/%2/").arg(m_port).arg(view);
}

bool PresenterController::webViewAvailable() const {
#ifdef WEBVIEW_SUPPORT
    return true;
#else
    return false;
#endif
}

QStringList PresenterController::networkUrls(const QString &view) const {
    QStringList urls;
    for (const QHostAddress &address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback() && !address.isLinkLocal())
            urls << QStringLiteral("http://%1:%2/%3/").arg(address.toString()).arg(m_port).arg(view);
    }
    return urls;
}

LayersModel *PresenterController::slideAt(int idx) const {
    if (!m_slides || idx < 0 || idx >= m_slides->numberOfSlides())
        return nullptr;
    return m_slides->slide(idx);
}

QJsonObject PresenterController::state() const {
    if (!m_slides)
        return {};

    const int count = m_slides->numberOfSlides();
    const int selected = m_slides->selectedSlideIdx();
    const int triggered = m_slides->triggeredSlideIdx();
    // The slide "next" goes to, as in the slideNext action: the selected slide when another
    // one is shown, otherwise the one after the shown slide.
    int next = -1;
    if (selected != triggered)
        next = selected < count - 1 ? selected : -1;
    else
        next = m_slides->nextNotSkippedSlideIdx(triggered);

    QJsonArray slides;
    for (int i = 0; i < count; ++i) {
        const LayersModel *slide = slideAt(i);
        if (!slide)
            continue;
        slides.append(QJsonObject{{QStringLiteral("index"), i},
            {QStringLiteral("name"), slide->getLayersName()},
            {QStringLiteral("notes"), slide->notes()},
            {QStringLiteral("thumbnail"), slide->thumbnail().isEmpty() ? 0 : slide->thumbnailRevision()}});
    }

    return {{QStringLiteral("name"), m_slides->getSlidesName()},
        {QStringLiteral("canSave"), !m_slides->getSlidesPath().isEmpty()},
        {QStringLiteral("needsSave"), m_slides->getSlidesNeedsSave()},
        {QStringLiteral("selected"), selected},
        {QStringLiteral("triggered"), triggered},
        {QStringLiteral("next"), next},
        // "next" plays an on-click animation step of the shown slide instead of changing slide.
        {QStringLiteral("pendingStep"), selected == triggered && m_slides->hasPendingAnimationStep()},
        {QStringLiteral("canCapture"), m_layersRenderer && m_layersRenderer->window()},
        {QStringLiteral("slides"), slides}};
}

bool PresenterController::setNotes(int slideIdx, const QString &notes) {
    LayersModel *slide = slideAt(slideIdx);
    if (!slide)
        return false;
    slide->setNotes(notes);
    return true;
}

QByteArray PresenterController::thumbnail(int slideIdx) const {
    const LayersModel *slide = slideAt(slideIdx);
    return slide ? slide->thumbnail() : QByteArray();
}

bool PresenterController::captureThumbnail(int slideIdx) {
    LayersModel *slide = slideAt(slideIdx);
    // Rendered at twice the size and scaled down, which smooths the edges.
    if (!slide || !m_layersRenderer || !m_layersRenderer->requestSnapshot(kThumbnailMaxWidth * 2))
        return false;
    m_thumbnailSlide = slide;
    return true;
}

void PresenterController::onSnapshotReady(const QImage &snapshot) {
    LayersModel *slide = m_thumbnailSlide;
    m_thumbnailSlide.clear();
    if (!slide || snapshot.isNull())
        return;

    QImage image = snapshot;
    if (image.width() > kThumbnailMaxWidth)
        image = image.scaledToWidth(kThumbnailMaxWidth, Qt::SmoothTransformation);

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.convertToFormat(QImage::Format_RGB888).save(&buffer, "JPEG", kThumbnailJpegQuality))
        return;
    slide->setThumbnail(jpeg);
}

bool PresenterController::save() {
    if (!m_slides || m_slides->getSlidesPath().isEmpty())
        return false;
    m_slides->saveAsJSONFile(m_slides->getSlidesPath());
    return !m_slides->getSlidesNeedsSave();
}

void PresenterController::openInBrowser(int screenIdx, const QString &view) {
    const QUrl url(localUrlOf(view));
    const QString browser = findAppModeBrowser();
    if (browser.isEmpty()) {
        QDesktopServices::openUrl(url);
        return;
    }

    const auto screens = QGuiApplication::screens();
    QScreen *screen = screenIdx >= 0 && screenIdx < screens.size() ? screens.at(screenIdx) : QGuiApplication::primaryScreen();
    const QRect geometry = screen ? screen->geometry() : QRect();
    // An own profile per view, so the window opens with these options even when the browser
    // (or the other view) already runs.
    const QString profile = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/%1-browser").arg(view);
    QDir().mkpath(profile);
    QProcess::startDetached(browser, {
        QStringLiteral("--app=%1").arg(url.toString()),
        QStringLiteral("--user-data-dir=%1").arg(QDir::toNativeSeparators(profile)),
        QStringLiteral("--window-position=%1,%2").arg(geometry.x()).arg(geometry.y()),
        QStringLiteral("--start-fullscreen"),
        QStringLiteral("--no-first-run"),
        QStringLiteral("--no-default-browser-check")});
}
