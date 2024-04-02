import QtQuick 2.12
import org.ctoolbox.cplay 1.0

Image {
    id: root

    width: parent.width
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    anchors.left: PlaylistSettings.position === "left" ? playSections.right : parent.left
    anchors.right: PlaylistSettings.position === "right" ? playList.left : parent.right
    anchors.top: parent.top

    source: playerController.foregroundImageFileUrl()
    fillMode: Image.PreserveAspectFit
    opacity: playerController.foregroundVisibility()

    Connections {
        target: playerController

        function onForegroundImageChanged(){
            root.source = playerController.foregroundImageFileUrl()
        }
        function onForegroundVisibilityChanged(){
            root.opacity = playerController.foregroundVisibility()
        }
    }
}
