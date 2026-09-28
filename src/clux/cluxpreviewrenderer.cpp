/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cluxpreviewrenderer.h"
#include "cluxpatterns.h"

#include <QVector>

namespace {

// The server's tick rate (config.json: server.tickRate) and its solid color transition,
// kept in sync with the C-Lux defaults so the preview moves at the same pace.
constexpr int kTickIntervalMs = 1000 / 30;
constexpr double kSolidColorTransition = 1.0;

} // namespace

CLuxPreviewRenderer::CLuxPreviewRenderer(QObject* parent)
    : QObject(parent), m_timer(this) {
    connect(&m_timer, &QTimer::timeout, this, &CLuxPreviewRenderer::onTick);
}

// After the includes above, so CLuxPattern/CLuxStaticPattern are complete here.
CLuxPreviewRenderer::~CLuxPreviewRenderer() = default;

void CLuxPreviewRenderer::setLightCount(int nLights) {
    const int count = qMax(0, nLights);
    if (count == m_nLights)
        return;

    m_nLights = count;
    // Instances are sized to the old ring, so rebuild them all.
    m_patterns.clear();
    rebuild();
}

void CLuxPreviewRenderer::setPatterns(const QVariantList& serialized) {
    if (serialized == m_serialized)
        return;

    m_serialized = serialized;
    rebuild();
}

void CLuxPreviewRenderer::setSolidColor(int r, int g, int b, bool enabled) {
    const bool colorChanged = m_solidColorValue[0] != r || m_solidColorValue[1] != g
                              || m_solidColorValue[2] != b;

    m_solidColorValue[0] = qBound(0, r, 255);
    m_solidColorValue[1] = qBound(0, g, 255);
    m_solidColorValue[2] = qBound(0, b, 255);
    m_solidEnabled = enabled;

    // The server eases solid color changes over its transition time.
    if (m_solidColor && colorChanged)
        m_solidColor->fadeTo({r, g, b}, kSolidColorTransition);
}

void CLuxPreviewRenderer::start() {
    if (m_timer.isActive())
        return;

    m_elapsed.start();
    m_timer.start(kTickIntervalMs);
}

void CLuxPreviewRenderer::stop() {
    m_timer.stop();
}

void CLuxPreviewRenderer::rebuild() {
    if (m_nLights <= 0) {
        m_patterns.clear();
        m_solidColor.reset();
        return;
    }

    // The solid color layer, kept out of the pattern list like on the server.
    if (!m_solidColor || m_solidColor->lightCount() != m_nLights) {
        auto solid = std::make_unique<CLuxStaticPattern>(QStringLiteral("solid-color"), true, 1.0);
        solid->resize(m_nLights);
        solid->setColor(m_solidColorValue[0], m_solidColorValue[1], m_solidColorValue[2]);
        m_solidColor = std::move(solid);
    }

    std::vector<std::unique_ptr<CLuxPattern>> next;
    next.reserve(static_cast<size_t>(m_serialized.size()));
    for (const QVariant& v : m_serialized) {
        const QVariantMap params = v.toMap();
        const QString name = params.value(QStringLiteral("name")).toString();
        const QString type = params.value(QStringLiteral("type")).toString();

        std::unique_ptr<CLuxPattern> pattern;
        // Reuse the running instance when a pattern keeps its place in the stack, as the
        // server does: applying a scene leaves already-running patterns untouched.
        for (auto& existing : m_patterns) {
            if (!existing)
                continue; // already claimed by an earlier entry this pass
            if (existing->name() == name && existing->type() == type) {
                pattern = std::move(existing);
                break;
            }
        }

        if (!pattern) {
            pattern = createLuxPattern(params, m_nLights);
            if (!pattern && !m_unsupportedTypes.contains(type)) {
                m_unsupportedTypes.insert(type);
                qWarning() << "CLuxPreviewRenderer: no local implementation for pattern type"
                           << type << "- it renders dark in preview mode";
            }
        } else {
            // No-op when the parameters are unchanged, like the server's update().
            pattern->updateFrom(params);
        }

        if (pattern)
            next.push_back(std::move(pattern));
    }
    m_patterns = std::move(next);
}

void CLuxPreviewRenderer::onTick() {
    // Measured frame time, like the server's Date.now() delta.
    const double dt = m_elapsed.restart() / 1000.0;

    // Advance every enabled pattern in stack order, then the solid color layer - the
    // server's tick() without its scene dissolves.
    for (auto& p : m_patterns) {
        if (p->enabled())
            p->tick(dt);
    }
    if (m_solidColor)
        m_solidColor->tick(dt);

    // previewBlend(): the solid color layer under the enabled patterns, source-over.
    QVector<double> accum(m_nLights * 3, 0.0);
    if (m_solidEnabled && m_solidColor)
        m_solidColor->composite(accum);
    for (auto& p : m_patterns) {
        if (p->enabled())
            p->composite(accum);
    }

    QVariantList frame;
    frame.reserve(m_nLights * 3);
    for (double v : accum)
        frame.append(qRound(v));
    Q_EMIT frameReady(frame);
}
