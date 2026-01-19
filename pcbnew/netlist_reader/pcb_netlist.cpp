/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2011 Jean-Pierre Charras.
 * Copyright (C) 2013 Wayne Stambaugh <stambaughw@gmail.com>.
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#include "pcb_netlist.h"

#include <footprint.h>
#include <richio.h>
#include <string_utils.h>
#include <wx/tokenzr.h>
#include <wx/log.h>


void COMPONENT::SetFootprint( FOOTPRINT* aFootprint )
{
    m_footprint.reset( aFootprint );
    KIID_PATH path = m_path;

    if( !m_kiids.empty() )
        path.push_back( m_kiids.front() );

    if( aFootprint == nullptr )
        return;

    aFootprint->SetReference( m_reference );
    aFootprint->SetValue( m_value );
    aFootprint->SetFPID( m_fpid );
    aFootprint->SetPath( path );

    // Copy over unit info
    std::vector<FOOTPRINT::FP_UNIT_INFO> fpUnits;

    for( const UNIT_INFO& u : m_units )
        fpUnits.push_back( { u.m_unitName, u.m_pins } );

    aFootprint->SetUnitInfo( fpUnits );
}


const COMPONENT_VARIANT* COMPONENT::GetVariant( const wxString& aVariantName ) const
{
    auto it = m_variants.find( aVariantName );

    return it != m_variants.end() ? &it->second : nullptr;
}


COMPONENT_VARIANT* COMPONENT::GetVariant( const wxString& aVariantName )
{
    auto it = m_variants.find( aVariantName );

    return it != m_variants.end() ? &it->second : nullptr;
}


void COMPONENT::AddVariant( const COMPONENT_VARIANT& aVariant )
{
    if( aVariant.m_name.IsEmpty() )
        return;

    auto it = m_variants.find( aVariant.m_name );

    if( it != m_variants.end() )
    {
        COMPONENT_VARIANT updated = aVariant;
        updated.m_name = it->first;
        it->second = std::move( updated );
        return;
    }

    m_variants.emplace( aVariant.m_name, aVariant );
}
