/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2016 Wayne Stambaugh, stambaughw@gmail.com
 * Copyright (C) 2016-2017 KiCad Developers, see change_log.txt for contributors.
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

/**
 * @file sch_validators.cpp
 * @brief Implementation of control validators for schematic dialogs.
 */

#include <wx/combo.h>
#include <wx/msgdlg.h>

#include <sch_connection.h>
#include <sch_validators.h>
#include <project/net_settings.h>
#include <template_fieldnames.h>


SCH_FIELD_VALIDATOR::SCH_FIELD_VALIDATOR(  bool aIsLibEditor, int aFieldId, wxString* aValue ) :
    wxTextValidator( wxFILTER_EXCLUDE_CHAR_LIST, aValue )
{
    m_fieldId = aFieldId;
    m_isLibEditor = aIsLibEditor;

    // Fields cannot contain carriage returns, line feeds, or tabs.
    wxString excludes( "\r\n\t" );

    // The reference field cannot contain spaces.
    if( aFieldId == REFERENCE )
    {
        excludes += " ";
    }
    else if( aFieldId == VALUE && m_isLibEditor )
    {
        excludes += " :/\\";
    }
    else if( aFieldId == SHEETFILENAME_V )
    {
        excludes += ":/\\";
    }

    long style = GetStyle();

    // The reference, value sheetname and sheetfilename fields cannot be empty.
    if( aFieldId == REFERENCE
            || aFieldId == VALUE
            || aFieldId == SHEETNAME_V
            || aFieldId == SHEETFILENAME_V
            || aFieldId == FIELD_NAME )
    {
        style |= wxFILTER_EMPTY;
    }

    SetStyle( style );
    SetCharExcludes( excludes );
}


SCH_FIELD_VALIDATOR::SCH_FIELD_VALIDATOR( const SCH_FIELD_VALIDATOR& aValidator ) :
    wxTextValidator( aValidator )
{
    m_fieldId = aValidator.m_fieldId;
    m_isLibEditor = aValidator.m_isLibEditor;
}


bool SCH_FIELD_VALIDATOR::Validate( wxWindow *aParent )
{
    // If window is disabled, simply return
    if( !m_validatorWindow->IsEnabled() || !m_validatorWindow->IsShown() )
        return true;

    wxTextEntry * const text = GetTextEntry();

    if( !text )
        return false;

    wxString val( text->GetValue() );

    // The format of the error message for not allowed chars
    wxString fieldCharError;

    switch( m_fieldId )
    {
    case REFERENCE:
        fieldCharError = _( "The reference designator cannot contain %s character(s)." );
        break;

    case VALUE:
        fieldCharError = _( "The value field cannot contain %s character(s)." );
        break;

    case FOOTPRINT:
        fieldCharError = _( "The footprint field cannot contain %s character(s)." );
        break;

    case DATASHEET:
        fieldCharError = _( "The datasheet field cannot contain %s character(s)." );
        break;

    case SHEETNAME_V:
        fieldCharError = _( "The sheet name cannot contain %s character(s)." );
        break;

    case SHEETFILENAME_V:
        fieldCharError = _( "The sheet filename cannot contain %s character(s)." );
        break;

    default:
        fieldCharError = _( "The field cannot contain %s character(s)." );
        break;
    };

    wxString msg;

    // We can only do some kinds of validation once the input is complete, so
    // check for them here:
    if( HasFlag( wxFILTER_EMPTY ) && val.empty() )
    {
        // Some fields cannot have an empty value, and user fields require a name:
        if( m_fieldId == FIELD_NAME )
            msg.Printf( _( "The name of the field cannot be empty." ) );
        else    // the FIELD_VALUE id or REFERENCE or VALUE
            msg.Printf( _( "The value of the field cannot be empty." ) );
    }
    else if( HasFlag( wxFILTER_EXCLUDE_CHAR_LIST ) && ContainsExcludedCharacters( val ) )
    {
        wxArrayString whiteSpace;
        bool spaceIllegal = m_fieldId == REFERENCE
                                || ( m_fieldId == VALUE && m_isLibEditor )
                                || m_fieldId == SHEETNAME_V
                                || m_fieldId == SHEETFILENAME_V;

        if( val.Find( '\r' ) != wxNOT_FOUND )
            whiteSpace.Add( _( "carriage return" ) );
        if( val.Find( '\n' ) != wxNOT_FOUND )
            whiteSpace.Add( _( "line feed" ) );
        if( val.Find( '\t' ) != wxNOT_FOUND )
            whiteSpace.Add( _( "tab" ) );
        if( spaceIllegal && (val.Find( ' ' ) != wxNOT_FOUND) )
            whiteSpace.Add( _( "space" ) );

        wxString badChars;

        if( whiteSpace.size() == 1 )
            badChars = whiteSpace[0];
        else if( whiteSpace.size() == 2 )
            badChars.Printf( _( "%s or %s" ), whiteSpace[0], whiteSpace[1] );
        else if( whiteSpace.size() == 3 )
            badChars.Printf( _( "%s, %s, or %s" ), whiteSpace[0], whiteSpace[1], whiteSpace[2] );
        else if( whiteSpace.size() == 4 )
            badChars.Printf( _( "%s, %s, %s, or %s" ),
                             whiteSpace[0], whiteSpace[1], whiteSpace[2], whiteSpace[3] );
        else
            wxCHECK_MSG( false, true, "Invalid illegal character in field validator." );

        msg.Printf( fieldCharError, badChars );
    }
    else if( m_fieldId == REFERENCE && val.Contains( wxT( "${" ) ) )
    {
        msg.Printf( _( "The reference designator cannot contain text variable references" ) );
    }

    if ( !msg.empty() )
    {
        m_validatorWindow->SetFocus();

        wxMessageBox( msg, _( "Field Validation Error" ), wxOK | wxICON_EXCLAMATION, aParent );

        return false;
    }

    return true;
}


wxString SCH_NETNAME_VALIDATOR::IsValid( const wxString& str ) const
{
    if( NET_SETTINGS::ParseBusGroup( str, nullptr, nullptr ) )
        return wxString();

    if( ( str.Contains( '[' ) || str.Contains( ']' ) ) &&
        !NET_SETTINGS::ParseBusVector( str, nullptr, nullptr ) )
        return _( "Signal name contains '[' or ']' but is not a valid vector bus name" );

    return NETNAME_VALIDATOR::IsValid( str );
}
