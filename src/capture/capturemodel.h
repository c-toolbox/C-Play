/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CAPTUREMODEL_H
#define CAPTUREMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QStringList>

// QML facing info for the Capture layer: the capture card inputs on this machine and whether
// the GPU supports direct capture transfer (AMD DirectGMA / NVIDIA GPUDirect for Video).
class CaptureModel : public QObject {
    Q_OBJECT

public:
    explicit CaptureModel(QObject *parent = nullptr);

    // Display names and matching source strings (layer file paths) of all detected inputs.
    Q_PROPERTY(QStringList inputNames READ inputNames NOTIFY inputsChanged)
    QStringList inputNames() const;
    Q_PROPERTY(QStringList inputSources READ inputSources NOTIFY inputsChanged)
    QStringList inputSources() const;

    // Capture SDK runtimes (drivers) installed on this machine, e.g. "Datapath RGBEasy".
    Q_PROPERTY(QStringList availableSdks READ availableSdks NOTIFY inputsChanged)
    QStringList availableSdks() const;

    Q_PROPERTY(QString gpuName READ gpuName CONSTANT)
    QString gpuName() const;
    Q_PROPERTY(bool professionalGpu READ professionalGpu CONSTANT)
    bool professionalGpu() const;

    Q_INVOKABLE void updateInputList();

    // Builds a layer file path for a custom selection (input is 1-based).
    Q_INVOKABLE QString sourceString(const QString &backend, int input, const QString &ganging, bool directGpu, bool audio) const;

Q_SIGNALS:
    void inputsChanged();

private:
    QStringList m_inputNames;
    QStringList m_inputSources;
    QStringList m_availableSdks;
    QString m_gpuName;
    bool m_professionalGpu = false;
};

// QML facing list of the predefined capture setups in data/predefined-captures.json
// (see CapturePresets). Only enabled entries are listed.
class CapturePresetsModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit CapturePresetsModel(QObject *parent = nullptr);

    enum {
        titleRole = Qt::UserRole,
        sourceRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updatePresetsList();

    Q_PROPERTY(int numberOfPresets READ numberOfPresets NOTIFY presetsListChanged)
    int numberOfPresets() const;

Q_SIGNALS:
    void presetsListChanged();

private:
    QStringList m_titles;
    QStringList m_sources;
};

#endif // CAPTUREMODEL_H
