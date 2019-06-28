/*******************************************************************
* systeminformation.cpp
* Copyright 2009    Dario Andres Rodriguez <andresbajotierra@gmail.com>
* Copyright 2009    George Kiagiadakis <gkiagia@users.sourceforge.net>
* Copyright 2019    Harald Sitter <sitter@kde.org>
*
* modify it under the terms of the GNU General Public License as
* This program is free software; you can redistribute it and/or
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************/

#include <config-drkonqi.h>

#include "systeminformation.h"

#if HAVE_UNAME
# include <errno.h>
# include <sys/utsname.h>
#endif

#include <QFile>

#include <KProcess>
#include "drkonqi_debug.h"
#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>
#include <kcoreaddons_version.h>
#include <KCoreAddons>
#include <KOSRelease>
#include <QStandardPaths>

static const QString OS_UNSPECIFIED = QStringLiteral("unspecified");
static const QString PLATFORM_UNSPECIFIED = QStringLiteral("unspecified");

SystemInformation::Config::Config()
    : lsbReleasePath(QStandardPaths::findExecutable(QLatin1String("lsb_release")))
    , osReleasePath(/* Use KOSRelease default */)
{
}

SystemInformation::SystemInformation(Config infoConfig, QObject *parent)
    : QObject(parent)
    , m_bugzillaOperatingSystem(OS_UNSPECIFIED)
    , m_bugzillaPlatform(PLATFORM_UNSPECIFIED)
    , m_complete(false)
    , m_infoConfig(infoConfig)
{
    // NOTE: the relative order is important here
    m_bugzillaOperatingSystem = fetchOSBasicInformation();
    m_operatingSystem = fetchOSDetailInformation();

    tryToSetBugzillaPlatform();

    KConfigGroup config(KSharedConfig::openConfig(), "SystemInformation");
    m_compiledSources = config.readEntry("CompiledSources", false);
}

SystemInformation::~SystemInformation()
{
    KConfigGroup config(KSharedConfig::openConfig(), "SystemInformation");
    config.writeEntry("CompiledSources", m_compiledSources);
    config.sync();
}

void SystemInformation::tryToSetBugzillaPlatform()
{
    QString platform = PLATFORM_UNSPECIFIED;
    // first, try to guess bugzilla platfrom from the internal OS information
    // this should work for BSDs, solaris and windows.
    platform = guessBugzillaPlatform(m_bugzillaOperatingSystem);

    // if the internal information is not enough, refer to external information
    if (platform == PLATFORM_UNSPECIFIED) {
        tryToSetBugzillaPlatformFromExternalInfo();
    } else {
        setBugzillaPlatform(platform);
    }
}

void SystemInformation::tryToSetBugzillaPlatformFromExternalInfo()
{
    //Run lsb_release async
    QString lsb_release = m_infoConfig.lsbReleasePath;
    if (!lsb_release.isEmpty()) {
        qCDebug(DRKONQI_LOG) << "found lsb_release";
        KProcess *process = new KProcess();
        process->setOutputChannelMode(KProcess::OnlyStdoutChannel);
        process->setEnv(QStringLiteral("LC_ALL"), QStringLiteral("C"));
        *process << lsb_release << QStringLiteral("-sd");
        connect(process, static_cast<void (KProcess::*)(int, QProcess::ExitStatus)>(&KProcess::finished), this, &SystemInformation::lsbReleaseFinished);
        process->start();
    } else {
        // when lsb_release is unavailable, turn to /etc/os-release
        const QString& osReleaseInfo = fetchOSReleaseInformation();
        const QString& platform = guessBugzillaPlatform(osReleaseInfo);
        setBugzillaPlatform(platform);
        m_complete = true;
    }
}

void SystemInformation::lsbReleaseFinished()
{
    KProcess *process = qobject_cast<KProcess*>(sender());
    Q_ASSERT(process);
    m_distributionPrettyName = QString::fromLocal8Bit(process->readAllStandardOutput().trimmed());
    process->deleteLater();

    //Guess distro string
    QString platform = guessBugzillaPlatform(m_distributionPrettyName);

    // if lsb_release doesn't work well, turn to the /etc/os-release file
    if (platform == PLATFORM_UNSPECIFIED) {
        const QString& osReleaseInfo = fetchOSReleaseInformation();
        platform = guessBugzillaPlatform(osReleaseInfo);
    }

    setBugzillaPlatform(platform);
    m_complete = true;
}

//this function maps the distribution information to an "Hardware Platform"    .
//value that is accepted by bugs.kde.org.  If the values change on the server    .
//side, they need to be updated here as well                                   .
QString SystemInformation::guessBugzillaPlatform(const QString& distroInfo) const
{
    static QHash<QString, QString> platforms {
        { QStringLiteral("suse"), QStringLiteral("openSUSE RPMs") },
        { QStringLiteral("mint"), QStringLiteral("Mint (Ubuntu Based)") },
        { QStringLiteral("lmde"), QStringLiteral("Mint (Debian Based)") },
        { QStringLiteral("ubuntu"), QStringLiteral("Ubuntu Packages") },
        { QStringLiteral("fedora"), QStringLiteral("Fedora RPMs") },
        { QStringLiteral("redhat"), QStringLiteral("RedHat RPMs") },
        { QStringLiteral("gentoo"), QStringLiteral("Gentoo Packages") },
        { QStringLiteral("mandriva"), QStringLiteral("Mandriva RPMs") },
        { QStringLiteral("mageia"), QStringLiteral("Mageia RPMs") },
        { QStringLiteral("slack"), QStringLiteral("Slackware Packages") },
        { QStringLiteral("pclinuxos"), QStringLiteral("PCLinuxOS") },
        { QStringLiteral("pardus"), QStringLiteral("Pardus Packages") },
        { QStringLiteral("freebsd"), QStringLiteral("FreeBSD Ports") },
        { QStringLiteral("netbsd"), QStringLiteral("NetBSD pkgsrc") },
        { QStringLiteral("openbsd"), QStringLiteral("OpenBSD Packages") },
        { QStringLiteral("solaris"), QStringLiteral("Solaris Packages") },
        { QStringLiteral("chakra"), QStringLiteral("Chakra") },
        { QStringLiteral("ms windows"), QStringLiteral("MS Windows") },
        { QStringLiteral("arch"), QStringLiteral("Archlinux Packages") }
    };
    for (auto it = platforms.constBegin(); it != platforms.constEnd(); ++it) {
        if (distroInfo.contains(it.key(), Qt::CaseInsensitive)) {
            return it.value();
        }
    }

    // Debian has multiple platforms.
    if (distroInfo.contains(QStringLiteral("debian"), Qt::CaseInsensitive)) {
        if (distroInfo.contains(QStringLiteral("unstable"), Qt::CaseInsensitive)) {
            return QStringLiteral("Debian unstable");
        } else if (distroInfo.contains(QStringLiteral("testing"), Qt::CaseInsensitive)) {
            return QStringLiteral("Debian testing");
        } else {
            return QStringLiteral("Debian stable");
        }
    }

    return PLATFORM_UNSPECIFIED;
}

//this function maps the operating system to an OS value that is accepted by bugs.kde.org.
//if the values change on the server side, they need to be updated here as well.
QString SystemInformation::fetchOSBasicInformation() const
{
    //krazy:excludeall=cpp
    //Get the base OS string (bugzillaOS)
#if defined(Q_OS_LINUX)
    return QLatin1String("Linux");
#elif defined(Q_OS_FREEBSD)
    return QLatin1String("FreeBSD");
#elif defined(Q_OS_NETBSD)
    return QLatin1String("NetBSD");
#elif defined(Q_OS_OPENBSD)
    return QLatin1String("OpenBSD");
#elif defined(Q_OS_AIX)
    return QLatin1String("AIX");
#elif defined(Q_OS_HPUX)
    return QLatin1String("HP-UX");
#elif defined(Q_OS_IRIX)
    return QLatin1String("IRIX");
#elif defined(Q_OS_OSF)
    return QLatin1String("Tru64");
#elif defined(Q_OS_SOLARIS)
    return QLatin1String("Solaris");
#elif defined(Q_OS_CYGWIN)
    return QLatin1String("Cygwin");
#elif defined(Q_OS_DARWIN)
    return QLatin1String("OS X");
#elif defined(Q_OS_WIN32)
    return QLatin1String("MS Windows");
#else
    return OS_UNSPECIFIED;
#endif

}

QString SystemInformation::fetchOSDetailInformation() const
{
    //Get complete OS string (and fallback to base string)
    QString operatingSystem = m_bugzillaOperatingSystem;

#if HAVE_UNAME
    struct utsname buf;

    auto unameFunc = &uname;
    if (m_infoConfig.unameFunc) {
        unameFunc = (int (*)(utsname*))m_infoConfig.unameFunc;
    }

    if ((*unameFunc)(&buf) == -1) {
        qCDebug(DRKONQI_LOG) << "call to uname failed" << errno;
    } else {
        operatingSystem = QString::fromLocal8Bit(buf.sysname) + QLatin1Char(' ')
            + QString::fromLocal8Bit(buf.release) + QLatin1Char(' ')
            + QString::fromLocal8Bit(buf.machine);
    }
#endif

    return operatingSystem;
}

QString SystemInformation::fetchOSReleaseInformation()
{
    KOSRelease os(m_infoConfig.osReleasePath);
    return m_distributionPrettyName = os.prettyName();
}

QString SystemInformation::operatingSystem() const
{
    return m_operatingSystem;
}

QString SystemInformation::bugzillaOperatingSystem() const
{
    return m_bugzillaOperatingSystem;
}

QString SystemInformation::bugzillaPlatform() const
{
    return m_bugzillaPlatform;
}

void SystemInformation::setBugzillaPlatform(const QString & platform)
{
    m_bugzillaPlatform = platform;
}

QString SystemInformation::distributionPrettyName() const
{
    return m_distributionPrettyName;
}

bool SystemInformation::compiledSources() const
{
    return m_compiledSources;
}

void SystemInformation::setCompiledSources(bool compiled)
{
    m_compiledSources = compiled;
}

QString SystemInformation::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString SystemInformation::frameworksVersion() const
{
    return KCoreAddons::versionString();
}

bool SystemInformation::complete() const
{
    return m_complete;
}
