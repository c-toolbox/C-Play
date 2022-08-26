import Qt3D.Core 2.15
import Qt3D.Render 2.15
import Qt3D.Input 2.15
import Qt3D.Extras 2.15

import QtQuick 2.0 as QQ2
import TrackballCameraController 1.0
import QtQuick.Window 2.11

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
        position: Qt.vector3d( 0.0, 0.0, 5.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    TrackballCameraController{
        camera: camera
        windowSize: Qt.size(window.width, window.height)
        rotationSpeed: 0.1
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
        radius: 1.0
        generateTangents: true
        rings: 128
        slices: 128
    }

    Transform {
        id: sphereTransform
        //scale3D: Qt.vector3d(1.5, 1, 0.5)
        rotation: fromAxisAndAngle(Qt.vector3d(0, 0, 1), 180)
    }

    Entity {
        id: sphereEntity
        components: [ sphereMesh, earthMaterial, sphereTransform ]
    }
}
