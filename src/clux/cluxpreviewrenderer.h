/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLUXPREVIEWRENDERER_H
#define CLUXPREVIEWRENDERER_H

#include <QObject>
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include <memory>
#include <vector>

class CLuxPattern;
class CLuxStaticPattern;

// Local stand-in for the C-Lux engine's tick loop: it advances the same pattern stack the
// server holds and composites frames exactly like previewBlend() - the solid color layer
// under the enabled patterns, source-over - so preview mode can render from local state
// instead of following the live frame feed. Scene cross-dissolves are not reproduced: a
// local scene change switches at once to its final state rather than easing over the
// server's sceneTransition.
class CLuxPreviewRenderer : public QObject {
    Q_OBJECT

public:
    explicit CLuxPreviewRenderer(QObject* parent = nullptr);
    // Defined in the .cpp, where the pattern classes are complete: destroying the
    // unique_ptr members needs their full definitions.
    ~CLuxPreviewRenderer() override;

    // How many lights the ring has; pattern instances are (re)built when it changes.
    void setLightCount(int nLights);
    // The serialized /patterns entries, in stack order (bottom to top).
    void setPatterns(const QVariantList& serialized);
    // The solid color layer the server composites under the patterns. Color changes ease
    // over the server's solidColorTransition; enabling or disabling switches at once.
    void setSolidColor(int r, int g, int b, bool enabled);

    // Start or stop ticking at the server's tick rate (~30 fps).
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

Q_SIGNALS:
    // nLights*3 ints (r, g, b per light): the unmasked composite of the current stack.
    void frameReady(const QVariantList& frame);

private:
    // Rebuild the pattern instances to match m_serialized and m_nLights, keeping running
    // patterns in place when they keep their name, as the server's stacks do.
    void rebuild();
    void onTick();

    QTimer m_timer;
    QElapsedTimer m_elapsed;

    int m_nLights = 0;
    QVariantList m_serialized;
    // std::vector: QList requires copyable elements and cannot hold unique_ptr.
    std::vector<std::unique_ptr<CLuxPattern>> m_patterns;
    std::unique_ptr<CLuxStaticPattern> m_solidColor;

    int m_solidColorValue[3] = {0, 0, 0};
    bool m_solidEnabled = false;
    // Pattern types without a local implementation, warned about once each.
    QSet<QString> m_unsupportedTypes;
};

#endif // CLUXPREVIEWRENDERER_H
