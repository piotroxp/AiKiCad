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

#ifndef AI_COMMAND_PROCESSOR_H
#define AI_COMMAND_PROCESSOR_H

#include <wx/string.h>
#include <memory>
#include <eda_base_frame.h>
#include <lib_id.h>
#include "ai_service.h"

class BOARD;
class SCHEMATIC;
class FOOTPRINT;

/**
 * Result of processing an AI command.
 */
struct AI_COMMAND_RESULT
{
    bool success = false;
    wxString message;
    wxString error;
};

/**
 * Interface for file operations (mockable for tests).
 */
class I_FILE_OPERATIONS
{
public:
    virtual ~I_FILE_OPERATIONS() = default;
    virtual bool SaveFile( const wxString& aPath, const wxString& aContent ) = 0;
    virtual bool LoadFile( const wxString& aPath, wxString& aContent ) = 0;
    virtual bool FileExists( const wxString& aPath ) = 0;
};

/**
 * Default file operations implementation.
 */
class FILE_OPERATIONS : public I_FILE_OPERATIONS
{
public:
    bool SaveFile( const wxString& aPath, const wxString& aContent ) override;
    bool LoadFile( const wxString& aPath, wxString& aContent ) override;
    bool FileExists( const wxString& aPath ) override;
};

/**
 * Processes natural language commands and executes design actions.
 * Provides idempotent operations and mockable file operations for testing.
 */
class AI_COMMAND_PROCESSOR
{
public:
    AI_COMMAND_PROCESSOR( EDA_BASE_FRAME* aFrame,
                         std::unique_ptr<I_FILE_OPERATIONS> aFileOps = nullptr );
    ~AI_COMMAND_PROCESSOR();

    /**
     * Process a natural language command.
     * Returns a result indicating success/failure and a message.
     * Operations are idempotent - repeated commands yield consistent results.
     */
    AI_COMMAND_RESULT ProcessCommand( const wxString& aCommand );

    /**
     * Extract and execute commands from an AI response message.
     * This bypasses the AI service call and directly processes commands found in the response.
     */
    AI_COMMAND_RESULT ProcessCommandsFromResponse( const wxString& aResponse );

    /**
     * Get the current editor context (schematic, board, footprint).
     */
    wxString GetContext() const;

    /**
     * Set file operations (for dependency injection in tests).
     */
    void SetFileOperations( std::unique_ptr<I_FILE_OPERATIONS> aFileOps );

    /**
     * Set AI service (for dependency injection in tests).
     */
    void SetAIService( std::unique_ptr<I_AI_SERVICE> aAIService );

    /**
     * Get AI service (for streaming support).
     */
    I_AI_SERVICE* GetAIService() const { return m_aiService.get(); }
    
    /**
     * Gather context (made public for streaming UI).
     */
    AI_CONTEXT gatherContext() const;

    // Public for testing
    bool parseAddComponent( const wxString& aCommand, wxString& aComponentName, VECTOR2I& aPosition );
    bool parseModifyComponent( const wxString& aCommand, wxString& aRefDes );
    bool parseAddTrace( const wxString& aCommand, VECTOR2I& aStart, VECTOR2I& aEnd, int& aWidth );
    bool parseModifyFootprint( const wxString& aCommand, wxString& aFootprintName );
    bool parseConnectCommand( const wxString& aCommand, wxString& aRef1, wxString& aPin1,
                             wxString& aRef2, wxString& aPin2 );

private:
    AI_COMMAND_RESULT processDirectCommand( const wxString& aCommand );
    AI_COMMAND_RESULT processSchematicCommand( const wxString& aCommand );
    AI_COMMAND_RESULT processBoardCommand( const wxString& aCommand );
    AI_COMMAND_RESULT processFootprintCommand( const wxString& aCommand );
    AI_COMMAND_RESULT processGenericCommand( const wxString& aCommand );



    /**
     * Gather schematic-specific context (components, nets, sheets).
     */
    void gatherSchematicContext( AI_CONTEXT& aContext ) const;

    /**
     * Gather board-specific context (footprints, tracks, nets).
     */
    void gatherBoardContext( AI_CONTEXT& aContext ) const;

    /**
     * Gather available symbol libraries and symbols.
     */
    void gatherSymbolLibraries( AI_CONTEXT& aContext ) const;

    /**
     * Gather available footprint libraries and footprints.
     */
    void gatherFootprintLibraries( AI_CONTEXT& aContext ) const;

    /**
     * Actually place a component symbol in the schematic.
     * @param aLibId Library ID (e.g., "Device:R")
     * @param aPosition Position to place the symbol
     * @return true if successful
     */
    bool executePlaceComponent( const LIB_ID& aLibId, const VECTOR2I& aPosition );

    /**
     * Actually draw a wire between two points.
     * @param aStart Start point
     * @param aEnd End point
     * @return true if successful
     */
    bool executeDrawWire( const VECTOR2I& aStart, const VECTOR2I& aEnd );

    /**
     * Connect two component pins by reference designator and pin name.
     * @param aRef1 First component reference (e.g., "U1")
     * @param aPin1 First pin name (e.g., "VIN" or "1")
     * @param aRef2 Second component reference (e.g., "C1")
     * @param aPin2 Second pin name (e.g., "1" or "2")
     * @return true if successful
     */
    bool executeConnectComponents( const wxString& aRef1, const wxString& aPin1,
                                   const wxString& aRef2, const wxString& aPin2 );

    /**
     * Find a symbol by library ID (handles library:name format).
     * @param aSymbolName Symbol name (e.g., "Device:R" or "R")
     * @param aLibId Output library ID
     * @return true if found
     */
    bool findSymbolByName( const wxString& aSymbolName, LIB_ID& aLibId );

    EDA_BASE_FRAME* m_frame;
    std::unique_ptr<I_FILE_OPERATIONS> m_fileOps;
    std::unique_ptr<I_AI_SERVICE> m_aiService;
};

#endif // AI_COMMAND_PROCESSOR_H
