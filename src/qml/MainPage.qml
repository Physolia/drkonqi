// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2021-2022 Harald Sitter <sitter@kde.org>

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.19 as Kirigami

import org.kde.drkonqi 1.0

Kirigami.Page {
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    title: CrashedApplication.name

    Component.onDestruction: Settings.save()

    ColumnLayout {
        anchors.fill: parent

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: xi18nc("@info", "<para>We are sorry, <application>%1</application> closed unexpectedly.</para>", CrashedApplication.name)
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            onLinkActivated: link => Qt.openUrlExternally(link)
            text: canReportText
        }

        Item {
            Layout.fillHeight: true
        }

        GridLayout {
            Layout.alignment: Qt.AlignHCenter

            columns: Kirigami.Settings.isMobile ? 1 : 2
            rows: Kirigami.Settings.isMobile ? 2 : 1

            MainPageButton {
                Layout.alignment: Qt.AlignHCenter

                action: Kirigami.Action {
                    enabled: Kirigami.Settings.isMobile ? true : canAutoReport
                    visible: Kirigami.Settings.isMobile ? canAutoReport : true
                    icon.name: "document-send-symbolic"
                    text: i18nc("@action", "Send Automatic Report")
                    onTriggered: {
                        reportInterface.setSendWhenReady(true)
                        reportInterface.sendSentryReport()
                        pageStack.replace("qrc:/ui/SentryPage.qml")
                    }
                }
            }

            MainPageButton {
                Layout.alignment: Qt.AlignHCenter

                action: Kirigami.Action {
                    icon.name: "code-class-symbolic"
                    text: i18nc("@action", "See Developer Information")
                    onTriggered: pageStack.push("qrc:/ui/DeveloperPage.qml")
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
