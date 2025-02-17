// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2021 Harald Sitter <sitter@kde.org>

import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.19 as Kirigami

import org.kde.drkonqi 1.0

DeveloperPage {
    title: i18nc("@title", "Fetching the Backtrace (Automatic Crash Information)")
    reportActionVisible: false
    basic: !advancedAction.checked

    onTraceChanged: {
        reportInterface.backtrace = trace
        reportInterface.setFirstBacktraceFunctions(BacktraceGenerator.parser().firstValidFunctions())
    }

    footerActionsLeft: [
        Kirigami.Action {
            id: advancedAction
            checkable: true
            icon.name: "code-context"
            text: i18nc("@action:button", "Show backtrace content (advanced)")
        }
    ]
    footerActionsRight: [
        Kirigami.Action {
            enabled: {
                if (DrKonqi.ignoreQuality() && state == BacktraceGenerator.Loaded) {
                    return true;
                }
                switch (usefulness) {
                case BacktraceParser.ReallyUseful:
                case BacktraceParser.MayBeUseful:
                // TODO figure out how to best decide whether to move on or not
                case BacktraceParser.ProbablyUseless:
                    return true
                case BacktraceParser.Useless:
                case BacktraceParser.InvalidUsefulness:
                    return false
                }
            }
            icon.name: "go-next"
            text: i18nc("@action:button", "Next")
            onTriggered: pageStack.push("qrc:/ui/BugzillaPage.qml")
        }
    ]
}
