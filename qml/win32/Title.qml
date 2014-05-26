import QtQuick 1.0
import "UI" 1.0

Rectangle {
    width: 520
    height: 480
    color: "#868284"
    Config {id:config}

    Rectangle {
        anchors { right: parent.right; rightMargin: 1; top: parent.top; topMargin: 1 }
        width: 28; height: 28
        radius: height
        color: p.advanced ? "#999999" : "transparent"
        border.width: 2
        smooth: true
        MouseArea {
            anchors.fill: parent
            onClicked: p.advanced = !p.advanced
        }
    }

    Row {
        id: titleRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.top: parent.top; anchors.topMargin: 7
        spacing: (parent.width - title1.w - /*title2.w - title3.w -*/ title4.w - title5.w - title6.w) / (3)

        property int curObj: 3;
        TitleObject {
            id: title1
            obj: 0
            text: "Extract"
            property int w: visible ? width : 0
        }
        /*TitleObject {
            visible: p.advanced
            id: title2
            obj: 1
            text: "Tools"
            property int w: visible ? width : 0
        }
        TitleObject {
            visible: p.advanced
            id: title3
            obj: 2
            text: "Boot"
            property int w: visible ? width : 0
        }*/
        TitleObject {
            id: title4
            obj: 3
            text: "Search"
            property int w: visible ? width : 0
        }
        TitleObject {
            id: title5
            obj: 4
            text: "Backup"
            property int w: visible ? width : 0
        }
        TitleObject {
            id: title6
            obj: 5
            text: "Install"
            property int w: visible ? width : 0
        }
    }
    Loader {
        visible: titleRow.curObj == 0
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
        source: "extract.qml"
    }
    Loader {
        visible: titleRow.curObj == 1
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
        source: "downloader.qml"
    }/*
    Loader {
        visible: titleRow.curObj == 2
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
        source: "boot.qml"
    }*/
    Loader {
        visible: titleRow.curObj == 3
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
		source: "main.qml"
    }
    Loader {
        visible: titleRow.curObj == 4 && (i.knownBattery > -1 && !i.wrongPassBlock)
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
        source: "backup.qml"
    }
    Loader {
        visible: titleRow.curObj == 5 && (i.knownBattery > -1 && !i.wrongPassBlock)
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
        source: "installer.qml"
    }
    Loader {
        visible: titleRow.curObj >= 4 && (i.knownBattery <= -1 || i.wrongPassBlock)
        anchors.top: parent.top
        anchors.topMargin: 15 + config.notificationFontSize
        width: parent.width;
        height: parent.height - 5 - config.notificationFontSize;
        source: "usbconnect.qml"
    }
}
