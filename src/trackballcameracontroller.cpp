#include "trackballcameracontroller.h"
#include <Qt3DRender/QCamera>
#include <QVector2D>
#include <Qt3DInput/QAction>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/QAxis>
#include <Qt3DInput/QAnalogAxisInput>
#include <Qt3DInput/QMouseDevice>
#include <Qt3DInput/QKeyboardDevice>
#include <Qt3DInput/QMouseHandler>
#include <Qt3DInput/QKeyboardHandler>
#include <Qt3DCore/QTransform>
#include <QtMath>
#include <QTimer>

TrackballCameraController::TrackballCameraController(Qt3DCore::QNode *parent)
    : Qt3DExtras::QAbstractCameraController (parent)
{
    Qt3DInput::QMouseHandler *mouseHandler = new Qt3DInput::QMouseHandler(this);
    mouseHandler->setSourceDevice(mouseDevice());

    QObject::connect(mouseHandler, &Qt3DInput::QMouseHandler::pressed,
                     [this](Qt3DInput::QMouseEvent *pressedEvent) {
        pressedEvent->setAccepted(true);
        m_mouseLastPosition = QPoint(pressedEvent->x(), pressedEvent->y());
        m_mouseCurrentPosition = m_mouseLastPosition;
    });

    QObject::connect(mouseHandler, &Qt3DInput::QMouseHandler::positionChanged,
                     [this](Qt3DInput::QMouseEvent *positionChangedEvent) {
        positionChangedEvent->setAccepted(true);
        m_mouseCurrentPosition = QPoint(positionChangedEvent->x(),
                                              positionChangedEvent->y());
    });

    m_lastRotation = QQuaternion(0.f, 0.f, 0.f, 0.f);
    m_continousRotation = false;
}

void TrackballCameraController::setRotationXYZ(QVector3D rotationXYZ)
{
    if (m_rotationXYZ == rotationXYZ)
        return;

    auto theCamera = camera();
    if (theCamera != nullptr) {
        QQuaternion rotation = QQuaternion::fromEulerAngles(rotationXYZ);
        theCamera->rotateAboutViewCenter(rotation);

        m_rotationXYZ = rotationXYZ;
        emit rotationXYZChanged();
    }
}

void TrackballCameraController::setAbsoluteRotation(QVector3D rotationXYZ) {
    if (m_rotationXYZ == rotationXYZ)
        return;

    auto theCamera = camera();
    if (theCamera != nullptr) {
        QVector3D diff = m_rotationXYZ - rotationXYZ;
        QQuaternion rotation = QQuaternion::fromEulerAngles(diff);
        theCamera->rotateAboutViewCenter(rotation);
        m_rotationXYZ = rotationXYZ;
    }
}

void TrackballCameraController::stopRotation() {
    m_lastRotation = QQuaternion(0.f, 0.f, 0.f, 0.f);
    m_continousRotation = false;
}

bool TrackballCameraController::performRotation() {
    auto theCamera = camera();
    if (theCamera != nullptr) {
        if (m_continousRotation) {
            theCamera->rotateAboutViewCenter(m_lastRotation);
            m_rotationXYZ = theCamera->transform()->rotation().toEulerAngles();
            emit rotationXYZChanged();
            return true;
        }
    }
    return false;
}

QVector3D TrackballCameraController::projectToTrackball(const QPoint &screenCoords) const
{
    float sx = screenCoords.x(), sy = m_windowSize.height() - screenCoords.y();

    QVector2D p2d(sx / m_windowSize.width() - 0.5f, sy / m_windowSize.height() - 0.5f);

    float z = 0.0f;
    float r2 = m_trackballSize * m_trackballSize;
    if (p2d.lengthSquared() <= r2 * 0.5f){
        z = sqrt(r2 - p2d.lengthSquared());
    }else{
        z = r2 * 0.5f / p2d.length();
    }
    QVector3D p3d(p2d, z);

    return p3d;
}

float clamp(float x)
{
    return x > 1? 1 : (x < -1? -1 : x);
}

void TrackballCameraController::createRotation(const QPoint &firstPoint, const QPoint &nextPoint,
                                               QVector3D &dir, float &angle)
{
    auto lastPos3D = projectToTrackball(firstPoint).normalized();
    auto currentPos3D = projectToTrackball(nextPoint).normalized();

    // Compute axis of rotation:
    dir = QVector3D::crossProduct(currentPos3D, lastPos3D);

    // Approximate rotation angle:
    angle = acos(clamp(QVector3D::dotProduct(currentPos3D, lastPos3D)));
}

void TrackballCameraController::moveCamera(const Qt3DExtras::QAbstractCameraController::InputState &state, float)
{
    auto theCamera = camera();

    if(theCamera == nullptr)
        return;

    if(state.leftMouseButtonActive){
        QVector3D dir;
        float angle;
        createRotation(m_mouseLastPosition, m_mouseCurrentPosition, dir, angle);

        auto currentRotation = theCamera->transform()->rotation();

        auto rotatedAxis = currentRotation.rotatedVector(dir);
        angle *= m_rotationSpeed;

        QQuaternion rotation = QQuaternion::fromAxisAndAngle(rotatedAxis, angle * M_1_PI * 180);
        //theCamera->rotateAboutViewCenter(rotation);

        if (!rotation.isIdentity())
            m_continousRotation = true;
        else
            m_continousRotation = false;

        m_lastRotation = rotation;

    }else if (state.rightMouseButtonActive) {
        auto offset = m_mouseCurrentPosition - m_mouseLastPosition;

        QVector3D dir;
        float angle;
        createRotation(m_mouseLastPosition, m_mouseCurrentPosition, dir, angle);
        angle *= m_rotationSpeed;

        if (dir.y() < 0.f)
            angle *= -1.f;

        QQuaternion rotation = QQuaternion::fromAxisAndAngle(theCamera->viewVector(), angle * M_1_PI * 180);
        //theCamera->rotateAboutViewCenter(rotation);

        if (!rotation.isIdentity())
            m_continousRotation = true;
        else
            m_continousRotation = false;

        m_lastRotation = rotation;

    }
    m_mouseLastPosition = m_mouseCurrentPosition;
}
