/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERVIEWCONTROLLER_H
#define LAYERVIEWCONTROLLER_H

#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include <memory>

class BaseLayer;
class LayersModel;
class SlidesModel;

// Backs the layer view (web/layer_mode), served by the HTTP server at /layerview/: the slides and
// their layers, to edit where flat layers are (position and size), their volume and visibility.
// The canvas the view moves layers on (fisheye, flat, sphere or circular) is chosen in C-Play
// (PresentationSettings::layerViewCanvas). Lives on the GUI thread; the HTTP server calls it
// through queued calls.
class LayerViewController : public QObject {
    Q_OBJECT

public:
    explicit LayerViewController(QObject *parent = nullptr);

    void setSlidesModel(SlidesModel *slides);

    // Everything the layer view shows, sampled into the "layerview" event topic.
    QJsonObject state() const;
    // Sets the values in the query of row in slide (-1 is the master): plane azimuth, elevation,
    // roll, distance, horizontal, vertical, width, height, centreDegrees, widthDegrees (numbers)
    // and volume (0-100). sub > 0 addresses that duplicate of a layer in duplicate mode (1-based)
    // instead, whose volume is the layer's. Returns the layer as in state(), or an error in "error".
    QJsonObject setLayerValues(int slideIdx, int row, int sub, const QJsonObject &values);
    // Fades the layer in row of slide (or its duplicate sub) up or down, as double-clicking it in
    // C-Play does.
    bool fadeLayer(int slideIdx, int row, int sub, bool show);

private:
    LayersModel *slideAt(int idx) const;
    // The layer in row, or its duplicate sub (1-based) when sub > 0.
    std::shared_ptr<BaseLayer> layerAt(LayersModel *slide, int row, int sub) const;
    QJsonObject layerState(LayersModel *slide, int row, int sub = 0) const;

    QPointer<SlidesModel> m_slides;
};

#endif // LAYERVIEWCONTROLLER_H
