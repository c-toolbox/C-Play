/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "capturemodel.h"
#include <capture/capturebackend.h>
#include <capture/capturepresets.h>

CaptureModel::CaptureModel(QObject *parent)
    : QObject(parent) {
    const CaptureGpuInfo gpu = detectCaptureGpu();
    m_gpuName = QString::fromStdString(gpu.name);
    m_professionalGpu = gpu.professional;
    for (const std::string &sdk : CaptureBackend::availableSdks())
        m_availableSdks.append(QString::fromStdString(sdk));
}

QStringList CaptureModel::inputNames() const {
    return m_inputNames;
}

QStringList CaptureModel::inputSources() const {
    return m_inputSources;
}

QStringList CaptureModel::availableSdks() const {
    return m_availableSdks;
}

QString CaptureModel::gpuName() const {
    return m_gpuName;
}

bool CaptureModel::professionalGpu() const {
    return m_professionalGpu;
}

void CaptureModel::updateInputList() {
    m_inputNames.clear();
    m_inputSources.clear();
    m_availableSdks.clear();
    for (const CaptureInputInfo &info : CaptureBackend::listInputs()) {
        m_inputNames.append(QString::fromStdString(info.name));
        m_inputSources.append(QString::fromStdString(info.source.toString()));
    }
    for (const std::string &sdk : CaptureBackend::availableSdks())
        m_availableSdks.append(QString::fromStdString(sdk));
    Q_EMIT inputsChanged();
}

QString CaptureModel::sourceString(const QString &backend, int input, const QString &ganging, bool directGpu, bool audio) const {
    CaptureSource src;
    src.backend = backend.toStdString();
    src.input = input;
    src.ganging = ganging.toStdString();
    src.directGpu = directGpu;
    src.audio = audio;
    return QString::fromStdString(src.toString());
}

CapturePresetsModel::CapturePresetsModel(QObject *parent)
    : QAbstractListModel(parent) {
}

int CapturePresetsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_titles.size();
}

QVariant CapturePresetsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !checkIndex(index))
        return QVariant();
    if (role == titleRole)
        return m_titles.at(index.row());
    if (role == sourceRole)
        return m_sources.at(index.row());
    return QVariant();
}

QHash<int, QByteArray> CapturePresetsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[titleRole] = "title";
    roles[sourceRole] = "source";
    return roles;
}

void CapturePresetsModel::updatePresetsList() {
    CapturePresets presets;
    const std::string path = CapturePresets::findDefaultFilePath();
    if (!path.empty())
        presets.loadFromFile(path);

    beginResetModel();
    m_titles.clear();
    m_sources.clear();
    for (const CapturePresets::Entry &e : presets.entries()) {
        if (!e.enabled)
            continue;
        m_titles.append(QString::fromStdString(e.title));
        m_sources.append(QString::fromStdString(e.source.toString()));
    }
    endResetModel();
    Q_EMIT presetsListChanged();
}

int CapturePresetsModel::numberOfPresets() const {
    return m_titles.size();
}
