/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layerviewcontroller.h"
#include "gridsettings.h"
#include "layersmodel.h"
#include "presentationsettings.h"
#include "slidesmodel.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
// The canvas names the web view knows, in the order of PresentationSettings::layerViewCanvas.
const char *const kCanvases[] = {"fisheye", "flat", "sphere", "circular"};
// Plane sizes and distances are limited like in the C-Play grid parameters.
constexpr double kMaxPlaneCm = 100000.0;
}

LayerViewController::LayerViewController(QObject *parent)
    : QObject(parent) {
}

void LayerViewController::setSlidesModel(SlidesModel *slides) {
    m_slides = slides;
}

LayersModel *LayerViewController::slideAt(int idx) const {
    if (!m_slides)
        return nullptr;
    if (idx == -1)
        return m_slides->masterSlide();
    return idx >= 0 && idx < m_slides->numberOfSlides() ? m_slides->slide(idx) : nullptr;
}

QJsonObject LayerViewController::layerState(LayersModel *slide, int row) const {
    const std::shared_ptr<BaseLayer> layer = slide->rowLayerShared(row);
    if (!layer)
        return {};

    // The size the plane is drawn with, which follows the media aspect ratio unless the plane
    // size is based on both values.
    const auto size = layer->computedPlaneSize();
    QJsonObject plane{{QStringLiteral("azimuth"), layer->planeAzimuth()},
        {QStringLiteral("elevation"), layer->planeElevation()},
        {QStringLiteral("roll"), layer->planeRoll()},
        {QStringLiteral("distance"), layer->planeDistance()},
        {QStringLiteral("horizontal"), layer->planeHorizontal()},
        {QStringLiteral("vertical"), layer->planeVertical()},
        {QStringLiteral("width"), layer->planeWidth()},
        {QStringLiteral("height"), layer->planeHeight()},
        // 0 = both values, 1 = height and media aspect ratio, 2 = width and media aspect ratio.
        {QStringLiteral("aspectRatio"), layer->planeAspectRatio()},
        {QStringLiteral("actualWidth"), size ? double(size->x) : layer->planeWidth()},
        {QStringLiteral("actualHeight"), size ? double(size->y) : layer->planeHeight()},
        {QStringLiteral("circular"), layer->planeCircular()},
        {QStringLiteral("centreDegrees"), layer->planeCentreDegrees()},
        {QStringLiteral("widthDegrees"), layer->planeWidthDegrees()}};

    return {{QStringLiteral("row"), row},
        {QStringLiteral("id"), QString::number(layer->identifier())},
        {QStringLiteral("title"), slide->layerTitle(row)},
        {QStringLiteral("type"), QString::fromStdString(BaseLayer::typeDescription(layer->type()))},
        // 0 = 2D, 1 = flat plane, 2 = dome, 3 = sphere (equirectangular), 4 = sphere (EAC).
        {QStringLiteral("gridMode"), layer->gridMode()},
        // "<slide number>. <slide name>" of the slide that owns a layer kept visible from it.
        {QStringLiteral("source"), slide->referenceSource(row)},
        {QStringLiteral("locked"), slide->isLocked(row)},
        {QStringLiteral("visibility"), qRound(layer->alpha() * 100.f)},
        {QStringLiteral("visibilityMaximum"), layer->visibilityMaximum()},
        {QStringLiteral("hasAudio"), layer->hasAudio()},
        {QStringLiteral("volume"), layer->volume()},
        {QStringLiteral("plane"), plane}};
}

QJsonObject LayerViewController::state() const {
    if (!m_slides)
        return {};

    QJsonArray slides;
    for (int i = -1; i < m_slides->numberOfSlides(); ++i) {
        LayersModel *slide = slideAt(i);
        if (!slide)
            continue;
        QJsonArray layers;
        for (int row = 0; row < slide->rowCount(); ++row) {
            const QJsonObject layer = layerState(slide, row);
            if (!layer.isEmpty())
                layers.append(layer);
        }
        slides.append(QJsonObject{{QStringLiteral("index"), i},
            {QStringLiteral("name"), slide->getLayersName()},
            {QStringLiteral("skipped"), i >= 0 && slide->slideSkipped()},
            {QStringLiteral("layers"), layers}});
    }

    const int canvas = std::clamp(PresentationSettings::layerViewCanvas(), 0, int(std::size(kCanvases)) - 1);
    const double circularWidthCm = GridSettings::circularSurfaceWidthCM();
    return {{QStringLiteral("name"), m_slides->getSlidesName()},
        {QStringLiteral("canSave"), !m_slides->getSlidesPath().isEmpty()},
        {QStringLiteral("needsSave"), m_slides->getSlidesNeedsSave()},
        {QStringLiteral("selected"), m_slides->selectedSlideIdx()},
        {QStringLiteral("triggered"), m_slides->triggeredSlideIdx()},
        {QStringLiteral("canvas"), QString::fromLatin1(kCanvases[canvas])},
        // The fisheye of the master view: the field of view and radius of the dome.
        {QStringLiteral("dome"), QJsonObject{{QStringLiteral("fov"), GridSettings::surfaceFov()},
            {QStringLiteral("radius"), GridSettings::surfaceRadius()}}},
        // The flat canvas spans the plane move ranges of the grid settings.
        {QStringLiteral("flat"), QJsonObject{{QStringLiteral("horizontalRange"), GridSettings::plane_Horizontal_Range_CM()},
            {QStringLiteral("verticalRange"), GridSettings::plane_Vertical_Range_CM()}}},
        // The circular surface mapping: 360 degrees span widthCm, the canvas aspect gives the height.
        {QStringLiteral("circular"), QJsonObject{{QStringLiteral("enabled"), GridSettings::circularEnabled()},
            {QStringLiteral("widthCm"), circularWidthCm},
            {QStringLiteral("heightCm"), circularWidthCm * GridSettings::circularCanvasHeight() / std::max(1, GridSettings::circularCanvasWidth())},
            {QStringLiteral("seamDegrees"), GridSettings::circularSeamDegrees()},
            {QStringLiteral("clockwise"), GridSettings::circularClockwise()}}},
        {QStringLiteral("slides"), slides}};
}

QJsonObject LayerViewController::setLayerValues(int slideIdx, int row, const QJsonObject &values) {
    const auto error = [](const QString &text) { return QJsonObject{{QStringLiteral("error"), text}}; };
    LayersModel *slide = slideAt(slideIdx);
    if (!slide || row < 0 || row >= slide->rowCount())
        return error(QStringLiteral("No layer %1 in slide %2").arg(row).arg(slideIdx));
    const std::shared_ptr<BaseLayer> layer = slide->rowLayerShared(row);
    if (!layer || !layer->isEnabled())
        return error(QStringLiteral("The layer cannot be changed"));

    // Everything is checked before anything changes.
    struct Setter {
        const char *name;
        double min;
        double max;
        void (BaseLayer::*set)(double);
    };
    static const Setter setters[] = {
        {"azimuth", -1e6, 1e6, &BaseLayer::setPlaneAzimuth},
        {"elevation", -1e6, 1e6, &BaseLayer::setPlaneElevation},
        {"roll", -1e6, 1e6, &BaseLayer::setPlaneRoll},
        {"distance", 0, kMaxPlaneCm, &BaseLayer::setPlaneDistance},
        {"horizontal", -kMaxPlaneCm, kMaxPlaneCm, &BaseLayer::setPlaneHorizontal},
        {"vertical", -kMaxPlaneCm, kMaxPlaneCm, &BaseLayer::setPlaneVertical},
        {"width", 0, kMaxPlaneCm, &BaseLayer::setPlaneWidth},
        {"height", 0, kMaxPlaneCm, &BaseLayer::setPlaneHeight},
        {"centreDegrees", -3600000, 3600000, &BaseLayer::setPlaneCentreDegrees},
        {"widthDegrees", 0.01, 360, &BaseLayer::setPlaneWidthDegrees}};
    QList<QPair<const Setter *, double>> changes;
    for (const Setter &setter : setters) {
        const QString key = QString::fromLatin1(setter.name);
        if (!values.contains(key))
            continue;
        bool ok = false;
        const double value = values.value(key).toVariant().toDouble(&ok);
        if (!ok || !std::isfinite(value) || value < setter.min || value > setter.max)
            return error(QStringLiteral("%1 must be a number from %2 to %3").arg(key).arg(setter.min).arg(setter.max));
        changes.append({&setter, value});
    }
    int volume = -1;
    if (values.contains(QStringLiteral("volume"))) {
        bool ok = false;
        volume = values.value(QStringLiteral("volume")).toVariant().toInt(&ok);
        if (!ok || volume < 0 || volume > 100)
            return error(QStringLiteral("volume must be from 0 to 100"));
    }
    if (!changes.isEmpty() && layer->gridMode() != BaseLayer::Plane)
        return error(QStringLiteral("Only flat layers have a position and size"));

    for (const auto &[setter, value] : changes)
        (layer.get()->*setter->set)(value);
    if (volume >= 0) {
        layer->setVolume(volume);
        // The stored level is the maximum; what plays still follows the visibility.
        layer->setVolume(layer->volumeAtAlpha(layer->alpha()), false);
    }
    if (!changes.isEmpty() || volume >= 0) {
        slide->markLayerNeedsSave(row);
        slide->updateLayer(row);
        m_slides->setNeedsSync(true);
    }
    return layerState(slide, row);
}

bool LayerViewController::fadeLayer(int slideIdx, int row, bool show) {
    LayersModel *slide = slideAt(slideIdx);
    if (!slide || row < 0 || row >= slide->rowCount())
        return false;
    const std::shared_ptr<BaseLayer> layer = slide->rowLayerShared(row);
    if (!layer || !slide->fadeLayer(layer.get(), show))
        return false;
    m_slides->setNeedsSync(true);
    return true;
}
