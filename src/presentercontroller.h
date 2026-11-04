/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PRESENTERCONTROLLER_H
#define PRESENTERCONTROLLER_H

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class LayersModel;
class QProcess;
class LayersRendererQtItem;
class SlidesModel;

// Backs the presenter view (web/presentation_mode), served by the HTTP server at /presenter/:
// slide notes, slide thumbnails and the presentation state it shows. Lives on the GUI thread;
// the HTTP server calls it through queued calls.
class PresenterController : public QObject {
    Q_OBJECT
    Q_MOC_INCLUDE("layersrendererqtitem.h")
    // The master view renderer; slide thumbnails are rendered with its camera (perspective,
    // fisheye or sphere view).
    Q_PROPERTY(LayersRendererQtItem *layersRenderer READ layersRenderer WRITE setLayersRenderer NOTIFY layersRendererChanged)
    Q_PROPERTY(bool serverRunning READ serverRunning NOTIFY serverChanged)
    Q_PROPERTY(QString localUrl READ localUrl NOTIFY serverChanged)
    // True when built with Qt WebView, so the presenter view can open in a C-Play window.
    Q_PROPERTY(bool webViewAvailable READ webViewAvailable CONSTANT)
    // The web views open in a browser app window (openInBrowser): {view: screen index}.
    Q_PROPERTY(QVariantMap browserViews READ browserViews NOTIFY browserViewsChanged)

public:
    explicit PresenterController(QObject *parent = nullptr);
    // Closes the browser app windows, as the web views need the HTTP server of this C-Play.
    ~PresenterController() override;

    void setSlidesModel(SlidesModel *slides);
    void setServer(bool running, int port);

    LayersRendererQtItem *layersRenderer() const;
    void setLayersRenderer(LayersRendererQtItem *renderer);
    bool serverRunning() const;
    QString localUrl() const;
    bool webViewAvailable() const;
    QVariantMap browserViews() const;
    // Address on this computer of the web view served at /<view>/: "presenter" or "movie".
    Q_INVOKABLE QString localUrlOf(const QString &view) const;
    // Addresses of the web view for other devices on the network.
    Q_INVOKABLE QStringList networkUrls(const QString &view = QStringLiteral("presenter")) const;

    // Everything the presenter view shows, sampled into the "presentation" event topic.
    QJsonObject state() const;
    bool setNotes(int slideIdx, const QString &notes);
    QByteArray thumbnail(int slideIdx) const;
    // Renders the master view with its camera as the thumbnail of slideIdx. The thumbnail is set
    // when the frame has been rendered; false when it cannot be requested.
    Q_INVOKABLE bool captureThumbnail(int slideIdx);
    // Saves the presentation to the file it was loaded from or last saved to.
    bool save();
    // The recently loaded presentations: {files: [{path, name}]}, the newest first.
    QJsonObject recentPresentations() const;
    // Opens path in C-Play if it is one of the recent presentations, else false.
    bool openRecentPresentation(const QString &path);
    // Opens the web view fullscreen on the screen in a browser app window (no tabs or address bar),
    // or in the default browser when no Chromium based browser is found.
    // A view open in a browser app window is closed first, so each view has one window.
    Q_INVOKABLE void openInBrowser(int screenIdx, const QString &view = QStringLiteral("presenter"));
    // Closes the browser app window of the web view opened with openInBrowser.
    Q_INVOKABLE void closeInBrowser(const QString &view = QStringLiteral("presenter"));

Q_SIGNALS:
    void layersRendererChanged();
    void serverChanged();
    void browserViewsChanged();

private:
    // The slide at idx, nullptr when idx is not a slide (the master is not one).
    LayersModel *slideAt(int idx) const;
    void onSnapshotReady(const QImage &image);

    QPointer<SlidesModel> m_slides;
    QPointer<LayersRendererQtItem> m_layersRenderer;
    // The slide the requested snapshot becomes the thumbnail of.
    QPointer<LayersModel> m_thumbnailSlide;
    struct BrowserView {
        QPointer<QProcess> process;
        int screenIdx = -1;
    };
    // The browser app windows opened with openInBrowser, by view.
    QHash<QString, BrowserView> m_browserViews;
    // Changes each time C-Play starts, so the presenter view restarts its timer.
    const QString m_session;
    bool m_serverRunning = false;
    int m_port = 7007;
};

#endif // PRESENTERCONTROLLER_H
