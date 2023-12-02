import QtQuick 2.12
import com.georgefb.haruna 1.0

Image {
    id: root

    width: parent.width
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    anchors.left: PlaylistSettings.position === "left" ? playSections.right : parent.left
    anchors.right: PlaylistSettings.position === "right" ? playList.left : parent.right
    anchors.top: parent.top

    source: playerController.backgroundImageFileUrl()
    fillMode: Image.PreserveAspectFit
    anchors.fill: parent
    opacity: playerController.backgroundVisibility()

    Connections {
        target: playerController

        function onBackgroundImageChanged(){
            root.source = playerController.backgroundImageFileUrl()
        }
        function onBackgroundVisibilityChanged(){
            root.opacity = playerController.backgroundVisibility()
        }
    }
}
