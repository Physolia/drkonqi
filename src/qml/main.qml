// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2021-2022 Harald Sitter <sitter@kde.org>

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.19 as Kirigami

import org.kde.drkonqi 1.0

Kirigami.ApplicationWindow {
    id: appWindow

    title: CrashedApplication.name
    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 22
    height: minimumHeight

    property alias bugzilla: reportInterface.bugzilla

    ReportInterface {
        id: reportInterface
    }

    DuplicateModel {
        id: duplicateModel
        manager: bugzilla
        iface: reportInterface
    }

    pageStack.initialPage: MainPage {}
}
