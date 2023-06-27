// SPDX-FileCopyrightText: 2023 Jonas Kalinka <jonas.kalinka@basyskom.com>
// SPDX-FileCopyrightText: 2023 basysKom GmbH

// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import RiveQtQuickPlugin 1.0

Item {
    id: root

    property alias fileSource: riveItem.fileSource

    RowLayout {
        anchors.fill: parent

        ControlPanel {
            title: "Artboards"
            model: riveItem.artboards
            visible: model !== undefined ? model.length > 0 : false
            onClicked: modelData => {
                           riveItem.currentArtboardIndex = modelData.id
                       }

            Layout.fillHeight: true
            Layout.minimumWidth: 200
            Layout.minimumHeight: 200
        }

        ControlPanel {
            title: "Animations"

            model: riveItem.animations
            visible: model !== undefined ? model.length > 0 : false
            onClicked: modelData => {
                           riveItem.triggerAnimation(modelData.id)
                       }

            Layout.fillHeight: true
            Layout.minimumWidth: 200
            Layout.minimumHeight: 200
        }

        ControlPanel {
            title: "State Machines"

            model: riveItem.stateMachines
            visible: model !== undefined ? model.length > 0 : false
            onClicked: modelData => {
                           riveItem.currentStateMachineIndex = modelData.id
                       }

            Layout.fillHeight: true
            Layout.minimumWidth: 200
            Layout.minimumHeight: 200
        }

        ControlPanel {
            title: "Triggers"

            model: riveItem.stateMachineInterface ? riveItem.stateMachineInterface.triggers : 0
            visible: model !== undefined ? model.length > 0 : false
            onClicked: modelData => {
                           riveItem.stateMachineInterface.activateTrigger(modelData)
                       }

            Layout.fillHeight: true
            Layout.minimumWidth: 200
            Layout.minimumHeight: 200
        }

        Rectangle {
            color: "black"
            Layout.fillHeight: true
            Layout.fillWidth: true

            Rectangle {
                anchors.fill: parent
                anchors.margins: 30
                color: "grey"
                border.color: "red"

                RiveQtQuickItem {
                    id: riveItem

                    anchors.fill: parent
                    anchors.margins: 1

                    currentArtboardIndex: 0
                    currentStateMachineIndex: 0

                    renderQuality: RiveQtQuickItem.Medium
                    fillMode: RiveQtQuickItem.Stretch
                }
            }
        }
    }
}
