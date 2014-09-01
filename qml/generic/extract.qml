import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import "UI" 1.0

TabView {
    id: main
    Rectangle {
        visible: p.splitting
        anchors {bottom: parent.bottom; bottomMargin: 10; horizontalCenter: parent.horizontalCenter }
        height: 80; width: parent.width - 40; radius: 8
        z: 5;
        color: "gray"
        opacity: 0.95
        Column {
            spacing: 5
            anchors {top: parent.top; topMargin: 10; left: parent.left; leftMargin: parent.width * 0.5 - splitText.typeText.length * 9 }
            Row {
                spacing: 10

                Text {
                    id: splitText
                    property string typeText: "";
                    property int splitType: p.splitting
                    onSplitTypeChanged: {
                        switch(splitType) {
                        case 1: typeText = "Splitting Autoloader "; break;
                        case 2: typeText = "Combining Autoloader "; break;
                        case 3: typeText = "Extracting Image "; break;
                        case 4: typeText = "Extracting Apps "; break;
                        case 5: typeText = "Fetching required files "; break;
                        default: typeText = "Waiting "; break;
                        }
                    }

                    text: typeText + ((p.splitting == 5) ? "" : "(" + p.splitProgress + "%)");
                    font.pointSize: 14
                }
                BusyIndicator {
                    width: parent.height
                    height: parent.height
                }
            }
            Text {
                visible: p.splitProgress > 100
                text: "Percentages are not entirely accurate."
                font.pointSize: 10
            }
        }

        Button {
            z: 6;
            visible: p.splitting == 2
            anchors {bottom: parent.bottom; bottomMargin: 10; horizontalCenter: parent.horizontalCenter}
            text: "Cancel";
            onClicked: p.abortSplit();
        }
    }

    ColumnLayout {
        height: parent.height / 2
        width: parent.width
        Layout.columnSpan: 4
        ColumnLayout {
            Text {
                text: "Autoloader Tools"
                font.pointSize: 14
                font.bold: true
            }
            RowLayout {
                Button {
                    text: "Split";
                    enabled: !p.splitting
                    onClicked: if (!p.splitting) p.splitAutoloader(osSelect.checked * 1 + radioSelect.checked * 2 + pinSelect.checked * 4);
                }
                CheckBox {
                    id: osSelect
                    text: "OS"
                    checked: true
                }
                CheckBox {
                    id: radioSelect
                    text: "Radio"
                    checked: true
                }
                CheckBox {
                    visible: p.advanced
                    id: pinSelect
                    text: "PINList"
                }
            }
            Label {
                text: "Split signed images from autoloaders"
                font.bold: true;
            }
            RowLayout {
                Text {
                    text: "Combine:"
                    font.pointSize: 12
                    font.bold: true;
                }
                Button {
                    text: "Folder";
                    enabled: !p.splitting
                    onClicked: if (!p.splitting) p.combineFolder();
                }
                Button {
                    text: ".signed(s)";
                    enabled: !p.splitting
                    onClicked: if (!p.splitting) p.combineFiles();
                }
            }
            Label {
                text: "Combine signed images in to an autoloader"
                font.bold: true;
            }
        }
        ColumnLayout {
            height: parent.height / 2
            width: parent.width
            Layout.columnSpan: 4
            Text {
                text: "Signed Image Tools"
                font.pointSize: 14
                font.bold: true
            }
            ColumnLayout {
                visible: p.advanced
                RowLayout {
                    property int partValue: corePart.checked * 1 + userPart.checked * 2
                    Button {
                        text: "Dump Contents"
                        enabled: !p.splitting && parent.partValue
                        onClicked: if (!p.splitting) p.extractImage(0, parent.partValue);
                    }
                    CheckBox {
                        id: corePart
                        checked: true
                        text: "Core"
                    }
                    CheckBox {
                        id: userPart
                        checked: true
                        text: "User"
                    }
                }
                Label {
                    text: "Dump file contents"
                    font.bold: true;
                }
            }
            ColumnLayout {
                visible: p.advanced
                RowLayout {
                    property int imageValue: rcfsImage.checked * 1 + qnxImage.checked * 2
                    Button {
                        text: "Extract Image"
                        enabled: !p.splitting && parent.imageValue
                        onClicked: if (!p.splitting) p.extractImage(1, parent.imageValue);
                    }
                    CheckBox {
                        id: rcfsImage
                        checked: true
                        text: "RCFS"
                    }
                    CheckBox {
                        id: qnxImage
                        text: "QNX6"
                    }
                }
                Label {
                    text: "Extracts filesystem image"
                    font.bold: true;
                }
            }
            ColumnLayout {
                Button {
                    text: "Extract Apps"
                    enabled: !p.splitting
                    onClicked: if (!p.splitting) p.extractImage(2, 2);
                }
                Label {
                    text: "Extract all bar archives"
                    font.bold: true;
                }
            }
        }
    }
}
