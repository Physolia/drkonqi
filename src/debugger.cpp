/*
    Copyright (C) 2009  George Kiagiadakis <gkiagia@users.sourceforge.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "debugger.h"

#include <KConfig>
#include <KConfigGroup>
#include <KMacroExpander>
#include <QCoreApplication>
#include <QDir>

#include "crashedapplication.h"
#include "drkonqi.h"
#include "drkonqi_debug.h"

//static
QList<Debugger> Debugger::availableInternalDebuggers(const QString & backend)
{
    return availableDebuggers(QStringLiteral("debuggers/internal"), backend);
}

//static
QList<Debugger> Debugger::availableExternalDebuggers(const QString & backend)
{
    return availableDebuggers(QStringLiteral("debuggers/external"), backend);
}

bool Debugger::isValid() const
{
    return m_config;
}

bool Debugger::isInstalled() const
{
    QString tryexec = tryExec();
    if(tryexec.isEmpty()) {
        qCDebug(DRKONQI_LOG) << "tryExec of" << name() << "is empty!";
        return false;
    }

    // Find for executable in PATH and in our application path
    return !QStandardPaths::findExecutable(tryexec).isEmpty()
        || !QStandardPaths::findExecutable(tryexec, {QCoreApplication::applicationDirPath()}).isEmpty();
}

QString Debugger::name() const
{
    return isValid() ? m_config->group("General").readEntry("Name") : QString();
}

QString Debugger::codeName() const
{
    //fall back to the "TryExec" string if "CodeName" is not specified.
    //for most debuggers those strings should be the same
    return isValid() ? m_config->group("General").readEntry("CodeName", tryExec()) : QString();
}

QString Debugger::tryExec() const
{
    return isValid() ? m_config->group("General").readEntry("TryExec") : QString();
}

QStringList Debugger::supportedBackends() const
{
    return isValid() ? m_config->group("General").readEntry("Backends")
                                   .split(QLatin1Char('|'), QString::SkipEmptyParts) : QStringList();
}

void Debugger::setUsedBackend(const QString & backendName)
{
    if (supportedBackends().contains(backendName)) {
        m_backend = backendName;
    }
}

QString Debugger::command() const
{
    return (isValid() && m_config->hasGroup(m_backend))
        ? m_config->group(m_backend).readPathEntry("Exec", QString())
        : QString();
}

QString Debugger::backtraceBatchCommands() const
{
    return (isValid() && m_config->hasGroup(m_backend))
        ? m_config->group(m_backend).readPathEntry("BatchCommands", QString())
        : QString();
}

bool Debugger::runInTerminal() const
{
    return (isValid() && m_config->hasGroup(m_backend))
        ? m_config->group(m_backend).readEntry("Terminal", false)
        : false;
}

QString Debugger::backendValueOfParameter(const QString &key) const
{
    return (isValid() && m_config->hasGroup(m_backend))
        ? m_config->group(m_backend).readEntry(key, QString())
        : QString();
}

//static
void Debugger::expandString(QString & str, ExpandStringUsage usage, const QString & tempFile)
{
    const CrashedApplication *appInfo = DrKonqi::crashedApplication();
    const QHash<QString, QString> map = {
        { QLatin1String("progname"), appInfo->name() },
        { QLatin1String("execname"), appInfo->fakeExecutableBaseName() },
        { QLatin1String("execpath"), appInfo->executable().absoluteFilePath() },
        { QLatin1String("signum"), QString::number(appInfo->signalNumber()) },
        { QLatin1String("signame"), appInfo->signalName() },
        { QLatin1String("pid"), QString::number(appInfo->pid()) },
        { QLatin1String("tempfile"), tempFile },
        { QLatin1String("thread"), QString::number(appInfo->thread()) },
    };

    if (usage == ExpansionUsageShell) {
        str = KMacroExpander::expandMacrosShellQuote(str, map);
    } else {
        str = KMacroExpander::expandMacros(str, map);
    }
}

//static
QList<Debugger> Debugger::availableDebuggers(const QString & path, const QString & backend)
{
    QStringList debuggerDirs {
        // Search in default path
        QStandardPaths::locate(QStandardPaths::AppDataLocation, path, QStandardPaths::LocateDirectory),
        // Search from application path, this helps when deploying an application
        QString(QStringLiteral("%1/%2")).arg(QCoreApplication::applicationDirPath(), path)
    };
    QList<Debugger> result;

    for (const auto & debuggerDir: debuggerDirs) {
        QStringList debuggers = QDir(debuggerDir).entryList(QDir::Files);
        for (const auto & debuggerFile : debuggers) {
            Debugger debugger;
            debugger.m_config = KSharedConfig::openConfig(QString(QStringLiteral("%1/%2")).arg(debuggerDir, debuggerFile));
            if (debugger.supportedBackends().contains(backend)) {
                debugger.setUsedBackend(backend);
                result.append(debugger);
            }
        }
    }
    return result;
}
