import QtQuick

// Velan — voice assistant visualisation.
// Three concentric circles that animate based on voice assistant state.
// voiceState maps to VoiceAssistantState proto enum:
//   0=IDLE  1=LISTENING  2=RECORDING  3=THINKING  4=TALKING
//
// innerColor is exposed so callers (e.g. the screen border) can match it
// without duplicating the literal.
Item {
    id: root

    width:  180
    height: 180

    property int voiceState: vehicle.voiceAssistState

    // Innermost circle colour — also used for the screen border in ClusterRoot.
    readonly property color innerColor: "#d946ef"

    // ── Outer ring ────────────────────────────────────────────────────────────
    Rectangle {
        id: outerCircle
        anchors.centerIn: parent
        width:  180; height: 180
        radius: width / 2
        color:  "#3b0764"

        SequentialAnimation on scale {
            running: root.voiceState !== 0
            loops:   Animation.Infinite
            NumberAnimation { to: 1.06; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0.94; duration: 900; easing.type: Easing.InOutSine }
        }
    }

    // ── Middle ring ───────────────────────────────────────────────────────────
    Rectangle {
        anchors.centerIn: parent
        width:  116; height: 116
        radius: width / 2
        color:  "#7e22ce"
    }

    // ── Innermost circle (core) ───────────────────────────────────────────────
    Rectangle {
        id: innerCircle
        anchors.centerIn: parent
        width:  60; height: 60
        radius: width / 2
        color:  root.innerColor

        SequentialAnimation on scale {
            running: root.voiceState !== 0
            loops:   Animation.Infinite
            NumberAnimation { to: 1.25; duration: 500; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.00; duration: 500; easing.type: Easing.InOutSine }
        }
    }

    // ── State label ───────────────────────────────────────────────────────────
    Text {
        anchors.centerIn: parent
        z: 1
        text: (["", "LISTEN", "REC", "THINK", "TALK"])[root.voiceState] || ""
        font.pixelSize: 10
        font.bold:      true
        color:          "white"
    }

    // ── Fade in / out ─────────────────────────────────────────────────────────
    opacity: voiceState !== 0 ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }
}
