/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   Copyright 2019, Adriaan de Groot <groot@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "OEMViewStep.h"

#include "utils/Variant.h"

#include <QDate>
#include <QLabel>

OEMViewStep::OEMViewStep(QObject* parent)
    : Calamares::ViewStep( parent )
    , m_widget( nullptr )
    , m_visited( false )
{
}

OEMViewStep::~OEMViewStep()
{
}

bool OEMViewStep::isBackEnabled() const
{
    return true;
}

bool OEMViewStep::isNextEnabled() const
{
    return true;
}

bool OEMViewStep::isAtBeginning() const
{
    return true;
}

bool OEMViewStep::isAtEnd() const
{
    return true;
}

static QString substitute( QString s )
{
    QString t_date = QStringLiteral( "@@DATE@@" );
    if ( s.contains( t_date ) )
    {
        auto date = QDate::currentDate();
        s = s.replace( t_date, date.toString( Qt::ISODate ));
    }

    return s;
}

void OEMViewStep::onActivate()
{
    if ( !m_visited && m_user_batchIdentifier.isEmpty() )
    {
        m_user_batchIdentifier = substitute( m_conf_batchIdentifier );
        // m_widget->setIdentifier( m_user_batchIdentifier );
    }
    m_visited = true;
}

void OEMViewStep::onLeave()
{
    // m_user_batchIdentifier = m_widget->identifier();
}

QString OEMViewStep::prettyName() const
{
    return tr( "OEM Configuration" );
}

QWidget * OEMViewStep::widget()
{
    return nullptr; // m_widget;
}

Calamares::JobList OEMViewStep::jobs() const
{
    return Calamares::JobList();
}

void OEMViewStep::setConfigurationMap(const QVariantMap& configurationMap)
{
    m_conf_batchIdentifier = CalamaresUtils::getString( configurationMap, "batch-identifier" );
}

CALAMARES_PLUGIN_FACTORY_DEFINITION( OEMViewStepFactory, registerPlugin<OEMViewStep>(); )
