/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layerqtitem.h"
#include "application.h"
#include "layersmodel.h"
#include "audiosettings.h"
#include "presentationsettings.h"
#include "gridsettings.h"
#include "subtitlesettings.h"
#include "slidesmodel.h"
#include "track.h"

#ifdef PDF_SUPPORT
#include <layers/pdflayer.h>
#endif
#include <layers/textlayer.h>
#include <layers/mpvlayer.h>
#include <layers/controllayer.h>
#include <layers/restlayer.h>
#include <layers/imagelayer.h>

#include <QOpenGLContext>
#include <QQuickGraphicsDevice>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/qquickwindow.h>
#include <array>

static void* get_proc_address_qopengl_v1(void* ctx, const char* name) {
    Q_UNUSED(ctx)

    QOpenGLContext* oglCtx = QOpenGLContext::currentContext();
    if (!oglCtx)
        return nullptr;

    return reinterpret_cast<void*>(oglCtx->getProcAddress(QByteArray(name)));
}

static void* get_proc_address_qopengl_v2(const char* name, void* ctx) {
    Q_UNUSED(ctx)

   QOpenGLContext* oglCtx = QOpenGLContext::currentContext();
    if (!oglCtx)
        return nullptr;

    return reinterpret_cast<void*>(oglCtx->getProcAddress(QByteArray(name)));
}

LayerQtItem::LayerQtItem()
    : m_layerIdx(-1), m_layer(nullptr), m_ownsLayer(false), m_updatingLayer(true), m_renderer(nullptr), m_audioTracksModel(new TracksModel), m_timer(nullptr),
    m_viewOffset(0, 0), m_viewSize(0, 0), m_roiOffset(0, 0), m_roiSize(0, 0) {
    connect(this, &QQuickItem::windowChanged, this, &LayerQtItem::handleWindowChanged);
}

int LayerQtItem::layerIdx() const{
    return m_layerIdx;
}

void LayerQtItem::setLayerIdx(int idx) {
    m_layerIdx = idx;

    BaseLayer* nl = nullptr;
    if (Application::isCreated() && m_layerIdx >= 0)
        nl = Application::instance().slidesModel()->selectedSlide()->layer(m_layerIdx);

    if (nl == nullptr) {
        if(m_layer) {
            m_layer->setShouldUpdateFrame(false);
        }
        m_layerIdx = -1;
        m_layer = nullptr;
        Q_EMIT layerChanged();
        Q_EMIT layerValueChanged();
        return;
    }
    if (nl != m_layer) {
        if (m_layer) {
            m_layer->setShouldUpdateFrame(false);
        }
        m_layer = nl;
        m_layer->setShouldPreLoad(true);
        Q_EMIT layerChanged();
        Q_EMIT layerValueChanged();
        if (window()) {
            window()->update();
        }
    }
}

int LayerQtItem::layerStereoMode() const{
    if (m_layer)
        return m_layer->stereoMode();
    else
        return PresentationSettings::defaultStereoModeForLayers();
}

void LayerQtItem::setLayerStereoMode(int mode) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->stereoMode() != static_cast<uint8_t>(mode)) {
            Q_EMIT layerNeedsSave();
        }
        m_layer->setStereoMode(static_cast<uint8_t>(mode));
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerGridMode() const{
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->gridMode();
    if (m_layer)
        return m_layer->gridMode();
    else
        return PresentationSettings::defaultGridModeForLayers();
}

void LayerQtItem::setLayerGridMode(int mode) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->gridMode() != static_cast<uint8_t>(mode)) {
            Q_EMIT layerNeedsSave();
        }
        target->setGridMode(static_cast<uint8_t>(mode));
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerVisibility() const{
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return static_cast<int>(sub->alpha() * 100.f);
    if (m_layer)
        return static_cast<int>(m_layer->alpha() * 100.f);
    else
        return PresentationSettings::defaultLayerVisibility();
}

void LayerQtItem::setLayerVisibility(int value) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        target->setAlpha(static_cast<float>(value) * 0.01f);
        Q_EMIT layerValueChanged();
    }
}

bool LayerQtItem::layerHasAudio() const {
    if (m_layer)
        return m_layer->hasAudio();
    else
        return false;
}

int LayerQtItem::layerAudioId() const {
    if (m_layer)
        return m_layer->audioId();
    else
        return -1;
}

void LayerQtItem::setLayerAudioId(int value) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->audioId() != value) {
            m_layer->setAudioId(value);
            Q_EMIT layerValueChanged();
        }
    }
}

int LayerQtItem::layerVolume() const {
    if (m_layer)
        return m_layer->volume();
    else
        return 0;
}

void LayerQtItem::setLayerVolume(int value) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->volume() != value) {
            Q_EMIT layerNeedsSave();
        }
        m_layer->setVolume(value);
        Q_EMIT layerValueChanged();
    }
}

bool LayerQtItem::layerPause() const {
    if (m_layer)
        return m_layer->pause();
    else
        return 0.0;
}

void LayerQtItem::setLayerPause(bool value) {
    if (m_layer && m_layer->isEnabled()) {
        m_layer->setPause(value);
        Q_EMIT layerPositionChanged();
    }
}

double LayerQtItem::layerPosition() const {
    if (m_layer)
        return m_layer->position();
    else
        return 0.0;
}

void LayerQtItem::setLayerPosition(double value) {
    if (m_layer && m_layer->isEnabled()) {
        m_layer->setPosition(value);
        Q_EMIT layerPositionChanged();
    }
}

double LayerQtItem::layerDuration() const {
    if (m_layer)
        return m_layer->duration();
    else
        return 0.0;
}

double LayerQtItem::layerRemaining() const {
    if (m_layer)
        return m_layer->remaining();
    else
        return 0.0;
}

int LayerQtItem::layerEofMode() const {
    if (m_layer && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        return mpvLayer->eofMode();
    }
    return -1;
}

void LayerQtItem::setLayerEofMode(int value) {
    if (m_layer && m_layer->isEnabled() && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        if (mpvLayer->eofMode() != value) {
            mpvLayer->setEOFMode(value);
            Q_EMIT layerNeedsSave();
        }
        Q_EMIT layerValueChanged();
    }
}

bool LayerQtItem::layerLoopTimeEnabled() const {
    if (m_layer && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        return mpvLayer->loopTimeEnabled();
    }
    return false;
}

void LayerQtItem::setLayerLoopTimeEnabled(bool value) {
    if (m_layer && m_layer->isEnabled() && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        if (mpvLayer->loopTimeEnabled() != value) {
            mpvLayer->setLoopTime(mpvLayer->loopTimeA(), mpvLayer->loopTimeB(), value);
            Q_EMIT layerNeedsSave();
        }
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerLoopTimeA() const {
    if (m_layer && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        return mpvLayer->loopTimeA();
    }
    return 0.0;
}

void LayerQtItem::setLayerLoopTimeA(double value) {
    if (m_layer && m_layer->isEnabled() && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        if (mpvLayer->loopTimeA() != value) {
            mpvLayer->setLoopTime(value, mpvLayer->loopTimeB(), mpvLayer->loopTimeEnabled());
            Q_EMIT layerNeedsSave();
        }
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerLoopTimeB() const {
    if (m_layer && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        return mpvLayer->loopTimeB();
    }
    return 0.0;
}

void LayerQtItem::setLayerLoopTimeB(double value) {
    if (m_layer && m_layer->isEnabled() && (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO)) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layer);
        if (mpvLayer->loopTimeB() != value) {
            mpvLayer->setLoopTime(mpvLayer->loopTimeA(), value, mpvLayer->loopTimeEnabled());
            Q_EMIT layerNeedsSave();
        }
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerPage() const {
#ifdef PDF_SUPPORT
    if (m_layer && m_layer->type() == BaseLayer::PDF) {
        PdfLayer* pdfLayer = static_cast<PdfLayer*>(m_layer);
        return pdfLayer->page();
    }
    else
        return -1;
#else
    return -1;
#endif
}

void LayerQtItem::setLayerPage(int value) {
#ifdef PDF_SUPPORT
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::PDF) {
        PdfLayer* pdfLayer = static_cast<PdfLayer*>(m_layer);
        if (pdfLayer->page() != value) {
            Q_EMIT layerNeedsSave();
        }
        pdfLayer->setPage(value);
        Q_EMIT layerValueChanged();
    }
#endif
}

int LayerQtItem::layerNumPages() const {
#ifdef PDF_SUPPORT
    if (m_layer && m_layer->type() == BaseLayer::PDF) {
        PdfLayer* pdfLayer = static_cast<PdfLayer*>(m_layer);
        return pdfLayer->numPages();
    }
    else
        return -1;
#else
    return -1;
#endif
}

double LayerQtItem::layerRotatePitch() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->rotate().x;
    if (m_layer)
        return m_layer->rotate().x;
    else
        return 0.0;
}

void LayerQtItem::setLayerRotatePitch(double x) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        glm::vec3 rot = target->rotate();
        if (!qFuzzyCompare((double)rot.x, x)) {
            Q_EMIT layerNeedsSave();
        }
        rot.x = x;
        target->setRotate(rot);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerRotateYaw() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->rotate().y;
    if (m_layer)
        return m_layer->rotate().y;
    else
        return 0.0;
}

void LayerQtItem::setLayerRotateYaw(double y)
{
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        glm::vec3 rot = target->rotate();
        if (!qFuzzyCompare((double)rot.y, y)) {
            Q_EMIT layerNeedsSave();
        }
        rot.y = y;
        target->setRotate(rot);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerRotateRoll() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->rotate().z;
    if (m_layer)
        return m_layer->rotate().z;
    else
        return 0.0;
}

void LayerQtItem::setLayerRotateRoll(double z) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        glm::vec3 rot = target->rotate();
        if (!qFuzzyCompare((double)rot.z, z)) {
            Q_EMIT layerNeedsSave();
        }
        rot.z = z;
        target->setRotate(rot);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneWidth() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeWidth();
    if (m_layer)
        return m_layer->planeWidth();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneWidth(double pW) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeWidth() != pW) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneWidth(pW);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneHeight() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeHeight();
    if (m_layer)
        return m_layer->planeHeight();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneHeight(double pH) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeHeight() != pH) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneHeight(pH);
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerPlaneAspectRatio() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeAspectRatio();
    if (m_layer)
        return m_layer->planeAspectRatio();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneAspectRatio(int parc) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeAspectRatio() != static_cast<uint8_t>(parc)) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneAspectRatio(static_cast<uint8_t>(parc));
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneAzimuth() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeAzimuth();
    if (m_layer)
        return m_layer->planeAzimuth();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneAzimuth(double pA) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeAzimuth() != pA) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneAzimuth(pA);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneElevation() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeElevation();
    if (m_layer)
        return m_layer->planeElevation();
    else
        return GridSettings::plane_Elevation_Degrees();
}

void LayerQtItem::setLayerPlaneElevation(double pE) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeElevation() != pE) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneElevation(pE);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneRoll() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeRoll();
    if (m_layer)
        return m_layer->planeRoll();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneRoll(double pR) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeRoll() != pR) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneRoll(pR);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneDistance() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeDistance();
    if (m_layer)
        return m_layer->planeDistance();
    else
        return GridSettings::plane_Distance_CM();
}

void LayerQtItem::setLayerPlaneDistance(double pD) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeDistance() != pD) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneDistance(pD);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneHorizontal() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeHorizontal();
    if (m_layer)
        return m_layer->planeHorizontal();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneHorizontal(double pH) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeHorizontal() != pH) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneHorizontal(pH);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneVertical() const {
    BaseLayer* sub = selectedSubLayerPtr();
    if (sub)
        return sub->planeVertical();
    if (m_layer)
        return m_layer->planeVertical();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneVertical(double pV) {
    BaseLayer* target = selectedSubLayerPtr();
    if (!target) target = m_layer;
    if (target && target->isEnabled()) {
        if (target->planeVertical() != pV) {
            Q_EMIT layerNeedsSave();
        }
        target->setPlaneVertical(pV);
        Q_EMIT layerValueChanged();
    }
}

bool LayerQtItem::layerRoiEnabled() {
    if (m_layer)
        return m_layer->roiEnabled();
    else
        return false;
}

void LayerQtItem::setLayerRoiEnabled(bool value) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->roiEnabled() != value) {
            Q_EMIT layerNeedsSave();
        }
        m_layer->setRoiEnabled(value);
        Q_EMIT layerValueChanged();
        if (value)
            updateRoi();
    }
}

QString LayerQtItem::layerTypeName() {
    if (m_layer)
        return QString::fromStdString(m_layer->typeName());
    else
        return QStringLiteral("");
}

QString LayerQtItem::layerTitle() {
    if (m_layer)
        return QString::fromStdString(m_layer->title());
    else
        return QStringLiteral("");
}

void LayerQtItem::setLayerTitle(QString value) {
    if (m_layer && m_layer->isEnabled()) {
        if (QString::fromStdString(m_layer->title()) != value) {
            m_layer->setTitle(value.toStdString());
            Q_EMIT layerValueChanged();
        }
    }
}

QSize LayerQtItem::textureSize() {
    if (m_layer) {
        return QSize(static_cast<int>(m_layer->width()), static_cast<int>(m_layer->height()));
    }
    else {
        return m_viewSize;
    }
}

QPoint LayerQtItem::viewOffset() {
    return m_viewOffset;
}

void LayerQtItem::setViewOffset(QPoint p) {
    m_viewOffset = p;
    Q_EMIT viewChanged();
}

QSize LayerQtItem::viewSize() {
    return m_viewSize;
}

void LayerQtItem::setViewSize(QSize s) {
    m_viewSize = s;
    Q_EMIT viewChanged();
}

QPoint LayerQtItem::roiOffset() {
    return m_roiOffset;
}

void LayerQtItem::setRoiOffset(QPoint p) {
    if (m_layer && m_layer->isEnabled()) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.x = glm::max(static_cast<float>(p.x() - m_viewOffset.x()) / static_cast<float>(m_viewSize.width()), 0.f);
        
        //UI uses Top-Left coordinate system, while we want to store in Bottom-Left
        float yFromTop = glm::max(static_cast<float>(p.y() - m_viewOffset.y()) / static_cast<float>(m_viewSize.height()), 0.f);
        currentRoi.y = 1.f - currentRoi.w - yFromTop;
        //currentRoi.y = glm::max(static_cast<float>(p.y() - m_viewOffset.y()) / static_cast<float>(m_viewSize.height()), 0.f);

        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

QSize LayerQtItem::roiSize() {
    return m_roiSize;
}

void LayerQtItem::setRoiSize(QSize s) {
    if (m_layer && m_layer->isEnabled()) {
        glm::vec4 currentRoi = m_layer->roi();
        //UI uses Top-Left coordinate system, while we store in Bottom-Left
        float yFromTop = 1.f - currentRoi.w - currentRoi.y;
        currentRoi.z = glm::min(static_cast<float>(s.width()) / static_cast<float>(m_viewSize.width()), 1.f - currentRoi.x);
        currentRoi.w = glm::min(static_cast<float>(s.height()) / static_cast<float>(m_viewSize.height()), 1.f - yFromTop);
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

void LayerQtItem::setRoi(QPoint offset, QSize size) {
    if (m_layer && m_layer->isEnabled()) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.x = glm::max(static_cast<float>(offset.x() - m_viewOffset.x()) / static_cast<float>(m_viewSize.width()), 0.f);
        //UI uses Top-Left coordinate system, while we want to store in Bottom-Left
        float yFromTop = glm::max(static_cast<float>(offset.y() - m_viewOffset.y()) / static_cast<float>(m_viewSize.height()), 0.f);
        currentRoi.z = glm::min(static_cast<float>(size.width()) / static_cast<float>(m_viewSize.width()), 1.f - currentRoi.x);
        currentRoi.w = glm::min(static_cast<float>(size.height()) / static_cast<float>(m_viewSize.height()), 1.f - yFromTop);
        currentRoi.y = 1.f - currentRoi.w - yFromTop;
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

void LayerQtItem::dragLeft(double Xdiff) {
    setRoi(m_roiOffset + QPoint(Xdiff, 0), m_roiSize + QSize(-Xdiff, 0));
}

void LayerQtItem::dragRight(double Xdiff) {
    setRoi(m_roiOffset, m_roiSize + QSize(Xdiff, 0));
}

void LayerQtItem::dragTop(double Ydiff) {
    setRoi(m_roiOffset + QPoint(0, Ydiff), m_roiSize + QSize(0, -Ydiff));
}

void LayerQtItem::dragBottom(double Ydiff) {
    setRoi(m_roiOffset, m_roiSize + QSize(0, Ydiff));
}

QPoint LayerQtItem::roiTexOffset() {
    if (m_layer) {
        QPoint realRoiOffset;
        realRoiOffset.setX(static_cast<int>(m_layer->roi().x * m_layer->width()));
        realRoiOffset.setY(static_cast<int>(m_layer->roi().y * m_layer->height()));
        return realRoiOffset;
    }
    else {
        return m_roiOffset;
    }
}

void LayerQtItem::setRoiTexOffset(QPoint p) {
    if (m_layer && m_layer->isEnabled()) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.x = glm::max(static_cast<float>(p.x()) / static_cast<float>(m_layer->width()), 0.f);
        //This value should already be Bottom-Left
        currentRoi.y = glm::max(static_cast<float>(p.y()) / static_cast<float>(m_layer->height()), 0.f);
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

QSize LayerQtItem::roiTexSize() {
    if (m_layer) {
        QSize realRoiSize;
        realRoiSize.setWidth(static_cast<int>(m_layer->roi().z * m_layer->width()));
        realRoiSize.setHeight(static_cast<int>(m_layer->roi().w * m_layer->height()));
        return realRoiSize;
    }
    else {
        return m_roiSize;
    }
}

void LayerQtItem::setRoiTexSize(QSize s) {
    if (m_layer && m_layer->isEnabled()) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.z = glm::min(static_cast<float>(s.width()) / static_cast<float>(m_layer->width()), 1.f);
        currentRoi.w = glm::min(static_cast<float>(s.height()) / static_cast<float>(m_layer->height()), 1.f);
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

void LayerQtItem::updateView() {
    if (m_renderer) {
        m_viewOffset = m_renderer->viewOffset() / window()->devicePixelRatio();
        m_viewSize = m_renderer->viewSize() / window()->devicePixelRatio();
        updateRoi();
        Q_EMIT viewChanged();
    }
}

void LayerQtItem::updateRoi() {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        m_roiOffset.setX((static_cast<int>(currentRoi.x * static_cast<float>(m_viewSize.width()))) + m_viewOffset.x());

        //UI uses Top-Left coordinate system, while we store in Bottom-Left
        float yFromTop = 1.f - currentRoi.w - currentRoi.y;
        m_roiOffset.setY((static_cast<int>(yFromTop * static_cast<float>(m_viewSize.height()))) + m_viewOffset.y());
        //m_roiOffset.setY((static_cast<int>(currentRoi.y * static_cast<float>(m_viewSize.height()))) + m_viewOffset.y());

        m_roiSize.setWidth(static_cast<int>(currentRoi.z * static_cast<float>(m_viewSize.width())));
        m_roiSize.setHeight(static_cast<int>(currentRoi.w * static_cast<float>(m_viewSize.height())));
        Q_EMIT roiChanged();
    }
}

void LayerQtItem::updateEnabled(bool value) {
    m_updatingLayer = value;
}

TracksModel* LayerQtItem::audioTracksModel() const {
    return m_audioTracksModel;
}

void LayerQtItem::loadTracks() {
    if (m_layer) {
        m_audioTracksModel->setTracks(m_layer->audioTracks());
    }
    else {
        m_audioTracksModel->setTracks(nullptr);
    }

    Q_EMIT audioTracksModelChanged();
}

QString LayerQtItem::layerText() const {
    if (m_layer && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        return QString::fromStdString(textLayer->text());
    }
    return QStringLiteral("");
}

void LayerQtItem::setLayerText(QString text) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        if (QString::fromStdString(textLayer->text()) != text) {
            textLayer->setText(text.toStdString());
            Q_EMIT layerValueChanged();
        }
    }
}

QString LayerQtItem::layerTextFontName() const {
    if (m_layer && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        return QString::fromStdString(textLayer->fontName());
    }
    return SubtitleSettings::subtitleFontFamily();
}

void LayerQtItem::setLayerTextFontName(QString name) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        if (QString::fromStdString(textLayer->fontName()) != name) {
            textLayer->setFont(name.toStdString());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

int LayerQtItem::layerTextFontSize() const {
    if (m_layer && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        return textLayer->fontSize();
    }
    return SubtitleSettings::subtitleFontSize();
}

void LayerQtItem::setLayerTextFontSize(int size) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        if (textLayer->fontSize() != size) {
            textLayer->setFontSize(size);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QColor LayerQtItem::layerTextFontColor() const {
    if (m_layer && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        return QColor(QString::fromStdString(textLayer->colorHex()));
    }
    return QColor(SubtitleSettings::subtitleColor());
}

void LayerQtItem::setLayerTextFontColor(QColor color) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        if (QColor(QString::fromStdString(textLayer->colorHex())) != color) {
            textLayer->setColor(color.name().toStdString(), color.redF(), color.greenF(), color.blueF());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

int LayerQtItem::layerTextAlignment() const {
    if (m_layer && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        return textLayer->alignment();
    }
    return SubtitleSettings::subtitleAlignment();
}

void LayerQtItem::setLayerTextAlignment(int align) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        if (textLayer->alignment() != align) {
            textLayer->setAlignment(align);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QSize LayerQtItem::layerTextRenderSize() const {
    if (m_layer && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        return QSize(textLayer->width(), textLayer->height());
    }
    return QSize(SubtitleSettings::subtitleTextureWidth(), SubtitleSettings::subtitleTextureHeight());
}

void LayerQtItem::setLayerTextRenderSize(QSize size) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::TEXT) {
        TextLayer* textLayer = static_cast<TextLayer*>(m_layer);
        QSize currentSize(textLayer->width(), textLayer->height());
        if (currentSize != size) {
            textLayer->setTextureSize(size.width(), size.height());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

bool LayerQtItem::layerQRCodeDetectionEnabled() const {
    if (m_layer)
        return m_layer->isQRCodeDetectionEnabled();
    return false;
}

void LayerQtItem::setLayerQRCodeDetectionEnabled(bool enabled) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->isQRCodeDetectionEnabled() != enabled) {
            m_layer->setQRCodeDetectionEnabled(enabled);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

int LayerQtItem::layerTextureDivisionMode() const {
    if (m_layer)
        return m_layer->textureDivisionMode();
    return 0;
}

void LayerQtItem::setLayerTextureDivisionMode(int mode) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->textureDivisionMode() != mode) {
            m_layer->setTextureDivisionMode(mode);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

int LayerQtItem::layerDivisionGrid() const {
    if (m_layer)
        return m_layer->textureDivisionGrid();
    return 0;
}

void LayerQtItem::setLayerDivisionGrid(int grid) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->textureDivisionGrid() != grid) {
            m_layer->setTextureDivisionGrid(grid);
            m_selectedSubLayer = 0; // Reset selection when grid changes
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QStringList LayerQtItem::layerSubLayerNames() const {
    QStringList names;
    names.append(QStringLiteral("Original"));
    if (!m_layer)
        return names;

    int mode = m_layer->textureDivisionMode();
    if (mode == 2) {
        // Division mode: generate names from grid dimensions even before sublayers are created
        int grid = m_layer->textureDivisionGrid();
        if(grid <= 0 || grid > 6)
            return names; // Invalid grid index, return default name
        int cols = 1, rows = 1;
        switch (grid) {
        case 1: cols = 1; rows = 2; break;
        case 2: cols = 2; rows = 1; break;
        case 3: cols = 2; rows = 2; break;
        case 4: cols = 2; rows = 3; break;
        case 5: cols = 3; rows = 2; break;
        case 6: cols = 3; rows = 3; break;
        default: break;
        }
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                names.append(QStringLiteral("Div_%1_%2").arg(col).arg(row));
            }
        }
    } else if (mode == 1 && m_layer->hasSubLayers()) {
        // QR/ImPres mode: use actual sublayer names
        auto& subLayers = m_layer->getSubLayers();
        for (const auto& sub : subLayers) {
            if (sub) {
                names.append(QString::fromStdString(sub->title()));
            }
        }
    }
    return names;
}

int LayerQtItem::layerSelectedSubLayer() const {
    return m_selectedSubLayer;
}

void LayerQtItem::setLayerSelectedSubLayer(int index) {
    if (m_selectedSubLayer != index) {
        m_selectedSubLayer = index;
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerSubLayerCount() const {
    // Return the expected sublayer count based on the division grid index,
    // even before sublayers are lazily created by the render thread.
    if (!m_layer)
        return 0;
    int mode = m_layer->textureDivisionMode();
    if (mode == 2) {
        // Division mode: count from grid index
        int grid = m_layer->textureDivisionGrid();
        switch (grid) {
        case 0: return 1;  // 1x1
        case 1: return 2;  // 1x2
        case 2: return 2;  // 2x1
        case 3: return 4;  // 2x2
        case 4: return 6;  // 2x3
        case 5: return 6;  // 3x2
        case 6: return 9;  // 3x3
        default: return 1;
        }
    } else if (mode == 1) {
        // QR/ImPres mode: count from actual sublayers
        if (m_layer->hasSubLayers())
            return static_cast<int>(m_layer->getSubLayers().size());
    }
    return 0;
}

QRectF LayerQtItem::layerSelectedSubLayerRoi() const {
    if (!m_layer || m_selectedSubLayer <= 0)
        return QRectF();

    int mode = m_layer->textureDivisionMode();
    if (mode == 2) {
        // Division mode: compute ROI from grid index even if sublayers don't exist yet
        int grid = m_layer->textureDivisionGrid();
        int cols = 1, rows = 1;
        switch (grid) {
        case 1: cols = 1; rows = 2; break;
        case 2: cols = 2; rows = 1; break;
        case 3: cols = 2; rows = 2; break;
        case 4: cols = 2; rows = 3; break;
        case 5: cols = 3; rows = 2; break;
        case 6: cols = 3; rows = 3; break;
        default: cols = 1; rows = 1; break;
        }
        int subIdx = m_selectedSubLayer - 1; // 0-based sublayer index
        if (subIdx < 0 || subIdx >= cols * rows)
            return QRectF();
        int col = subIdx % cols;
        int row = subIdx / cols;
        double cellW = 1.0 / static_cast<double>(cols);
        double cellH = 1.0 / static_cast<double>(rows);
        return QRectF(col * cellW, row * cellH, cellW, cellH);
    } else if (mode == 1 && m_layer->hasSubLayers()) {
        // QR mode: read ROI from the actual sublayer
        int subIdx = m_selectedSubLayer - 1;
        auto& subs = m_layer->getSubLayers();
        if (subIdx >= 0 && subIdx < static_cast<int>(subs.size())) {
            auto& sub = subs[subIdx];
            if (sub && sub->roiEnabled()) {
                glm::vec4 r = sub->roi();
                return QRectF(r.x, r.y, r.z, r.w);
            }
        }
    }
    return QRectF();
}

BaseLayer* LayerQtItem::selectedSubLayerPtr() const {
    if (!m_layer || m_selectedSubLayer <= 0 || !m_layer->hasSubLayers())
        return nullptr;
    int subIdx = m_selectedSubLayer - 1;
    auto& subs = m_layer->getSubLayers();
    if (subIdx >= 0 && subIdx < static_cast<int>(subs.size()))
        return subs[subIdx].get();
    return nullptr;
}

bool LayerQtItem::layerFlipY() const {
    if (m_layer)
        return m_layer->flipY();
    return false;
}

void LayerQtItem::setLayerFlipY(bool flip) {
    if (m_layer && m_layer->isEnabled()) {
        if (m_layer->flipY() != flip) {
            m_layer->setFlipY(flip);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QString LayerQtItem::layerOperation() const {
    if (m_layer && m_layer->type() == BaseLayer::CONTROL) {
        ControlLayer* controlLayer = static_cast<ControlLayer*>(m_layer);
        return QString::fromStdString(controlLayer->operation());
    }
    return QStringLiteral("");
}

void LayerQtItem::setLayerOperation(QString op) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::CONTROL) {
        ControlLayer* controlLayer = static_cast<ControlLayer*>(m_layer);
        if (QString::fromStdString(controlLayer->operation()) != op) {
            controlLayer->setOperation(op.toStdString());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QString LayerQtItem::layerParameter() const {
    if (m_layer && m_layer->type() == BaseLayer::CONTROL) {
        ControlLayer* controlLayer = static_cast<ControlLayer*>(m_layer);
        return QString::fromStdString(controlLayer->parameter());
    }
    return QStringLiteral("");
}

void LayerQtItem::setLayerParameter(QString param) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::CONTROL) {
        ControlLayer* controlLayer = static_cast<ControlLayer*>(m_layer);
        if (QString::fromStdString(controlLayer->parameter()) != param) {
            controlLayer->setParameter(param.toStdString());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QString LayerQtItem::layerRestUrl() const {
    if (m_layer && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        return QString::fromStdString(restLayer->url());
    }
    return QStringLiteral("");
}

void LayerQtItem::setLayerRestUrl(QString url) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        if (QString::fromStdString(restLayer->url()) != url) {
            restLayer->setUrl(url.toStdString());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

int LayerQtItem::layerRestMethod() const {
    if (m_layer && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        return restLayer->method();
    }
    return 0;
}

void LayerQtItem::setLayerRestMethod(int method) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        if (restLayer->method() != method) {
            restLayer->setMethod(method);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

QString LayerQtItem::layerRestParameters() const {
    if (m_layer && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        return QString::fromStdString(restLayer->parameters());
    }
    return QStringLiteral("");
}

void LayerQtItem::setLayerRestParameters(QString params) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        if (QString::fromStdString(restLayer->parameters()) != params) {
            restLayer->setParameters(params.toStdString());
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

bool LayerQtItem::layerRestIgnoreStatus() const {
    if (m_layer && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        return restLayer->ignoreStatus();
    }
    return false;
}

void LayerQtItem::setLayerRestIgnoreStatus(bool ignore) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(m_layer);
        if (restLayer->ignoreStatus() != ignore) {
            restLayer->setIgnoreStatus(ignore);
            Q_EMIT layerValueChanged();
            Q_EMIT layerNeedsSave();
        }
    }
}

void LayerQtItem::setLayerImageSequence(const QString &directory, const QString &prefix,
                                        int digitCount, const QString &suffix,
                                        int startIndex, int stopIndex, int step,
                                        int delayMs, bool loop) {
    if (m_layer && m_layer->isEnabled() && m_layer->type() == BaseLayer::IMAGE) {
        ImageLayer* imageLayer = static_cast<ImageLayer*>(m_layer);
        ImageLayer::SequenceParams params;
        params.directory = directory.toStdString();
        params.prefix = prefix.toStdString();
        params.suffix = suffix.toStdString();
        params.digitCount = digitCount;
        params.startIndex = startIndex;
        params.stopIndex = stopIndex;
        params.step = step;
        params.delayMs = delayMs;
        params.loop = loop;
        imageLayer->setSequenceParams(params);
        Q_EMIT layerValueChanged();
        Q_EMIT layerNeedsSave();
    }
}

void LayerQtItem::handleWindowChanged(QQuickWindow *win) {
    if (win) {
        connect(win, &QQuickWindow::beforeSynchronizing, this, &LayerQtItem::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &LayerQtItem::cleanup, Qt::DirectConnection);
        win->setColor(Qt::black);

        if (m_timer == nullptr) {
            m_timer = new QTimer();
            m_timer->setInterval((1.0f / 60.0f) * 1000.0f);

            connect(m_timer, &QTimer::timeout, win, &QQuickWindow::update);

            m_timer->start();
        }
    }
}

void LayerQtItem::cleanup() {
    delete m_renderer;
    m_renderer = nullptr;

    if (m_layer && m_ownsLayer) {
        delete m_layer;
        m_layer = nullptr;
    }
}

class CleanupJob : public QRunnable {
public:
    CleanupJob(LayerQtOpenGLObject *renderer) : m_renderer(renderer) {}
    void run() override { delete m_renderer; }

private:
    LayerQtOpenGLObject *m_renderer;
};

void LayerQtItem::releaseResources() {
    window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

LayerQtOpenGLObject::LayerQtOpenGLObject(QObject* parent)
    : QObject(parent), m_window(nullptr) {
}

LayerQtOpenGLObject::~LayerQtOpenGLObject() {
    delete m_program;
}

void LayerQtOpenGLObject::setWindowSize(const QSize &size) {
    m_windowSize = size;
}

void LayerQtOpenGLObject::setViewportSize(const QSize &size) {
    m_viewportSize = size;
}

void LayerQtOpenGLObject::setPosition(const QPoint &position) {
    m_position = position;
}

void LayerQtOpenGLObject::setWindow(QQuickWindow *window) {
    m_window = window;
}

void LayerQtOpenGLObject::setUpdateLayer(bool value) {
    m_updateLayer = value;
}

void LayerQtOpenGLObject::setItemVisible(bool visible) {
    m_itemVisible = visible;
    if (m_layer) {
        m_layer->setShouldUpdateFrame(visible);
    }
}

void LayerQtOpenGLObject::setOwnsLayer(bool ownsLayer) {
    m_ownsLayer = ownsLayer;
}

BaseLayer *LayerQtOpenGLObject::layer() {
    return m_layer;
}

void LayerQtOpenGLObject::setLayer(BaseLayer *l) {
    m_layer = l;
}

QPoint LayerQtOpenGLObject::viewOffset() {
    return m_viewOffset;
}

QSize LayerQtOpenGLObject::viewSize() {
    return m_viewSize;
}

void LayerQtOpenGLObject::calculateViewParameters() {
    int viewW = m_viewportSize.width();
    int viewH = m_viewportSize.height();
    int offsetX = 0;
    int offsetY = 0;
    float ratioViewport = static_cast<float>(viewW) / static_cast<float>(viewH);
    float ratioLayer = static_cast<float>(m_layer->width()) / static_cast<float>(m_layer->height());
    if (ratioLayer > ratioViewport) {
        // Use full width of viewport
        if (ratioLayer > 1.f) {
            viewH = static_cast<int>(static_cast<float>(m_viewportSize.width()) / ratioLayer);
        }
        else {
            viewH = static_cast<int>(static_cast<float>(m_viewportSize.width()) * ratioLayer);
        }
        offsetY = static_cast<int>(0.5f * (static_cast<float>(m_viewportSize.height() - viewH)));
    }
    else if (ratioLayer < ratioViewport) {
        // Use full height of viewport
        if (ratioLayer > 1.f) {
            viewW = static_cast<int>(static_cast<float>(m_viewportSize.height()) * ratioLayer);
        }
        else {
            viewW = static_cast<int>(static_cast<float>(m_viewportSize.height()) / ratioLayer);
        }
        offsetX = static_cast<int>(0.5f * (static_cast<float>(m_viewportSize.width() - viewW)));
    }

    if (offsetX != m_viewOffset.x() || offsetY != m_viewOffset.y()
        || viewW != m_viewSize.width() || viewH != m_viewSize.height()) {
        m_viewOffset.setX(offsetX);
        m_viewOffset.setY(offsetY);
        m_viewSize.setWidth(viewW);
        m_viewSize.setHeight(viewH);
        Q_EMIT viewChanged();
    }
}

void LayerQtItem::createLayer(int type, QString filepath){
    BaseLayer* newLayer = BaseLayer::createLayer(true, type, get_proc_address_qopengl_v1, get_proc_address_qopengl_v2);
    newLayer->setFilePath(filepath.toStdString());
    newLayer->initialize();

    if (m_layer && m_ownsLayer) {
        delete m_layer;
    }

    m_layer = newLayer;
    m_ownsLayer = true;
}

void LayerQtItem::start() {
    if (m_layer) {
        if (m_layer->type() == BaseLayer::CONTROL || m_layer->type() == BaseLayer::REST) {
            m_layer->start();
        } else if (m_ownsLayer) {
            m_layer->setShouldUpdate(true);
            m_layer->start();
        }
    }
}

void LayerQtItem::stop() {
    if (m_layer) {
        if (m_layer->type() == BaseLayer::CONTROL || m_layer->type() == BaseLayer::REST) {
            m_layer->stop();
        } else if (m_ownsLayer) {
            m_layer->setShouldUpdate(false);
            m_layer->stop();
        }
    }
}

void LayerQtItem::sync() {
    if (!m_renderer) {
        m_renderer = new LayerQtOpenGLObject();
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &LayerQtOpenGLObject::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &LayerQtOpenGLObject::paint, Qt::DirectConnection);
        connect(m_renderer, &LayerQtOpenGLObject::viewChanged, this, &LayerQtItem::updateView);
    }
    m_renderer->setLayer(m_layer);
    m_renderer->setWindowSize(window()->size() * window()->devicePixelRatio());
    m_renderer->setViewportSize(this->size().toSize() * window()->devicePixelRatio());
    m_renderer->setPosition(this->position().toPoint());
    m_renderer->setWindow(window());
    m_renderer->setUpdateLayer(m_updatingLayer);
    m_renderer->setItemVisible(isVisible());
    m_renderer->setOwnsLayer(m_ownsLayer);

    if (m_layer) {
        if (m_layer->type() == BaseLayer::LayerType::VIDEO || m_layer->type() == BaseLayer::LayerType::AUDIO) {
            Q_EMIT layerPositionChanged();
        }
    }
}

void LayerQtOpenGLObject::init() {
    if (!m_program) {
        QSGRendererInterface *rif = m_window->rendererInterface();
        Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (!ctx)
            return;

        // Point the private window at the same OpenGL context that is current on
        // the parent window's render thread. This guarantees that any GL objects
        // (textures, FBOs) created inside update() share the same namespace as
        // LayerQtOpenGLObject and LayersRendererQtOpenGLObject, which are also
        // driven by beforeRendering of the same parent window.
        m_window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(ctx));

        initializeOpenGLFunctions();

        m_program = new QOpenGLShaderProgram();

        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex,
                                                    "attribute highp vec3 vertices;"
                                                    "attribute highp vec2 texcoords;"
                                                    "varying highp vec2 coords;"
                                                    "uniform bool flipY;"
                                                    "void main() {"
                                                    "    gl_Position = vec4(vertices, 1.0);"
                                                    "    coords = flipY ? vec2(texcoords.x, 1.0-texcoords.y) : texcoords;"
                                                    "}");
        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment,
                                                    "varying highp vec2 coords;"
                                                    "uniform sampler2D tex;"
                                                    "void main() {"
                                                    "    gl_FragColor = texture2D(tex, coords);"
                                                    "}");

        m_program->bindAttributeLocation("vertices", 0);
        m_program->bindAttributeLocation("texcoords", 1);

        m_program->link();

        constexpr std::array<const float, 36> QuadVerts = {
            //     x     y     z      u    v      r    g    b    a
            -1.f, -1.f, -1.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f,
            1.f, -1.f, -1.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f,
            -1.f, 1.f, -1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

        m_vao.create();
        m_vao.bind();
        // setting up buffers
        m_vbo.create();
        m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_vbo.bind();
        m_vbo.allocate(QuadVerts.data(), 36 * sizeof(float));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            9 * sizeof(float),
            reinterpret_cast<void *>(3 * sizeof(float)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            4,
            GL_FLOAT,
            GL_FALSE,
            9 * sizeof(float),
            reinterpret_cast<void *>(5 * sizeof(float)));

        m_vao.release();
    }
}

void LayerQtOpenGLObject::paint() {
    if (!m_layer || !m_layer->isEnabled() || !m_itemVisible || !m_program) {
        return;
    }

    m_window->beginExternalCommands();
   
    // If layered not own, update is handled in the layermodel by it's owner
    if(m_ownsLayer) {
        m_layer->update();
    }

    if (!m_layer->ready()) {
        m_window->endExternalCommands();
        return;
    }

    if (m_ownsLayer) {
        if (m_layer->shouldUpdate() && m_layer->pause()) {
            m_layer->enableAudio(AudioSettings::enableAudioOnMaster());
            m_layer->start();
        }
        if (!m_layer->shouldUpdate() && !m_layer->pause()) {
            m_layer->stop();
        }
    }

    calculateViewParameters();

    m_program->bind();

    m_program->enableAttributeArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_layer->textureId());

    m_program->setUniformValue("tex", 1);
    m_program->setUniformValue("flipY", (m_layer->flipY() ? 1 : 0 ));

    glViewport(m_viewOffset.x(), m_viewOffset.y(), m_viewSize.width(), m_viewSize.height());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_vao.bind();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vao.release();

    m_program->disableAttributeArray(0);
    m_program->release();

    glDisable(GL_BLEND);

    m_window->endExternalCommands();

    glViewport(0, 0, m_windowSize.width(), m_windowSize.height());

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}
