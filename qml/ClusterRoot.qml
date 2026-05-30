import QtQuick
import QtQuick.Controls

Window {
    id: root
    width:  1920
    height: 720
    visible: true
    title: "Instrument Cluster"
    color: "#0d0d0d"

    // ── Drive mode state machine ──────────────────────────────────────────────
    // vehicle.driveMode: 0=normal  1=sport  2=reverse
    // QML states change layout, colours, and which widgets are visible.

    // Use a root container to handle states and transitions
    Rectangle {
        id: mainContainer
        anchors.fill: parent
        color: "transparent"

        state: {
            switch (vehicle.driveMode) {
                case 1:  return "sport"
                case 2:  return "reverse"
                default: return "normal"
            }
        }

        states: [
            State {
                name: "normal"
                PropertyChanges { target: speedometer; x: 80;  scale: 1.2 }
                PropertyChanges { target: tachometer; scale: 1.2; visible: true }
                PropertyChanges { target: pipOverlay;  visible: false }
                PropertyChanges { target: modeLabel;   text: "NORMAL"; color: "#aaaaaa" }
            },
            State {
                name: "sport"
                PropertyChanges { target: speedometer; x: 120; scale: 1.4 }
                PropertyChanges { target: tachometer;
                                  x: root.width - tachometer.width -80 -120; 
                                  scale: 1.4; visible: true }
                PropertyChanges { target: pipOverlay;  visible: false }
                PropertyChanges { target: modeLabel;   text: "SPORT";  color: "#e24b4a" }
            },
            State {
                name: "reverse"
                PropertyChanges { target: speedometer; x: 80;  scale: 1.0 }
                PropertyChanges { target: tachometer;  visible: false }
                PropertyChanges { target: pipOverlay;  visible: true  }
                PropertyChanges { target: modeLabel;   text: "REVERSE"; color: "#f0c040" }
            }
        ]

        transitions: [
            Transition {
                NumberAnimation {
                    properties: "x, scale, opacity"
                    duration:   250
                    easing.type: Easing.OutCubic
                }
                ColorAnimation { duration: 250 }
            }
        ]

        // ── Layout ────────────────────────────────────────────────────────────────

        Speedometer {
            id: speedometer
            x:  80
            y:  (root.height - height) / 2
        }

        Tachometer {
            id: tachometer
            x:  root.width - width - 80
            y:  (root.height - height) / 2
        }

        // Centre info block
        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                id: modeLabel
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 18
                font.letterSpacing: 4
                color: "#aaaaaa"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "GEAR  " + {
                    0x0001: "N",
                    0x0002: "R",
                    0x0004: "P",
                    0x0008: "D",
                    0x0010: "1",
                    0x0020: "2",
                    0x0040: "3",
                    0x0080: "4",
                    0x0100: "5",
                    0x0200: "6",
                    0x0400: "7",
                    0x0800: "8",
                    0x1000: "9"
                }[vehicle.gear] || "?"
                font.pixelSize: 32
                font.bold: true
                color: "#ffffff"
            }

            // Fuel bar
            Rectangle {
                width:  240
                height: 8
                radius: 4
                color:  "#333333"
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    width:  Math.min(parent.width, parent.width * (vehicle.fuel / 100.0))
                    height: parent.height
                    radius: parent.radius
                    color:  vehicle.fuel < 15 ? "#e24b4a" : "#1d9e75"

                    Behavior on width {
                        NumberAnimation { duration: 300 }
                    }
                }
            }

            // Warning icons row
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                Rectangle {
                    width: 48; height: 48; radius: 24
                    color:   vehicle.warnEngine  ? "#e24b4a" : "#1a1a1a"
                    border.color: "#444"; border.width: 1
                    Text { anchors.centerIn: parent; text: "E"; color: "white"; font.pixelSize: 16 }
                }
                Rectangle {
                    width: 48; height: 48; radius: 24
                    color:   vehicle.warnBatt    ? "#f0c040" : "#1a1a1a"
                    border.color: "#444"; border.width: 1
                    Text { anchors.centerIn: parent; text: "B"; color: "white"; font.pixelSize: 16 }
                }
                Rectangle {
                    width: 48; height: 48; radius: 24
                    color:   vehicle.warnBrake   ? "#e24b4a" : "#1a1a1a"
                    border.color: "#444"; border.width: 1
                    Text { anchors.centerIn: parent; text: "!"; color: "white"; font.pixelSize: 16 }
                }
            }
        }

        PipOverlay {
            id: pipOverlay
            z: tachometer.z + 1
            anchors {
                right:  parent.right
                //bottom: parent.bottom
                verticalCenter: tachometer.verticalCenter
                margins: 50
            }
            visible: true
        }

    }

    // ── Screen border — active during RECORDING / THINKING / TALKING ─────────
    // Colors match Velan SiriOrb stateCoreColor: 2=orange, 3=violet, 4=cyan
    Rectangle {
        anchors.fill: parent
        color:        "transparent"
        border.color: ({2: "#FF6D00", 3: "#7C4DFF", 4: "#00E5FF"})[vehicle.voiceAssistState] || "#FF6D00"
        border.width: 4
        visible:      vehicle.voiceAssistState >= 2
        z:            mainContainer.z + 1
    }
}
