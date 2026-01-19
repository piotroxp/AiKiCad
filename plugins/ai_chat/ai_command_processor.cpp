/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ai_command_processor.h"
#include "ai_service.h"
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <eda_base_frame.h>
#include <sch_edit_frame.h>
#include <sch_symbol.h>
#include <sch_screen.h>
#include <schematic.h>
#include <commit.h>
#include <lib_id.h>
#include <tool/tool_manager.h>
#include <tool/actions.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <project_sch.h>
#include <project_pcb.h>
#include <libraries/symbol_library_adapter.h>
#include <footprint_library_adapter.h>
#include <libraries/library_manager.h>
#include <libraries/library_table.h>
#include <connection_graph.h>
#include <sch_commit.h>
#include <tool/tool_manager.h>
#include <tools/sch_actions.h>
// Forward declaration to avoid including sch_line_wire_bus_tool.h
// which pulls in headers not available in plugin context
class SCH_LINE_WIRE_BUS_TOOL;
#include <sch_line.h>
#include <set>
#include <algorithm>

// PCB-specific includes - only include when PCB types are available
// This prevents RTTI linking errors in eeschema-only builds
#if defined(PCBNEW) || defined(KICAD_BUILD_QA_TESTS)
#include <pcb_edit_frame.h>
#include <footprint_edit_frame.h>
#include <board.h>
#include <footprint.h>
#include <pcb_track.h>
#endif

// File operations implementation
bool FILE_OPERATIONS::SaveFile( const wxString& aPath, const wxString& aContent )
{
    wxFFile file( aPath, wxT( "w" ) );
    if( !file.IsOpened() )
        return false;
    return file.Write( aContent, wxConvUTF8 ) && file.Close();
}


bool FILE_OPERATIONS::LoadFile( const wxString& aPath, wxString& aContent )
{
    wxFFile file( aPath, wxT( "r" ) );
    if( !file.IsOpened() )
        return false;
    return file.ReadAll( &aContent, wxConvUTF8 );
}


bool FILE_OPERATIONS::FileExists( const wxString& aPath )
{
    return wxFileName::FileExists( aPath );
}

// Command processor implementation
AI_COMMAND_PROCESSOR::AI_COMMAND_PROCESSOR( EDA_BASE_FRAME* aFrame,
                                           std::unique_ptr<I_FILE_OPERATIONS> aFileOps ) :
        m_frame( aFrame ),
        m_fileOps( aFileOps ? std::move( aFileOps ) : std::make_unique<FILE_OPERATIONS>() ),
        m_aiService( std::make_unique<OLLAMA_AI_SERVICE>() )
{
}


AI_COMMAND_PROCESSOR::~AI_COMMAND_PROCESSOR()
{
}


wxString AI_COMMAND_PROCESSOR::GetContext() const
{
    if( !m_frame )
        return wxT( "unknown" );

    FRAME_T frameType = m_frame->GetFrameType();
    switch( frameType )
    {
    case FRAME_SCH:
    case FRAME_SCH_SYMBOL_EDITOR:
        return wxT( "schematic" );
    case FRAME_PCB_EDITOR:
    case FRAME_FOOTPRINT_EDITOR:
        return wxT( "board" );
    default:
        return wxT( "unknown" );
    }
}


void AI_COMMAND_PROCESSOR::SetFileOperations( std::unique_ptr<I_FILE_OPERATIONS> aFileOps )
{
    m_fileOps = std::move( aFileOps );
}


void AI_COMMAND_PROCESSOR::SetAIService( std::unique_ptr<I_AI_SERVICE> aAIService )
{
    m_aiService = std::move( aAIService );
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::ProcessCommand( const wxString& aCommand )
{
    if( !m_frame )
    {
        return { false, wxEmptyString, _( "No frame available" ) };
    }

    // First, try to use AI service if available
    if( m_aiService && m_aiService->IsAvailable() )
    {
        AI_CONTEXT context = gatherContext();

        AI_RESPONSE aiResponse = m_aiService->ProcessPrompt( aCommand, context );
        if( aiResponse.success )
        {
            // Try to extract and execute commands from AI response
            // Look for commands in backticks, numbered lists, or direct command lines
            wxString response = aiResponse.message;
            wxArrayString lines = wxSplit( response, '\n', '\0' );
            wxString executedCommands;
            wxString failedCommands;
            int commandCount = 0;
            int failedCount = 0;
            wxArrayString allCommands;
            
            // Extract all commands from the response
            for( const wxString& line : lines )
            {
                wxString trimmed = line;
                trimmed.Trim( false ).Trim( true );
                
                // Skip empty lines and markdown headers
                if( trimmed.IsEmpty() || trimmed.StartsWith( wxT( "#" ) ) )
                    continue;
                
                // Skip markdown bold headers like "**Add Ground Symbol**:"
                if( trimmed.StartsWith( wxT( "**" ) ) && trimmed.EndsWith( wxT( ":**" ) ) )
                    continue;
                
                // Look for commands in backticks: `command` or "command: `command`"
                int backtickStart = trimmed.Find( wxT( '`' ) );
                if( backtickStart != wxNOT_FOUND )
                {
                    int backtickEnd = trimmed.find( wxT( '`' ), backtickStart + 1 );
                    if( backtickEnd != wxNOT_FOUND )
                    {
                        wxString cmd = trimmed.SubString( backtickStart + 1, backtickEnd - 1 );
                        cmd.Trim( false ).Trim( true );
                        // Validate it's a real command
                        if( !cmd.IsEmpty() && 
                            ( cmd.Lower().StartsWith( wxT( "add" ) ) ||
                              cmd.Lower().StartsWith( wxT( "connect" ) ) ||
                              cmd.Lower().StartsWith( wxT( "wire" ) ) ||
                              cmd.Lower().StartsWith( wxT( "place" ) ) ) )
                        {
                            allCommands.Add( cmd );
                            continue;
                        }
                    }
                }
                
                // Look for "Command: `command`" or "command: `command`" pattern
                int cmdColonPos = trimmed.Lower().Find( wxT( "command:" ) );
                if( cmdColonPos != wxNOT_FOUND )
                {
                    wxString afterColon = trimmed.Mid( cmdColonPos + 8 ).Trim( false );
                    // Check for backtick
                    int btStart = afterColon.Find( wxT( '`' ) );
                    if( btStart != wxNOT_FOUND )
                    {
                        int btEnd = afterColon.find( wxT( '`' ), btStart + 1 );
                        if( btEnd != wxNOT_FOUND )
                        {
                            wxString cmd = afterColon.SubString( btStart + 1, btEnd - 1 );
                            cmd.Trim( false ).Trim( true );
                            if( !cmd.IsEmpty() )
                            {
                                allCommands.Add( cmd );
                                continue;
                            }
                        }
                    }
                }
                
                // Look for numbered list items: "1. command" or "1) command"
                if( trimmed.length() >= 3 && wxIsdigit( trimmed[0] ) && 
                    ( trimmed[1] == wxT( '.' ) || trimmed[1] == wxT( ')' ) ) )
                {
                    wxString cmd = trimmed.Mid( 2 ).Trim( false );
                    // Check if it's a direct command (not descriptive text)
                    if( cmd.Lower().StartsWith( wxT( "add" ) ) ||
                        cmd.Lower().StartsWith( wxT( "connect" ) ) ||
                        cmd.Lower().StartsWith( wxT( "wire" ) ) ||
                        cmd.Lower().StartsWith( wxT( "place" ) ) )
                    {
                        allCommands.Add( cmd );
                        continue;
                    }
                }
                
                // Look for direct command lines (starting with add, connect, wire, place)
                if( trimmed.Lower().StartsWith( wxT( "add" ) ) ||
                    trimmed.Lower().StartsWith( wxT( "connect" ) ) ||
                    trimmed.Lower().StartsWith( wxT( "wire" ) ) ||
                    trimmed.Lower().StartsWith( wxT( "place" ) ) )
                {
                    allCommands.Add( trimmed );
                }
            }
            
            // First pass: Place all components
            for( const wxString& command : allCommands )
            {
                wxString cmd = command;
                cmd.Trim( false ).Trim( true );
                
                // Only process component placement commands in first pass
                if( cmd.Lower().Contains( wxT( "component" ) ) ||
                    cmd.Lower().Contains( wxT( "symbol" ) ) )
                {
                    AI_COMMAND_RESULT cmdResult = processDirectCommand( cmd );
                    if( cmdResult.success )
                    {
                        commandCount++;
                        if( !executedCommands.IsEmpty() )
                            executedCommands += wxT( "\n" );
                        executedCommands += wxString::Format( wxT( "✓ %s" ), cmd );
                    }
                    else
                    {
                        failedCount++;
                        if( !failedCommands.IsEmpty() )
                            failedCommands += wxT( "\n" );
                        failedCommands += wxString::Format( wxT( "✗ %s (%s)" ), cmd, cmdResult.error );
                    }
                }
            }
            
            // Second pass: Connect components (after they're placed and annotated)
            // Refresh the schematic to ensure components are annotated
            SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
            if( schFrame && commandCount > 0 )
            {
                // Force annotation update
                schFrame->Schematic().CurrentSheet().LastScreen()->SetContentModified( true );
            }
            
            for( const wxString& command : allCommands )
            {
                wxString cmd = command;
                cmd.Trim( false ).Trim( true );
                
                // Only process connection commands in second pass
                if( cmd.Lower().Contains( wxT( "connect" ) ) ||
                    cmd.Lower().Contains( wxT( "wire" ) ) )
                {
                    AI_COMMAND_RESULT cmdResult = processDirectCommand( cmd );
                    if( cmdResult.success )
                    {
                        commandCount++;
                        if( !executedCommands.IsEmpty() )
                            executedCommands += wxT( "\n" );
                        executedCommands += wxString::Format( wxT( "✓ %s" ), cmd );
                    }
                    else
                    {
                        failedCount++;
                        if( !failedCommands.IsEmpty() )
                            failedCommands += wxT( "\n" );
                        failedCommands += wxString::Format( wxT( "✗ %s (%s)" ), cmd, cmdResult.error );
                    }
                }
            }
            
            // If we executed commands, combine with AI explanation
            if( commandCount > 0 || failedCount > 0 )
            {
                wxString result = wxString::Format( _( "Executed %d command(s)" ), commandCount );
                if( failedCount > 0 )
                    result += wxString::Format( _( ", %d failed" ), failedCount );
                result += wxT( ":\n" ) + executedCommands;
                if( !failedCommands.IsEmpty() )
                    result += wxT( "\n\nFailed:\n" ) + failedCommands;
                result += wxT( "\n\n" ) + aiResponse.message;
                return { true, result, wxEmptyString };
            }
            
            // AI provided a response, but also try to parse as direct command
            // This allows AI to explain or the user to use direct commands
            AI_COMMAND_RESULT directResult = processDirectCommand( aCommand );
            if( directResult.success )
            {
                // Direct command succeeded, combine with AI explanation if needed
                return { true, aiResponse.message + wxT( "\n\n" ) + directResult.message,
                        wxEmptyString };
            }
            else
            {
                // Use AI response as the result
                return { true, aiResponse.message, wxEmptyString };
            }
        }
    }

    // Fall back to direct command parsing
    return processDirectCommand( aCommand );
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::ProcessCommandsFromResponse( const wxString& aResponse )
{
    if( !m_frame )
    {
        return { false, wxEmptyString, _( "No frame available" ) };
    }

    // Extract and execute commands from AI response
    wxArrayString lines = wxSplit( aResponse, '\n', '\0' );
    wxString executedCommands;
    wxString failedCommands;
    int commandCount = 0;
    int failedCount = 0;
    wxArrayString allCommands;
    
    // Extract all commands from the response
    for( const wxString& line : lines )
    {
        wxString trimmed = line;
        trimmed.Trim( false ).Trim( true );
        
        // Skip empty lines and markdown headers
        if( trimmed.IsEmpty() || trimmed.StartsWith( wxT( "#" ) ) )
            continue;
        
        // Skip markdown bold headers like "**Add Ground Symbol**:"
        if( trimmed.StartsWith( wxT( "**" ) ) && trimmed.EndsWith( wxT( ":**" ) ) )
            continue;
        
        // Look for commands in backticks: `command` or "command: `command`"
        int backtickStart = trimmed.Find( wxT( '`' ) );
        if( backtickStart != wxNOT_FOUND )
        {
            int backtickEnd = trimmed.find( wxT( '`' ), backtickStart + 1 );
            if( backtickEnd != wxNOT_FOUND )
            {
                wxString cmd = trimmed.SubString( backtickStart + 1, backtickEnd - 1 );
                cmd.Trim( false ).Trim( true );
                // Validate it's a real command
                if( !cmd.IsEmpty() && 
                    ( cmd.Lower().StartsWith( wxT( "add" ) ) ||
                      cmd.Lower().StartsWith( wxT( "connect" ) ) ||
                      cmd.Lower().StartsWith( wxT( "wire" ) ) ||
                      cmd.Lower().StartsWith( wxT( "place" ) ) ) )
                {
                    allCommands.Add( cmd );
                    continue;
                }
            }
        }
        
        // Look for "Command: `command`" or "command: `command`" pattern
        int cmdColonPos = trimmed.Lower().Find( wxT( "command:" ) );
        if( cmdColonPos != wxNOT_FOUND )
        {
            wxString afterColon = trimmed.Mid( cmdColonPos + 8 ).Trim( false );
            // Check for backtick
            int btStart = afterColon.Find( wxT( '`' ) );
            if( btStart != wxNOT_FOUND )
            {
                int btEnd = afterColon.find( wxT( '`' ), btStart + 1 );
                if( btEnd != wxNOT_FOUND )
                {
                    wxString cmd = afterColon.SubString( btStart + 1, btEnd - 1 );
                    cmd.Trim( false ).Trim( true );
                    if( !cmd.IsEmpty() )
                    {
                        allCommands.Add( cmd );
                        continue;
                    }
                }
            }
        }
        
        // Look for numbered list items: "1. command" or "1) command"
        if( trimmed.length() >= 3 && wxIsdigit( trimmed[0] ) && 
            ( trimmed[1] == wxT( '.' ) || trimmed[1] == wxT( ')' ) ) )
        {
            wxString cmd = trimmed.Mid( 2 ).Trim( false );
            // Check if it's a direct command (not descriptive text)
            if( cmd.Lower().StartsWith( wxT( "add" ) ) ||
                cmd.Lower().StartsWith( wxT( "connect" ) ) ||
                cmd.Lower().StartsWith( wxT( "wire" ) ) ||
                cmd.Lower().StartsWith( wxT( "place" ) ) )
            {
                allCommands.Add( cmd );
                continue;
            }
        }
        
        // Look for direct command lines (starting with add, connect, wire, place)
        if( trimmed.Lower().StartsWith( wxT( "add" ) ) ||
            trimmed.Lower().StartsWith( wxT( "connect" ) ) ||
            trimmed.Lower().StartsWith( wxT( "wire" ) ) ||
            trimmed.Lower().StartsWith( wxT( "place" ) ) )
        {
            allCommands.Add( trimmed );
        }
    }
    
    // First pass: Place all components
    for( const wxString& command : allCommands )
    {
        wxString cmd = command;
        cmd.Trim( false ).Trim( true );
        
        // Only process component placement commands in first pass
        if( cmd.Lower().Contains( wxT( "component" ) ) ||
            cmd.Lower().Contains( wxT( "symbol" ) ) )
        {
            AI_COMMAND_RESULT cmdResult = processDirectCommand( cmd );
            if( cmdResult.success )
            {
                commandCount++;
                if( !executedCommands.IsEmpty() )
                    executedCommands += wxT( "\n" );
                executedCommands += wxString::Format( wxT( "✓ %s" ), cmd );
            }
            else
            {
                failedCount++;
                if( !failedCommands.IsEmpty() )
                    failedCommands += wxT( "\n" );
                failedCommands += wxString::Format( wxT( "✗ %s (%s)" ), cmd, cmdResult.error );
            }
        }
    }
    
    // Second pass: Connect components (after they're placed and annotated)
    // Refresh the schematic to ensure components are annotated
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( schFrame && commandCount > 0 )
    {
        // Force annotation update
        schFrame->Schematic().CurrentSheet().LastScreen()->SetContentModified( true );
    }
    
    for( const wxString& command : allCommands )
    {
        wxString cmd = command;
        cmd.Trim( false ).Trim( true );
        
        // Only process connection commands in second pass
        if( cmd.Lower().Contains( wxT( "connect" ) ) ||
            cmd.Lower().Contains( wxT( "wire" ) ) )
        {
            AI_COMMAND_RESULT cmdResult = processDirectCommand( cmd );
            if( cmdResult.success )
            {
                commandCount++;
                if( !executedCommands.IsEmpty() )
                    executedCommands += wxT( "\n" );
                executedCommands += wxString::Format( wxT( "✓ %s" ), cmd );
            }
            else
            {
                failedCount++;
                if( !failedCommands.IsEmpty() )
                    failedCommands += wxT( "\n" );
                failedCommands += wxString::Format( wxT( "✗ %s (%s)" ), cmd, cmdResult.error );
            }
        }
    }
    
    // Return result
    if( commandCount > 0 || failedCount > 0 )
    {
        wxString result = wxString::Format( _( "Executed %d command(s)" ), commandCount );
        if( failedCount > 0 )
            result += wxString::Format( _( ", %d failed" ), failedCount );
        result += wxT( ":\n" ) + executedCommands;
        if( !failedCommands.IsEmpty() )
            result += wxT( "\n\nFailed:\n" ) + failedCommands;
        return { true, result, wxEmptyString };
    }
    
    // No commands found in response
    return { false, wxEmptyString, _( "No commands found in response" ) };
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::processDirectCommand( const wxString& aCommand )
{
    wxString cmd = aCommand.Lower().Trim();
    wxString context = GetContext();

    // Route to appropriate handler based on context
    if( context == wxT( "schematic" ) )
        return processSchematicCommand( cmd );
    else if( context == wxT( "board" ) )
        return processBoardCommand( cmd );
    else
        return processGenericCommand( cmd );
}


// Helper function to get library names from all library tables (global + project)
static wxArrayString GetLibraryNames( SYMBOL_LIBRARY_ADAPTER* aAdapter )
{
    wxArrayString names;
    if( !aAdapter )
        return names;
    
    // Get rows from both global and project library tables
    // Use BOTH scope to get all libraries, and include invalid ones to see what's configured
    std::vector<LIBRARY_TABLE_ROW*> libRows = aAdapter->Rows( LIBRARY_TABLE_SCOPE::BOTH, true );
    for( LIBRARY_TABLE_ROW* row : libRows )
    {
        if( row )
        {
            wxString nickname = row->Nickname();
            if( !nickname.IsEmpty() )
                names.Add( nickname );
        }
    }
    return names;
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::processSchematicCommand( const wxString& aCommand )
{
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( !schFrame )
    {
        return { false, wxEmptyString, _( "Not in schematic editor" ) };
    }

    // Parse "add component <name> [at <x>,<y>]" or "add <name>"
    if( aCommand.Contains( wxT( "add" ) ) && ( aCommand.Contains( wxT( "component" ) ) || 
                                                aCommand.Contains( wxT( "symbol" ) ) ) )
    {
        wxString componentName;
        VECTOR2I position( 0, 0 );
        bool hasPosition = false;

        if( parseAddComponent( aCommand, componentName, position ) )
        {
            hasPosition = ( position.x != 0 || position.y != 0 );
            
            // If no position specified, use a default grid position
            if( !hasPosition )
            {
                // Use a reasonable default position (e.g., center of visible area or last placed)
                position = VECTOR2I( 100000, 100000 ); // Default in internal units (100mm)
            }

            LIB_ID libId;
            
            // Check if componentName is in "Library:Symbol" format
            if( componentName.Contains( wxT( ":" ) ) )
            {
                wxString libName = componentName.BeforeFirst( wxT( ':' ) );
                wxString symName = componentName.AfterFirst( wxT( ':' ) );
                
                // Try to find the library by case-insensitive matching
                PROJECT& project = schFrame->Prj();
                SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &project );
                if( adapter )
                {
                    // Get rows from both global and project library tables
                    std::vector<LIBRARY_TABLE_ROW*> libRows = adapter->Rows( LIBRARY_TABLE_SCOPE::BOTH, true );
                    wxString actualLibName;
                    
                    for( LIBRARY_TABLE_ROW* row : libRows )
                    {
                        if( row && row->Nickname().CmpNoCase( libName ) == 0 )
                        {
                            actualLibName = row->Nickname(); // Use exact case from library table
                            break;
                        }
                    }
                    
                    if( actualLibName.IsEmpty() )
                    {
                        wxArrayString availableLibs = GetLibraryNames( adapter );
                        wxString libList = availableLibs.IsEmpty() ? 
                            wxT( "(none - check Preferences > Manage Symbol Libraries)" ) :
                            wxJoin( availableLibs, wxT( ',' ) );
                        return { false, wxEmptyString, 
                                wxString::Format( _( "Library '%s' not found. Available libraries: %s" ),
                                                libName, libList ) };
                    }
                    
                    libId.SetLibNickname( actualLibName );
                    libId.SetLibItemName( symName );
                }
                else
                {
                    // Fallback: use as-is
                    libId.SetLibNickname( libName );
                    libId.SetLibItemName( symName );
                }
            }
            else
            {
                // Try to find the symbol in libraries by name only
                if( !findSymbolByName( componentName, libId ) )
                {
                    return { false, wxEmptyString, 
                            wxString::Format( _( "Component '%s' not found in libraries" ), componentName ) };
                }
            }
            
            // Now place the component
            if( executePlaceComponent( libId, position ) )
            {
                return { true, wxString::Format( _( "Added component '%s' at (%d, %d)" ),
                                                componentName, position.x, position.y ),
                        wxEmptyString };
            }
            else
            {
                return { false, wxEmptyString, 
                        wxString::Format( _( "Failed to place component '%s' (library: %s, symbol: %s). "
                                           "Check that the library is loaded and the symbol exists." ),
                                        componentName, libId.GetLibNickname().wx_str(), libId.GetLibItemName().wx_str() ) };
            }
        }
    }
    // Parse "connect <ref1>.<pin1> to <ref2>.<pin2>" or "wire <ref1>.<pin1> to <ref2>.<pin2>"
    else if( aCommand.Contains( wxT( "connect" ) ) || aCommand.Contains( wxT( "wire" ) ) )
    {
        wxString ref1, pin1, ref2, pin2;
        if( parseConnectCommand( aCommand, ref1, pin1, ref2, pin2 ) )
        {
            if( executeConnectComponents( ref1, pin1, ref2, pin2 ) )
            {
                return { true, wxString::Format( _( "Connected %s.%s to %s.%s" ),
                                                ref1, pin1, ref2, pin2 ),
                        wxEmptyString };
            }
            else
            {
                return { false, wxEmptyString, 
                        wxString::Format( _( "Failed to connect %s.%s to %s.%s" ),
                                         ref1, pin1, ref2, pin2 ) };
            }
        }
    }
    // Parse "modify component <refdes>"
    else if( aCommand.Contains( wxT( "modify" ) ) || aCommand.Contains( wxT( "change" ) ) )
    {
        wxString refDes;
        if( parseModifyComponent( aCommand, refDes ) )
        {
            return { true, wxString::Format( _( "Would modify component '%s'" ), refDes ),
                    wxEmptyString };
        }
    }

    return { false, wxEmptyString, _( "Command not recognized or incomplete" ) };
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::processBoardCommand( const wxString& aCommand )
{
    if( !m_frame )
        return { false, wxEmptyString, _( "No frame available" ) };

    FRAME_T frameType = m_frame->GetFrameType();
    
    // Only process PCB commands if we're in a PCB context
    if( frameType != FRAME_PCB_EDITOR && frameType != FRAME_FOOTPRINT_EDITOR )
        return { false, wxEmptyString, _( "Command not recognized or incomplete" ) };

#if defined(PCBNEW) || defined(KICAD_BUILD_QA_TESTS)
    // Only use dynamic_cast when PCB types are available at compile/link time
    // This prevents RTTI linking errors in eeschema-only builds
    PCB_EDIT_FRAME* pcbFrame = dynamic_cast<PCB_EDIT_FRAME*>( m_frame );
    FOOTPRINT_EDIT_FRAME* fpFrame = dynamic_cast<FOOTPRINT_EDIT_FRAME*>( m_frame );

    if( pcbFrame )
    {
        // Parse "add trace from <x1>,<y1> to <x2>,<y2> width <w>"
        if( aCommand.Contains( wxT( "add" ) ) && aCommand.Contains( wxT( "trace" ) ) )
        {
            VECTOR2I start, end;
            int width = 0;

            if( parseAddTrace( aCommand, start, end, width ) )
            {
                return { true, wxString::Format( _( "Would add trace from (%d, %d) to (%d, %d) width %d" ),
                                                start.x, start.y, end.x, end.y, width ),
                        wxEmptyString };
            }
        }
    }
    else if( fpFrame )
    {
        // Parse "modify footprint <name>"
        if( aCommand.Contains( wxT( "modify" ) ) || aCommand.Contains( wxT( "change" ) ) )
        {
            wxString footprintName;
            if( parseModifyFootprint( aCommand, footprintName ) )
            {
                return { true, wxString::Format( _( "Would modify footprint '%s'" ), footprintName ),
                        wxEmptyString };
            }
        }
    }
#endif

    return { false, wxEmptyString, _( "Command not recognized or incomplete" ) };
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::processFootprintCommand( const wxString& aCommand )
{
    return processBoardCommand( aCommand );
}


AI_COMMAND_RESULT AI_COMMAND_PROCESSOR::processGenericCommand( const wxString& aCommand )
{
    if( aCommand.Contains( wxT( "help" ) ) || aCommand.Contains( wxT( "?" ) ) )
    {
        wxString help = _( "Available commands:\n" );
        help += _( "- Add component <name> [at <x>,<y>]\n" );
        help += _( "- Modify component <refdes>\n" );
        help += _( "- Add trace from <x1>,<y1> to <x2>,<y2> [width <w>]\n" );
        help += _( "- Modify footprint <name>\n" );
        help += _( "- List components - Show components in current design\n" );
        help += _( "- List libraries - Show available symbol libraries\n" );
        help += _( "- List footprints - Show available footprint libraries\n" );
        help += _( "- Query <component/footprint name> - Search for specific parts\n" );
        return { true, help, wxEmptyString };
    }
    else if( aCommand.Contains( wxT( "list" ) ) && aCommand.Contains( wxT( "component" ) ) )
    {
        AI_CONTEXT context = gatherContext();
        if( context.availableComponents.empty() )
        {
            return { true, _( "No components found in current design." ), wxEmptyString };
        }

        wxString result = _( "Components in current design:\n" );
        for( const wxString& comp : context.availableComponents )
        {
            // Only show components from current design (not libraries)
            if( !comp.Contains( wxT( ":" ) ) ) // Library entries have "library:name" format
            {
                result += wxString::Format( wxT( "  - %s\n" ), comp );
            }
        }
        return { true, result, wxEmptyString };
    }
    else if( aCommand.Contains( wxT( "list" ) ) && aCommand.Contains( wxT( "librar" ) ) )
    {
        AI_CONTEXT context = gatherContext();
        wxString result = _( "Available symbol libraries and components:\n" );
        
        std::set<wxString> libraries;
        for( const wxString& comp : context.availableComponents )
        {
            if( comp.Contains( wxT( ":" ) ) )
            {
                wxString libName = comp.BeforeFirst( wxT( ':' ) );
                libraries.insert( libName );
            }
        }

        for( const wxString& lib : libraries )
        {
            result += wxString::Format( wxT( "  Library: %s\n" ), lib );
            int count = 0;
            for( const wxString& comp : context.availableComponents )
            {
                if( comp.StartsWith( lib + wxT( ":" ) ) && count < 10 )
                {
                    result += wxString::Format( wxT( "    - %s\n" ), comp.AfterFirst( wxT( ':' ) ) );
                    count++;
                }
            }
            if( count >= 10 )
                result += wxT( "    ... (more components available)\n" );
        }
        return { true, result, wxEmptyString };
    }
    else if( aCommand.Contains( wxT( "list" ) ) && aCommand.Contains( wxT( "footprint" ) ) )
    {
        AI_CONTEXT context = gatherContext();
        wxString result = _( "Available footprint libraries and footprints:\n" );
        
        std::set<wxString> libraries;
        for( const wxString& fp : context.availableFootprints )
        {
            if( fp.Contains( wxT( ":" ) ) )
            {
                wxString libName = fp.BeforeFirst( wxT( ':' ) );
                libraries.insert( libName );
            }
        }

        for( const wxString& lib : libraries )
        {
            result += wxString::Format( wxT( "  Library: %s\n" ), lib );
            int count = 0;
            for( const wxString& fp : context.availableFootprints )
            {
                if( fp.StartsWith( lib + wxT( ":" ) ) && count < 10 )
                {
                    result += wxString::Format( wxT( "    - %s\n" ), fp.AfterFirst( wxT( ':' ) ) );
                    count++;
                }
            }
            if( count >= 10 )
                result += wxT( "    ... (more footprints available)\n" );
        }
        return { true, result, wxEmptyString };
    }
    else if( aCommand.Contains( wxT( "query" ) ) || aCommand.Contains( wxT( "search" ) ) )
    {
        // Extract search term
        wxString searchTerm = aCommand;
        searchTerm.Replace( wxT( "query" ), wxT( "" ) );
        searchTerm.Replace( wxT( "search" ), wxT( "" ) );
        searchTerm.Trim( true ).Trim( false );
        searchTerm = searchTerm.Lower();

        if( searchTerm.IsEmpty() )
        {
            return { false, wxEmptyString, _( "Please specify what to search for." ) };
        }

        AI_CONTEXT context = gatherContext();
        wxString result = wxString::Format( _( "Search results for '%s':\n" ), searchTerm );

        bool found = false;
        for( const wxString& comp : context.availableComponents )
        {
            if( comp.Lower().Contains( searchTerm ) )
            {
                result += wxString::Format( wxT( "  Component: %s\n" ), comp );
                found = true;
            }
        }

        for( const wxString& fp : context.availableFootprints )
        {
            if( fp.Lower().Contains( searchTerm ) )
            {
                result += wxString::Format( wxT( "  Footprint: %s\n" ), fp );
                found = true;
            }
        }

        if( !found )
        {
            result += _( "No matches found." );
        }

        return { true, result, wxEmptyString };
    }

    return { false, wxEmptyString, _( "Command not recognized. Type 'help' for available commands." ) };
}


bool AI_COMMAND_PROCESSOR::parseAddComponent( const wxString& aCommand, wxString& aComponentName,
                                              VECTOR2I& aPosition )
{
    // Parse: "add component <library>:<symbol> at <x>,<y>" or "add component <library>:<symbol>"
    // Handle both Library:Symbol format and just Symbol (will search libraries)
    wxRegEx regex( wxT( "add\\s+component\\s+([\\w\\-]+):([\\w\\-]+)(?:\\s+at\\s+([+-]?\\d+)\\s*,\\s*([+-]?\\d+))?" ),
                   wxRE_ICASE );
    if( regex.Matches( aCommand ) )
    {
        // Format: Library:Symbol
        wxString libName = regex.GetMatch( aCommand, 1 );
        wxString symName = regex.GetMatch( aCommand, 2 );
        aComponentName = libName + wxT( ":" ) + symName;
        
        if( regex.GetMatchCount() >= 5 )
        {
            long x, y;
            if( regex.GetMatch( aCommand, 3 ).ToLong( &x ) &&
                regex.GetMatch( aCommand, 4 ).ToLong( &y ) )
            {
                aPosition.x = static_cast<int>( x );
                aPosition.y = static_cast<int>( y );
            }
        }
        return true;
    }
    
    // Try without library (just symbol name): "add component <symbol> at <x>,<y>"
    wxRegEx regex2( wxT( "add\\s+component\\s+(\\w+)(?:\\s+at\\s+([+-]?\\d+)\\s*,\\s*([+-]?\\d+))?" ),
                    wxRE_ICASE );
    if( regex2.Matches( aCommand ) )
    {
        aComponentName = regex2.GetMatch( aCommand, 1 );
        if( regex2.GetMatchCount() >= 4 )
        {
            long x, y;
            if( regex2.GetMatch( aCommand, 2 ).ToLong( &x ) &&
                regex2.GetMatch( aCommand, 3 ).ToLong( &y ) )
            {
                aPosition.x = static_cast<int>( x );
                aPosition.y = static_cast<int>( y );
            }
        }
        return true;
    }
    
    return false;
}


bool AI_COMMAND_PROCESSOR::parseModifyComponent( const wxString& aCommand, wxString& aRefDes )
{
    wxRegEx regex( wxT( "(?:modify|change)\\s+component\\s+(\\w+)" ), wxRE_ICASE );
    if( regex.Matches( aCommand ) )
    {
        aRefDes = regex.GetMatch( aCommand, 1 );
        return true;
    }
    return false;
}


bool AI_COMMAND_PROCESSOR::parseAddTrace( const wxString& aCommand, VECTOR2I& aStart,
                                          VECTOR2I& aEnd, int& aWidth )
{
    wxRegEx regex( wxT( "add\\s+trace\\s+from\\s+(\\d+)\\s*,\\s*(\\d+)\\s+to\\s+(\\d+)\\s*,\\s*(\\d+)(?:\\s+width\\s+(\\d+))?" ),
                   wxRE_ICASE );
    if( regex.Matches( aCommand ) )
    {
        long x1, y1, x2, y2;
        if( regex.GetMatch( aCommand, 1 ).ToLong( &x1 ) &&
            regex.GetMatch( aCommand, 2 ).ToLong( &y1 ) &&
            regex.GetMatch( aCommand, 3 ).ToLong( &x2 ) &&
            regex.GetMatch( aCommand, 4 ).ToLong( &y2 ) )
        {
            aStart.x = static_cast<int>( x1 );
            aStart.y = static_cast<int>( y1 );
            aEnd.x = static_cast<int>( x2 );
            aEnd.y = static_cast<int>( y2 );

            if( regex.GetMatchCount() >= 6 && !regex.GetMatch( aCommand, 5 ).IsEmpty() )
            {
                long w;
                if( regex.GetMatch( aCommand, 5 ).ToLong( &w ) )
                    aWidth = static_cast<int>( w );
            }
            else
            {
                aWidth = 0; // Default width
            }
            return true;
        }
    }
    return false;
}


bool AI_COMMAND_PROCESSOR::parseModifyFootprint( const wxString& aCommand, wxString& aFootprintName )
{
    wxRegEx regex( wxT( "(?:modify|change)\\s+footprint\\s+(\\w+)" ), wxRE_ICASE );
    if( regex.Matches( aCommand ) )
    {
        aFootprintName = regex.GetMatch( aCommand, 1 );
        return true;
    }
    return false;
}


AI_CONTEXT AI_COMMAND_PROCESSOR::gatherContext() const
{
    AI_CONTEXT context;
    context.editorType = GetContext();

    if( !m_frame )
        return context;

    // Get file and project information
    // Try to get filename from schematic or board
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( schFrame )
    {
        context.fileName = schFrame->Schematic().GetFileName();
    }
#if defined(PCBNEW) || defined(KICAD_BUILD_QA_TESTS)
    else
    {
        PCB_EDIT_FRAME* pcbFrame = dynamic_cast<PCB_EDIT_FRAME*>( m_frame );
        if( pcbFrame && pcbFrame->GetBoard() )
        {
            context.fileName = pcbFrame->GetBoard()->GetFileName();
        }
    }
#endif
    
    PROJECT& project = m_frame->Prj();
    context.projectPath = project.GetProjectPath();

    // Gather context based on editor type
    if( context.editorType == wxT( "schematic" ) )
    {
        gatherSchematicContext( context );
    }
    else if( context.editorType == wxT( "board" ) )
    {
        gatherBoardContext( context );
    }

    // Always gather library information (available in both contexts)
    gatherSymbolLibraries( context );
    
    // Only gather footprint libraries if PCB types are available
#if defined(PCBNEW) || defined(KICAD_BUILD_QA_TESTS)
    gatherFootprintLibraries( context );
#endif

    return context;
}


void AI_COMMAND_PROCESSOR::gatherSchematicContext( AI_CONTEXT& aContext ) const
{
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( !schFrame )
        return;

    SCHEMATIC& schematic = schFrame->Schematic();

    // Gather components from schematic
    SCH_SHEET_LIST sheets = schematic.Hierarchy();
    SCH_REFERENCE_LIST refList;
    sheets.GetSymbols( refList );

    for( size_t i = 0; i < refList.GetCount(); ++i )
    {
        SCH_REFERENCE ref = refList[i];
        wxString componentInfo = wxString::Format( wxT( "%s (%s)" ),
                                                   ref.GetRef(),
                                                   ref.GetSymbol()->GetLibId().GetLibItemName().wx_str() );
        aContext.availableComponents.push_back( componentInfo );
    }

    // Gather net information if connection graph is available
    CONNECTION_GRAPH* graph = schematic.ConnectionGraph();
    if( graph )
    {
        // Note: Net names could be added to context if needed
        // For now, we focus on components
    }
}


void AI_COMMAND_PROCESSOR::gatherBoardContext( AI_CONTEXT& aContext ) const
{
#if defined(PCBNEW) || defined(KICAD_BUILD_QA_TESTS)
    PCB_EDIT_FRAME* pcbFrame = dynamic_cast<PCB_EDIT_FRAME*>( m_frame );
    if( !pcbFrame )
        return;

    BOARD* board = pcbFrame->GetBoard();
    if( !board )
        return;

    // Gather footprints from board
    for( FOOTPRINT* footprint : board->Footprints() )
    {
        wxString footprintInfo = wxString::Format( wxT( "%s (%s)" ),
                                                    footprint->GetReference(),
                                                    footprint->GetFPID().GetLibItemName().wx_str() );
        aContext.availableFootprints.push_back( footprintInfo );
    }
#endif
}


void AI_COMMAND_PROCESSOR::gatherSymbolLibraries( AI_CONTEXT& aContext ) const
{
    if( !m_frame )
        return;

    PROJECT& project = m_frame->Prj();
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &project );
    if( !adapter )
        return;

    // Use adapter's Rows() method directly - it's filtered to the adapter's type
    std::vector<LIBRARY_TABLE_ROW*> libRows = adapter->Rows();
    std::vector<wxString> libNames;
    for( LIBRARY_TABLE_ROW* row : libRows )
    {
        if( row )
            libNames.push_back( row->Nickname() );
    }

    // Limit to first 20 libraries and first 50 symbols per library to avoid overwhelming context
    size_t libCount = std::min( libNames.size(), size_t( 20 ) );
    for( size_t i = 0; i < libCount; ++i )
    {
        const wxString& libName = libNames[i];
        std::vector<wxString> symbolNames = adapter->GetSymbolNames( libName );
        
        size_t symbolCount = std::min( symbolNames.size(), size_t( 50 ) );
        for( size_t j = 0; j < symbolCount; ++j )
        {
            wxString symbolInfo = wxString::Format( wxT( "%s:%s" ), libName, symbolNames[j] );
            aContext.availableComponents.push_back( symbolInfo );
        }
    }
}


void AI_COMMAND_PROCESSOR::gatherFootprintLibraries( AI_CONTEXT& aContext ) const
{
#if defined(PCBNEW) || defined(KICAD_BUILD_QA_TESTS)
    if( !m_frame )
        return;

    PROJECT& project = m_frame->Prj();
    FOOTPRINT_LIBRARY_ADAPTER* adapter = PROJECT_PCB::FootprintLibAdapter( &project );
    if( !adapter )
        return;

    // Use adapter's Rows() method directly - it's filtered to the adapter's type
    std::vector<LIBRARY_TABLE_ROW*> libRows = adapter->Rows();
    std::vector<wxString> libNames;
    for( LIBRARY_TABLE_ROW* row : libRows )
    {
        if( row )
            libNames.push_back( row->Nickname() );
    }

    // Limit to first 20 libraries and first 50 footprints per library
    size_t libCount = std::min( libNames.size(), size_t( 20 ) );
    for( size_t i = 0; i < libCount; ++i )
    {
        const wxString& libName = libNames[i];
        std::vector<wxString> footprintNames = adapter->GetFootprintNames( libName, true );
        
        size_t footprintCount = std::min( footprintNames.size(), size_t( 50 ) );
        for( size_t j = 0; j < footprintCount; ++j )
        {
            wxString footprintInfo = wxString::Format( wxT( "%s:%s" ), libName, footprintNames[j] );
            aContext.availableFootprints.push_back( footprintInfo );
        }
    }
#endif
}


bool AI_COMMAND_PROCESSOR::findSymbolByName( const wxString& aSymbolName, LIB_ID& aLibId )
{
    if( !m_frame )
        return false;

    PROJECT& project = m_frame->Prj();
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &project );
    if( !adapter )
        return false;

    // Check if symbol name contains library:name format
    if( aSymbolName.Contains( wxT( ":" ) ) )
    {
        wxString libName = aSymbolName.BeforeFirst( wxT( ':' ) );
        wxString symbolName = aSymbolName.AfterFirst( wxT( ':' ) );
        
        aLibId.SetLibNickname( libName );
        aLibId.SetLibItemName( symbolName );
        
        // Verify it exists
        LIB_SYMBOL* symbol = adapter->LoadSymbol( libName, symbolName );
        if( symbol )
            return true;
    }
    else
    {
        // Search all libraries for the symbol
        std::vector<LIBRARY_TABLE_ROW*> libRows = adapter->Rows();
        
        for( LIBRARY_TABLE_ROW* row : libRows )
        {
            if( !row )
                continue;
                
            wxString libName = row->Nickname();
            std::vector<wxString> symbolNames = adapter->GetSymbolNames( libName );
            
            for( const wxString& symName : symbolNames )
            {
                if( symName.CmpNoCase( aSymbolName ) == 0 )
                {
                    aLibId.SetLibNickname( libName );
                    aLibId.SetLibItemName( symName );
                    return true;
                }
            }
        }
    }
    
    return false;
}


bool AI_COMMAND_PROCESSOR::executePlaceComponent( const LIB_ID& aLibId, const VECTOR2I& aPosition )
{
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( !schFrame )
        return false;

    PROJECT& project = schFrame->Prj();
    SYMBOL_LIBRARY_ADAPTER* adapter = PROJECT_SCH::SymbolLibAdapter( &project );
    if( !adapter )
        return false;

    LIB_SYMBOL* libSymbol = nullptr;

    wxString libNickname = aLibId.GetLibNickname();
    wxString libItemName = aLibId.GetLibItemName();

    std::vector<LIBRARY_TABLE_ROW*> libRows = adapter->Rows();
    wxString actualLibNickname;

    for( LIBRARY_TABLE_ROW* row : libRows )
    {
        if( !row )
            continue;

        if( row->Nickname().CmpNoCase( libNickname ) == 0 )
        {
            actualLibNickname = row->Nickname();
            libSymbol = adapter->LoadSymbol( actualLibNickname, libItemName );
            if( libSymbol )
                break;
        }
    }

    if( !libSymbol )
    {
        wxLogDebug( wxT( "AI Chat: Could not find symbol '%s' in library '%s'" ), libItemName, libNickname );
        return false;
    }

    SCH_SCREEN* screen = schFrame->GetScreen();
    if( !screen )
        return false;

    LIB_ID correctLibId( actualLibNickname, libItemName );

    SCH_SYMBOL* symbol = new SCH_SYMBOL( *libSymbol, correctLibId, &schFrame->GetCurrentSheet(), 1, 0, aPosition );
    symbol->SetParent( screen );
    symbol->SetFlags( IS_NEW );

    if( schFrame->eeconfig()->m_AutoplaceFields.enable )
    {
        symbol->AutoplaceFields( screen, AUTOPLACE_AUTO );
    }

    SCH_COMMIT commit( schFrame->GetToolManager() );
    schFrame->AddToScreen( symbol, screen );
    commit.Added( symbol, screen );
    commit.Push( _( "Place Symbol" ) );

    schFrame->GetCanvas()->Refresh();

    return true;
}


bool AI_COMMAND_PROCESSOR::executeDrawWire( const VECTOR2I& aStart, const VECTOR2I& aEnd )
{
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( !schFrame )
        return false;

    SCH_SCREEN* screen = schFrame->GetScreen();
    if( !screen )
        return false;

    // Create wire
    SCH_LINE* wire = new SCH_LINE( aStart, LAYER_WIRE );
    wire->SetEndPoint( aEnd );
    wire->SetParent( &schFrame->Schematic() );
    wire->SetFlags( IS_NEW );

    // Add to screen and commit
    SCH_COMMIT commit( schFrame->GetToolManager() );
    schFrame->AddToScreen( wire, screen );
    commit.Added( wire, screen );

    // Trim overlapping wires and add junctions if needed
    // Note: We use forward declaration to avoid including sch_line_wire_bus_tool.h
    // which pulls in symbol_edit_frame.h that's not available in plugin context
    // The wire trimming will happen automatically on next schematic update

    commit.Push( _( "Draw Wire" ) );

    return true;
}


bool AI_COMMAND_PROCESSOR::parseConnectCommand( const wxString& aCommand, wxString& aRef1,
                                                wxString& aPin1, wxString& aRef2, wxString& aPin2 )
{
    // Parse patterns like:
    // "connect U1.VIN to C1.1"
    // "wire U1.VIN to C1.1"
    // "connect U1 pin VIN to C1 pin 1"
    // "connect U1.VIN to C1.P1" (handle P prefix)
    
    // Pattern 1: "connect U1.VIN to C1.1" or "connect U1.VIN to C1.P1"
    wxRegEx regex1( wxT( "(?:connect|wire)\\s+(\\w+)\\.([\\w\\d]+)\\s+to\\s+(\\w+)\\.([\\w\\d]+)" ), wxRE_ICASE );
    if( regex1.Matches( aCommand ) )
    {
        aRef1 = regex1.GetMatch( aCommand, 1 );
        aPin1 = regex1.GetMatch( aCommand, 2 );
        aRef2 = regex1.GetMatch( aCommand, 3 );
        aPin2 = regex1.GetMatch( aCommand, 4 );
        return true;
    }
    
    // Pattern 2: "connect U1 pin VIN to C1 pin 1"
    wxRegEx regex2( wxT( "(?:connect|wire)\\s+(\\w+)\\s+pin\\s+([\\w\\d]+)\\s+to\\s+(\\w+)\\s+pin\\s+([\\w\\d]+)" ), wxRE_ICASE );
    if( regex2.Matches( aCommand ) )
    {
        aRef1 = regex2.GetMatch( aCommand, 1 );
        aPin1 = regex2.GetMatch( aCommand, 2 );
        aRef2 = regex2.GetMatch( aCommand, 3 );
        aPin2 = regex2.GetMatch( aCommand, 4 );
        return true;
    }
    
    return false;
}


bool AI_COMMAND_PROCESSOR::executeConnectComponents( const wxString& aRef1, const wxString& aPin1,
                                                      const wxString& aRef2, const wxString& aPin2 )
{
    SCH_EDIT_FRAME* schFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );
    if( !schFrame )
        return false;

    SCHEMATIC& schematic = schFrame->Schematic();
    SCH_SHEET_LIST sheets = schematic.Hierarchy();
    SCH_REFERENCE_LIST refList;
    sheets.GetSymbols( refList );

    SCH_SYMBOL* symbol1 = nullptr;
    SCH_SYMBOL* symbol2 = nullptr;
    SCH_PIN* pin1 = nullptr;
    SCH_PIN* pin2 = nullptr;

    // Find symbols by reference
    for( size_t i = 0; i < refList.GetCount(); ++i )
    {
        SCH_REFERENCE ref = refList[i];
        wxString refDes = ref.GetRef();
        
        if( refDes.CmpNoCase( aRef1 ) == 0 )
        {
            symbol1 = ref.GetSymbol();
        }
        if( refDes.CmpNoCase( aRef2 ) == 0 )
        {
            symbol2 = ref.GetSymbol();
        }
    }

    if( !symbol1 || !symbol2 )
        return false;

    // Find pins by name or number
    // Handle both pin names (VIN, VOUT, GND) and pin numbers (1, 2, P1, P2)
    std::vector<SCH_PIN*> pins1 = symbol1->GetPins();
    std::vector<SCH_PIN*> pins2 = symbol2->GetPins();

    // Create search variants for pins (handle P1 -> 1, etc.)
    wxString pin1Search = aPin1;
    wxString pin2Search = aPin2;
    
    // Remove "P" prefix if present (e.g., "P1" -> "1")
    wxString pin1Alt;
    wxString pin2Alt;
    if( aPin1.StartsWith( wxT( "P" ) ) && aPin1.Length() > 1 )
    {
        pin1Alt = aPin1.Mid( 1 ); // Remove "P" prefix
    }
    if( aPin2.StartsWith( wxT( "P" ) ) && aPin2.Length() > 1 )
    {
        pin2Alt = aPin2.Mid( 1 ); // Remove "P" prefix
    }

    for( SCH_PIN* pin : pins1 )
    {
        wxString pinName = pin->GetShownName();
        wxString pinNumber = pin->GetShownNumber();
        
        // Try exact match with original, then with P prefix removed
        if( pinName.CmpNoCase( aPin1 ) == 0 || pinNumber.CmpNoCase( aPin1 ) == 0 ||
            ( !pin1Alt.IsEmpty() && ( pinName.CmpNoCase( pin1Alt ) == 0 || pinNumber.CmpNoCase( pin1Alt ) == 0 ) ) )
        {
            pin1 = pin;
            break;
        }
    }

    for( SCH_PIN* pin : pins2 )
    {
        wxString pinName = pin->GetShownName();
        wxString pinNumber = pin->GetShownNumber();
        
        // Try exact match with original, then with P prefix removed
        if( pinName.CmpNoCase( aPin2 ) == 0 || pinNumber.CmpNoCase( aPin2 ) == 0 ||
            ( !pin2Alt.IsEmpty() && ( pinName.CmpNoCase( pin2Alt ) == 0 || pinNumber.CmpNoCase( pin2Alt ) == 0 ) ) )
        {
            pin2 = pin;
            break;
        }
    }

    if( !pin1 || !pin2 )
        return false;

    // Get pin positions (in schematic coordinates)
    VECTOR2I pos1 = pin1->GetPosition();
    VECTOR2I pos2 = pin2->GetPosition();

    // Draw wire between pins
    return executeDrawWire( pos1, pos2 );
}
