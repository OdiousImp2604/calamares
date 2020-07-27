/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   SPDX-FileCopyrightText: 2014-2016 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *   License-Filename: LICENSE
 */

#include "CreateUserJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/CalamaresUtilsSystem.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>


CreateUserJob::CreateUserJob( const QString& userName,
                              const QString& fullName,
                              bool autologin,
                              const QStringList& defaultGroups )
    : Calamares::Job()
    , m_userName( userName )
    , m_fullName( fullName )
    , m_autologin( autologin )
    , m_defaultGroups( defaultGroups )
{
}


QString
CreateUserJob::prettyName() const
{
    return tr( "Create user %1" ).arg( m_userName );
}


QString
CreateUserJob::prettyDescription() const
{
    return tr( "Create user <strong>%1</strong>." ).arg( m_userName );
}


QString
CreateUserJob::prettyStatusMessage() const
{
    return tr( "Creating user %1." ).arg( m_userName );
}

static QStringList
groupsInTargetSystem( const QDir& targetRoot )
{
    QFileInfo groupsFi( targetRoot.absoluteFilePath( "etc/group" ) );
    QFile groupsFile( groupsFi.absoluteFilePath() );
    if ( !groupsFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return QStringList();
    }
    QString groupsData = QString::fromLocal8Bit( groupsFile.readAll() );
    QStringList groupsLines = groupsData.split( '\n' );
    for ( QStringList::iterator it = groupsLines.begin(); it != groupsLines.end(); ++it )
    {
        int indexOfFirstToDrop = it->indexOf( ':' );
        it->truncate( indexOfFirstToDrop );
    }
    return groupsLines;
}

static void
ensureGroupsExistInTarget( const QStringList& wantedGroups, const QStringList& availableGroups )
{
    for ( const QString& group : wantedGroups )
    {
        if ( !availableGroups.contains( group ) )
        {
            CalamaresUtils::System::instance()->targetEnvCall( { "groupadd", group } );
        }
    }
}

Calamares::JobResult
CreateUserJob::exec()
{
    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    QDir destDir( gs->value( "rootMountPoint" ).toString() );

    if ( gs->contains( "sudoersGroup" ) && !gs->value( "sudoersGroup" ).toString().isEmpty() )
    {
        cDebug() << "[CREATEUSER]: preparing sudoers";

        QString sudoersLine = QString( "%%1 ALL=(ALL) ALL\n" ).arg( gs->value( "sudoersGroup" ).toString() );
        auto fileResult
            = CalamaresUtils::System::instance()->createTargetFile( QStringLiteral( "/etc/sudoers.d/10-installer" ),
                                                                    sudoersLine.toUtf8().constData(),
                                                                    CalamaresUtils::System::WriteMode::Overwrite );

        if ( fileResult )
        {
            if ( CalamaresUtils::Permissions::apply( fileResult.path(), 0440 ) )
            {
                return Calamares::JobResult::error( tr( "Cannot chmod sudoers file." ) );
            }
        }
        else
        {
            return Calamares::JobResult::error( tr( "Cannot create sudoers file for writing." ) );
        }
    }

    cDebug() << "[CREATEUSER]: preparing groups";

    QStringList availableGroups = groupsInTargetSystem( destDir );
    ensureGroupsExistInTarget( m_defaultGroups, availableGroups );

    QString defaultGroups = m_defaultGroups.join( ',' );
    if ( m_autologin )
    {
        if ( gs->contains( "autologinGroup" ) && !gs->value( "autologinGroup" ).toString().isEmpty() )
        {
            QString autologinGroup = gs->value( "autologinGroup" ).toString();
            ensureGroupsExistInTarget( QStringList { autologinGroup }, availableGroups );
            defaultGroups.append( QString( ",%1" ).arg( autologinGroup ) );
        }
    }

    // If we're looking to reuse the contents of an existing /home
    if ( gs->value( "reuseHome" ).toBool() )
    {
        QString shellFriendlyHome = "/home/" + m_userName;
        QDir existingHome( destDir.absolutePath() + shellFriendlyHome );
        if ( existingHome.exists() )
        {
            QString backupDirName = "dotfiles_backup_" + QDateTime::currentDateTime().toString( "yyyy-MM-dd_HH-mm-ss" );
            existingHome.mkdir( backupDirName );

            CalamaresUtils::System::instance()->targetEnvCall(
                { "sh", "-c", "mv -f " + shellFriendlyHome + "/.* " + shellFriendlyHome + "/" + backupDirName } );
        }
    }

    cDebug() << "[CREATEUSER]: creating user";

    QStringList useradd { "useradd", "-m", "-U" };
    QString shell = gs->value( "userShell" ).toString();
    if ( !shell.isEmpty() )
    {
        useradd << "-s" << shell;
    }
    useradd << "-c" << m_fullName;
    useradd << m_userName;

    auto commandResult = CalamaresUtils::System::instance()->targetEnvCommand( useradd );
    if ( commandResult.getExitCode() )
    {
        cError() << "useradd failed" << commandResult.getExitCode();
        return commandResult.explainProcess( useradd, std::chrono::seconds( 10 ) /* bogus timeout */ );
    }

    commandResult
        = CalamaresUtils::System::instance()->targetEnvCommand( { "usermod", "-aG", defaultGroups, m_userName } );
    if ( commandResult.getExitCode() )
    {
        cError() << "usermod failed" << commandResult.getExitCode();
        return commandResult.explainProcess( "usermod", std::chrono::seconds( 10 ) /* bogus timeout */ );
    }

    QString userGroup = QString( "%1:%2" ).arg( m_userName ).arg( m_userName );
    QString homeDir = QString( "/home/%1" ).arg( m_userName );
    commandResult = CalamaresUtils::System::instance()->targetEnvCommand( { "chown", "-R", userGroup, homeDir } );
    if ( commandResult.getExitCode() )
    {
        cError() << "chown failed" << commandResult.getExitCode();
        return commandResult.explainProcess( "chown", std::chrono::seconds( 10 ) /* bogus timeout */ );
    }

    return Calamares::JobResult::ok();
}
