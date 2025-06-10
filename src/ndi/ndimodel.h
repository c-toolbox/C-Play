/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NDIMODEL_H
#define NDIMODEL_H

#include <QAbstractListModel>

class NDISendersModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit NDISendersModel(QObject *parent = nullptr);
    ~NDISendersModel();

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateSendersList();

    Q_PROPERTY(int numberOfSenders READ getNumberOfSenders NOTIFY sendersListChanged)
    int getNumberOfSenders();

    Q_INVOKABLE QString getNDIVersionString();

Q_SIGNALS:
    void sendersListChanged();

private:
    QStringList m_NDIsenders;
};

class PortAudioModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit PortAudioModel(QObject* parent = nullptr);
    ~PortAudioModel();

    struct PortAudioDevice {
        PortAudioDevice(int d, QString dn, QString an)
            : id(d), deviceName(dn), apiName(an)
        {
        }

        int id;
        QString deviceName;
        QString apiName;
    };

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_PROPERTY(int currentDevice READ getCurrentDevice WRITE setCurrentDevice NOTIFY currentDeviceChanged)
    int getCurrentDevice();
    void setCurrentDevice(int idx);

    Q_INVOKABLE void updatePortAudioList();

    Q_PROPERTY(int indexOfCurrentDevice READ getIndexOfCurrentDevice NOTIFY portAudioListChanged)
    int getIndexOfCurrentDevice();

    Q_PROPERTY(QString deviceInfo READ getDeviceInfo NOTIFY currentDeviceChanged)
    QString getDeviceInfo();

    Q_PROPERTY(QString deviceName READ getDeviceName NOTIFY currentDeviceChanged)
    QString getDeviceName();
    
    Q_PROPERTY(QString apiName READ getApiName NOTIFY currentDeviceChanged)
    QString getApiName();

Q_SIGNALS:
    void portAudioListChanged();
    void currentDeviceChanged();

private:
    QList<PortAudioDevice> m_audioOutputs;
    int m_currentDevice;
    bool m_paIsInitialized;
};

#endif // NDIMODEL_H
