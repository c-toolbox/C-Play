import Qt3D.Core 2.15
import Qt3D.Render 2.15
import Qt3D.Input 2.15
import Qt3D.Extras 2.15

import QtQuick 2.0 as QQ2
import TrackballCameraController 1.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import com.georgefb.haruna 1.0
import Haruna.Components 1.0

Entity{
    id: root
    property alias camera: camera
    property color ambientStrengthPlanet: "#ffffff"
    property real shininessSpecularMap: 0.0

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, 1.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    TrackballCameraController{
        id: trackBallController
        camera: camera
        windowSize: Qt.size(window.width, window.height)
        rotationSpeed: mpv.rotationSpeed

        onRotationXYZChanged: {
            if(mpv.gridToMapOn === 2) {
                mpv.rotateX = rotationXYZ.x;
                mpv.rotateY = rotationXYZ.y;
                mpv.rotateZ = rotationXYZ.z;
            }
            else {
                rotationXYZ.x = 0;
                rotationXYZ.y = 0;
                rotationXYZ.z = 0;
                camera.upVector = Qt.vector3d( 0.0, 1.0, 0.0 );
                camera.position = Qt.vector3d( 0.0, 0.0, 1.0 );
            }
        }

        Shortcut {
            sequence: "Ctrl+E"
            onActivated: toggleRotationTimer();
        }
    }

    components: [
        RenderSettings {
            activeFrameGraph: ForwardRenderer {
                camera: camera
                clearColor: "transparent"
            }
        },
        InputSettings { }
    ]

    PhongMaterial {
        id: phongMaterial
    }

    Texture2D {
       id: earthDiffuseTexture
       TextureImage {
           source: "qrc:/qml/Images/earthmap2k.jpg"
           mirrored: false
       }
    }

    Texture2D {
       id: earthNormalMap
       TextureImage {
           source: "qrc:/qml/Images/earthnormal2k.jpg"
           mirrored: false
       }
    }

    Texture2D {
       id: earthSpecularTexture
       TextureImage {
           source: "qrc:/qml/Images/earthspec2k.jpg"
           mirrored: false
       }
    }

    DiffuseSpecularMaterial {
        id: earthMaterial
        ambient: ambientStrengthPlanet
        diffuse: earthDiffuseTexture
        specular: earthSpecularTexture
        normal: earthNormalMap
        shininess: shininessSpecularMap
        alphaBlending: true
    }

    SphereMesh  {
        id: sphereMesh
        radius: 0.2
        generateTangents: true
        rings: 128
        slices: 128
    }

    Transform {
        id: sphereTransform
        rotation: fromEulerAngles(VideoSettings.surfaceRotateX-45, VideoSettings.surfaceRotateY-180, VideoSettings.surfaceRotateZ+180)
    }

    Entity {
        id: sphereEntity
        components: [ sphereMesh, earthMaterial, sphereTransform ]
    }
}
