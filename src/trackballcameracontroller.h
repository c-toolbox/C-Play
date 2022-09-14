#ifndef TRACKBALLCAMERACONTROLLER_H
#define TRACKBALLCAMERACONTROLLER_H

#include <Qt3DExtras/QAbstractCameraController>
#include <QPoint>
#include <QSize>
#include <Qt3DCore/QTransform>

class TrackballCameraController : public Qt3DExtras::QAbstractCameraController
{
    Q_OBJECT
public:
    Q_PROPERTY(QSize windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged)
    Q_PROPERTY(float trackballSize READ trackballSize WRITE setTrackballSize NOTIFY trackballSizeChanged)
    Q_PROPERTY(float rotationSpeed READ rotationSpeed WRITE setRotationSpeed NOTIFY rotationSpeedChanged)
    Q_PROPERTY(QVector3D rotationXYZ READ rotationXYZ WRITE setRotationXYZ NOTIFY rotationXYZChanged)

    TrackballCameraController(Qt3DCore::QNode *parent = nullptr);

    QSize windowSize() const
    {
        return m_windowSize;
    }

    float trackballSize() const
    {
        return m_trackballSize;
    }

    float rotationSpeed() const
    {
        return m_rotationSpeed;
    }

    QVector3D rotationXYZ() const
    {
        return m_rotationXYZ;
    }

public slots:
    void setWindowSize(QSize windowSize)
    {
        if (m_windowSize == windowSize)
            return;

        m_windowSize = windowSize;
        emit windowSizeChanged(m_windowSize);
    }

    void setTrackballSize(float trackballSize)
    {
        if (qFuzzyCompare(m_trackballSize, trackballSize))
            return;

        m_trackballSize = trackballSize;
        emit trackballSizeChanged(m_trackballSize);
    }

    void setRotationSpeed(float rotationSpeed)
    {
        if (qFuzzyCompare(m_rotationSpeed, rotationSpeed))
            return;

        m_rotationSpeed = rotationSpeed;
        emit rotationSpeedChanged(m_rotationSpeed);
    }

    void setRotationXYZ(QVector3D rotationXYZ)
    {
        if (m_rotationXYZ == rotationXYZ)
            return;

        m_rotationXYZ = rotationXYZ;
        emit rotationXYZChanged();
    }

    void toggleRotationTimer();

signals:
    void windowSizeChanged(QSize windowSize);
    void trackballSizeChanged(float trackballSize);
    void rotationSpeedChanged(float rotationSpeed);
    void rotationXYZChanged();

protected:
    void moveCamera(const Qt3DExtras::QAbstractCameraController::InputState &state, float dt) override;
    QVector3D projectToTrackball(const QPoint &screenCoords) const;
    void createRotation(const QPoint &firstPoint,
                               const QPoint &nextPoint, QVector3D &dir, float &angle);

private:
    QVector3D m_rotationXYZ;
    QQuaternion m_lastRotation;
    QPoint m_mouseLastPosition, m_mouseCurrentPosition;
    QSize m_windowSize;
    float m_trackballRadius = 1.0f;
    float m_panSpeed = 1.0f;
    float m_zoomSpeed = 1.0f;
    float m_rotationSpeed = 1.0f;
    float m_zoomCameraLimit = 1.0f;
    float m_trackballSize = 1.0f;
    QTimer* rotationTimer;
};

#endif // TRACKBALLCAMERACONTROLLER_H
