/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tcpcontrolcommandadapter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QRegularExpression>
#include <QStringDecoder>
#include <cmath>

void TcpControlCommandAdapter::parseMessage(const QByteArray &payload) {
    const auto reject = [this](const QString &reason) { Q_EMIT protocolError(reason); };
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString decoded = decoder.decode(payload);
    Q_UNUSED(decoded)
    if (decoder.hasError()) {
        reject(tr("TCP command is not valid UTF-8"));
        return;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        reject(tr("TCP command must be a JSON object"));
        return;
    }
    const auto object = document.object();
    const auto opValue = object.value(QStringLiteral("operation"));
    const auto paramValue = object.value(QStringLiteral("parameter"));
    if (!opValue.isString() || (!paramValue.isUndefined() && !paramValue.isString())
        || object.size() != (paramValue.isUndefined() ? 1 : 2)) {
        reject(tr("TCP command requires operation and an optional string parameter"));
        return;
    }
    const QString operation = opValue.toString();
    const QString parameter = paramValue.toString();
    static const QSet<QString> noParameter {
        QStringLiteral("Play"), QStringLiteral("Pause"), QStringLiteral("Stop"),
        QStringLiteral("Rewind"), QStringLiteral("FadeVolumeDown"), QStringLiteral("FadeVolumeUp"),
        QStringLiteral("FadeImageDown"), QStringLiteral("FadeImageUp"),
        QStringLiteral("OrientationAndSpinReset"), QStringLiteral("RunSurfaceTransition")
    };
    static const QSet<QString> booleans {
        QStringLiteral("SetSyncVolumeVisibilityFading"), QStringLiteral("SpinPitchUp"),
        QStringLiteral("SpinPitchDown"), QStringLiteral("SpinYawLeft"), QStringLiteral("SpinYawRight"),
        QStringLiteral("SpinRollCW"), QStringLiteral("SpinRollCCW")
    };
    static const QSet<QString> selections {
        QStringLiteral("LoadFromAudioTracks"), QStringLiteral("LoadFromPlaylist"),
        QStringLiteral("LoadFromSections"), QStringLiteral("LoadFromSlides")
    };
    bool valid = false;
    if (noParameter.contains(operation)) {
        valid = parameter.isEmpty();
    } else if (booleans.contains(operation)) {
        valid = parameter == QStringLiteral("true") || parameter == QStringLiteral("false")
            || parameter == QStringLiteral("1") || parameter == QStringLiteral("0");
    } else if (selections.contains(operation)) {
        // The dispatcher accepts either an index or an exact name.
        bool numeric = false;
        const int index = parameter.toInt(&numeric);
        static const QRegularExpression integer(QStringLiteral("^[+-]?[0-9]+$"));
        valid = !parameter.trimmed().isEmpty()
            && (integer.match(parameter.trimmed()).hasMatch() ? numeric && index >= 0 : true);
    } else if (operation == QStringLiteral("Seek") || operation == QStringLiteral("SetVolume")) {
        bool numeric = false;
        const int value = parameter.toInt(&numeric);
        valid = numeric && (operation == QStringLiteral("Seek") || (value >= 0 && value <= 100));
    } else if (operation == QStringLiteral("SetPosition") || operation == QStringLiteral("SetSpeed")
        || operation == QStringLiteral("SetBackgroundVisibility")
        || operation == QStringLiteral("SetForegroundVisibility")
        || operation == QStringLiteral("SetNodeWindowsOpacity")) {
        bool numeric = false;
        const double value = parameter.toDouble(&numeric);
        valid = numeric && std::isfinite(value);
        if (operation == QStringLiteral("SetSpeed"))
            valid = valid && value > 0;
        else if (operation == QStringLiteral("SetPosition"))
            valid = valid && value >= 0; // Seconds, not normalized playback position.
        else
            valid = valid && value >= 0 && value <= 1;
    } else {
        reject(tr("Unsupported TCP control operation: %1").arg(operation));
        return;
    }
    if (!valid) {
        reject(tr("Invalid parameter for TCP control operation: %1").arg(operation));
        return;
    }
    Q_EMIT commandReceived(operation, parameter);
}
