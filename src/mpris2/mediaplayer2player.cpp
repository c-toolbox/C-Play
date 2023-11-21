/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mediaplayer2player.h"
#include "_debug.h"
#include "mpvobject.h"
#include "httpserverthread.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>

MediaPlayer2Player::MediaPlayer2Player(QObject *parent)
    : QDBusAbstractAdaptor(parent), httpServer(new HttpServerThread(this))
{
    connect(this, &MediaPlayer2Player::mpvChanged,
            this, &MediaPlayer2Player::setupConnections);

    setupHttpServer();
}

void MediaPlayer2Player::setupConnections()
{
    if (!m_mpv) {
        return;
    }
}

void MediaPlayer2Player::setupHttpServer()
{
    httpServer->setupHttpServer();

    connect(httpServer, &HttpServerThread::finished, httpServer, &QObject::deleteLater);
    connect(httpServer, &HttpServerThread::pauseMedia, this, &MediaPlayer2Player::Pause);
    connect(httpServer, &HttpServerThread::playMedia, this, &MediaPlayer2Player::Play);
    connect(httpServer, &HttpServerThread::rewindMedia, this, &MediaPlayer2Player::Rewind);
    connect(httpServer, &HttpServerThread::setPosition, this, &MediaPlayer2Player::SetPosition);
    connect(httpServer, &HttpServerThread::setVolume, this, &MediaPlayer2Player::SetVolume);
    connect(httpServer, &HttpServerThread::fadeVolumeDown, this, &MediaPlayer2Player::FadeVolumeDown);
    connect(httpServer, &HttpServerThread::fadeVolumeUp, this, &MediaPlayer2Player::FadeVolumeUp);
    connect(httpServer, &HttpServerThread::fadeImageDown, this, &MediaPlayer2Player::FadeImageDown);
    connect(httpServer, &HttpServerThread::fadeImageUp, this, &MediaPlayer2Player::FadeImageUp);
    connect(httpServer, &HttpServerThread::loadFromPlaylist, this, &MediaPlayer2Player::LoadFromPlaylist);
    connect(httpServer, &HttpServerThread::loadFromSections, this, &MediaPlayer2Player::LoadFromSections);
    connect(httpServer, &HttpServerThread::spinPitchUp, this, &MediaPlayer2Player::SpinPitchUp);
    connect(httpServer, &HttpServerThread::spinPitchDown, this, &MediaPlayer2Player::SpinPitchDown);
    connect(httpServer, &HttpServerThread::spinYawLeft, this, &MediaPlayer2Player::SpinYawLeft);
    connect(httpServer, &HttpServerThread::spinYawRight, this, &MediaPlayer2Player::SpinYawRight);
    connect(httpServer, &HttpServerThread::spinRollCCW, this, &MediaPlayer2Player::SpinRollCCW);
    connect(httpServer, &HttpServerThread::spinRollCW, this, &MediaPlayer2Player::SpinRollCW);
    connect(httpServer, &HttpServerThread::orientationAndSpinReset, this, &MediaPlayer2Player::OrientationAndSpinReset);
    connect(httpServer, &HttpServerThread::runSurfaceTransistion, this, &MediaPlayer2Player::RunSurfaceTransistion);

    httpServer->start();
}

void MediaPlayer2Player::propertiesChanged(const QString &property, const QVariant &value)
{
    QDBusMessage msg = QDBusMessage::createSignal(QStringLiteral("/org/mpris/MediaPlayer2"),
                                                  QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("PropertiesChanged"));

    QVariantMap properties;
    properties[property] = value;

    msg << QString("org.mpris.MediaPlayer2.Player");
    msg << properties;
    msg << QStringList();

    QDBusConnection::sessionBus().send(msg);
}

void MediaPlayer2Player::Next()
{
    Q_EMIT next();
}

void MediaPlayer2Player::Previous()
{
    Q_EMIT previous();
}

void MediaPlayer2Player::Pause()
{
    Q_EMIT pause();
}

void MediaPlayer2Player::PlayPause()
{
    Q_EMIT playpause();
}

void MediaPlayer2Player::Stop()
{
    Q_EMIT stop();
}

void MediaPlayer2Player::Play()
{
    Q_EMIT play();
}

void MediaPlayer2Player::Rewind()
{
    if (m_mpv) {
        m_mpv->performRewind();
    }
}

void MediaPlayer2Player::Seek(qlonglong offset)
{
    Q_EMIT seek(offset/1000/1000);
}

void MediaPlayer2Player::LoadFromPlaylist(int idx)
{
    Q_EMIT loadFromPlaylist(idx);
}

void MediaPlayer2Player::LoadFromSections(int idx)
{
    Q_EMIT loadFromSections(idx);
}

void MediaPlayer2Player::SetPosition(double pos)
{
    if (m_mpv) {
        m_mpv->setPosition(pos);
    }
}

void MediaPlayer2Player::SetVolume(int level)
{
    if (m_mpv) {
        m_mpv->setVolume(level);
    }
}

void MediaPlayer2Player::FadeVolumeDown()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeDown();
    }
}

void MediaPlayer2Player::FadeVolumeUp()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeUp();
    }
}

void MediaPlayer2Player::FadeImageDown()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageDown();
    }
}

void MediaPlayer2Player::FadeImageUp()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageUp();
    }
}

void MediaPlayer2Player::SpinPitchUp()
{
    Q_EMIT spinPitchUp();
}

void MediaPlayer2Player::SpinPitchDown()
{
    Q_EMIT spinPitchDown();
}

void MediaPlayer2Player::SpinYawLeft()
{
    Q_EMIT spinYawLeft();
}

void MediaPlayer2Player::SpinYawRight()
{
    Q_EMIT spinYawRight();
}

void MediaPlayer2Player::SpinRollCW()
{
    Q_EMIT spinRollCW();
}

void MediaPlayer2Player::SpinRollCCW()
{
    Q_EMIT spinRollCCW();
}

void MediaPlayer2Player::OrientationAndSpinReset()
{
    Q_EMIT orientationAndSpinReset();
}

void MediaPlayer2Player::RunSurfaceTransistion()
{
    Q_EMIT runSurfaceTransistion();
}


MpvObject *MediaPlayer2Player::mpv() const
{
    return m_mpv;
}

void MediaPlayer2Player::setMpv(MpvObject *mpv)
{
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
    httpServer->setMpv(mpv);
    Q_EMIT mpvChanged();
}
