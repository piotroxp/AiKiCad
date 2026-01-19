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

#include "ai_service.h"
// kicad_curl_easy.h **must be** included before any wxWidgets header to avoid conflicts
#include <curl/curl.h>
#include <kicad_curl/kicad_curl_easy.h>
#include <nlohmann/json.hpp>
#include <json_common.h>
#include <wx/log.h>
#include <wx/tokenzr.h>
#include <wx/intl.h>  // For _() translation macro
#include <sstream>

OLLAMA_AI_SERVICE::OLLAMA_AI_SERVICE( const wxString& aBaseUrl ) :
        m_baseUrl( aBaseUrl ),
        m_model( wxT( "qwen2.5-coder:32b" ) ),  // Default to code-focused model
        m_availabilityChecked( false ),
        m_isAvailable( false )
{
    // Test connection on construction
    if( TestConnection() )
    {
        // Auto-select first available model if default not found
        std::vector<wxString> models = GetAvailableModels();
        if( !models.empty() )
        {
            // Prefer code-focused models
            bool foundCodeModel = false;
            for( const wxString& model : models )
            {
                if( model.Contains( wxT( "coder" ) ) || model.Contains( wxT( "code" ) ) )
                {
                    m_model = model;
                    foundCodeModel = true;
                    break;
                }
            }
            if( !foundCodeModel )
            {
                m_model = models[0]; // Use first available
            }
        }
    }
    m_isAvailable = TestConnection();
    m_availabilityChecked = true;
}


OLLAMA_AI_SERVICE::~OLLAMA_AI_SERVICE()
{
}


wxString OLLAMA_AI_SERVICE::buildSystemPrompt( const AI_CONTEXT& aContext ) const
{
    wxString prompt = wxT( "You are an AI assistant for KiCad EDA software. " );
    prompt += wxT( "You help users with electronic design tasks including schematic design, PCB layout, and footprint creation. " );
    prompt += wxString::Format( wxT( "Current context: %s editor. " ), aContext.editorType );

    if( !aContext.fileName.IsEmpty() )
        prompt += wxString::Format( wxT( "Working on file: %s. " ), aContext.fileName );

    if( !aContext.projectPath.IsEmpty() )
        prompt += wxString::Format( wxT( "Project path: %s. " ), aContext.projectPath );

    // Add component information
    if( !aContext.availableComponents.empty() )
    {
        prompt += wxT( "\n\nAvailable components in current design and libraries:\n" );
        size_t maxComponents = std::min( aContext.availableComponents.size(), size_t( 100 ) );
        for( size_t i = 0; i < maxComponents; ++i )
        {
            prompt += wxString::Format( wxT( "  - %s\n" ), aContext.availableComponents[i] );
        }
        if( aContext.availableComponents.size() > maxComponents )
        {
            prompt += wxString::Format( wxT( "  ... and %zu more components\n" ),
                                       aContext.availableComponents.size() - maxComponents );
        }
    }

    // Add footprint information
    if( !aContext.availableFootprints.empty() )
    {
        prompt += wxT( "\n\nAvailable footprints in current design and libraries:\n" );
        size_t maxFootprints = std::min( aContext.availableFootprints.size(), size_t( 100 ) );
        for( size_t i = 0; i < maxFootprints; ++i )
        {
            prompt += wxString::Format( wxT( "  - %s\n" ), aContext.availableFootprints[i] );
        }
        if( aContext.availableFootprints.size() > maxFootprints )
        {
            prompt += wxString::Format( wxT( "  ... and %zu more footprints\n" ),
                                       aContext.availableFootprints.size() - maxFootprints );
        }
    }

    prompt += wxT( "\nYou can help users:\n" );
    prompt += wxT( "- Add, modify, or remove components and footprints\n" );
    prompt += wxT( "- Query available libraries and parts\n" );
    prompt += wxT( "- Design circuits and PCB layouts\n" );
    prompt += wxT( "- Answer questions about KiCad functionality\n" );
    prompt += wxT( "- Provide design recommendations\n" );
    prompt += wxT( "\nIMPORTANT: When asked to create or build circuits, provide EXECUTABLE COMMANDS that can be automatically executed.\n" );
    prompt += wxT( "Break down complex requests into step-by-step commands using this format:\n" );
    prompt += wxT( "1. add component <name> at <x>,<y>\n" );
    prompt += wxT( "2. connect <ref1>.<pin1> to <ref2>.<pin2>\n" );
    prompt += wxT( "3. add component <name> at <x>,<y>\n" );
    prompt += wxT( "\nExample for 'create a 5V voltage regulator':\n" );
    prompt += wxT( "1. add component Device:LM7805 at 100000,100000\n" );
    prompt += wxT( "2. add component Device:C at 50000,100000\n" );
    prompt += wxT( "3. add component Device:C at 150000,100000\n" );
    prompt += wxT( "4. connect U1.VIN to C1.1\n" );
    prompt += wxT( "5. connect U1.VOUT to C2.1\n" );
    prompt += wxT( "6. connect U1.GND to C1.2\n" );
    prompt += wxT( "7. connect U1.GND to C2.2\n" );
    prompt += wxT( "\nUse the available components and footprints listed above when making recommendations." );

    return prompt;
}


AI_RESPONSE OLLAMA_AI_SERVICE::ProcessPrompt( const wxString& aPrompt,
                                             const AI_CONTEXT& aContext )
{
    if( !IsAvailable() )
    {
        return { false, wxEmptyString, _( "Ollama service is not available. Please ensure Ollama is running." ) };
    }

    wxString systemPrompt = buildSystemPrompt( aContext );

    // Build JSON payload for Ollama API
    nlohmann::json payload;
    payload["model"] = m_model.ToStdString();
    payload["prompt"] = aPrompt.ToStdString();
    payload["system"] = systemPrompt.ToStdString();
    payload["stream"] = false;

    return makeApiRequest( wxT( "/api/generate" ), wxString::FromUTF8( payload.dump() ) );
}


AI_RESPONSE OLLAMA_AI_SERVICE::ProcessPromptStreaming( const wxString& aPrompt,
                                                      const AI_CONTEXT& aContext,
                                                      std::function<void( const wxString& )> aCallback )
{
    if( !IsAvailable() )
    {
        return { false, wxEmptyString, _( "Ollama service is not available." ) };
    }

    wxString systemPrompt = buildSystemPrompt( aContext );

    nlohmann::json payload;
    payload["model"] = m_model.ToStdString();
    payload["prompt"] = aPrompt.ToStdString();
    payload["system"] = systemPrompt.ToStdString();
    payload["stream"] = true;

    return makeApiRequest( wxT( "/api/generate" ), wxString::FromUTF8( payload.dump() ),
                          true, aCallback );
}


AI_RESPONSE OLLAMA_AI_SERVICE::makeApiRequest( const wxString& aEndpoint,
                                               const wxString& aJsonPayload,
                                               bool aStream,
                                               std::function<void( const wxString& )> aStreamCallback ) const
{
    AI_RESPONSE response;

    wxString urlStr = m_baseUrl + aEndpoint;

    // Use KiCad's curl wrapper for HTTP requests
    std::ostringstream responseStream;
    KICAD_CURL_EASY curl;
    curl.SetHeader( "Content-Type", "application/json" );
    curl.SetURL( urlStr.ToUTF8().data() );
    curl.SetPostFields( aJsonPayload.ToUTF8().data() );
    // Note: SetOutputStream signature takes const but implementation casts to non-const for writing
    curl.SetOutputStream( &responseStream );

    int curlCode = curl.Perform();

    if( curlCode != CURLE_OK )
    {
        response.error = wxString::Format( _( "Failed to connect to Ollama: %s" ),
                                          wxString::FromUTF8( curl.GetErrorText( curlCode ) ) );
        return response;
    }

    std::string responseStr = responseStream.str();
    wxString wxResponse = wxString::FromUTF8( responseStr );

    // For streaming, parse line by line (Ollama sends newline-delimited JSON)
    if( aStream && aStreamCallback )
    {
        wxStringTokenizer tokenizer( wxResponse, wxT( "\n" ) );
        wxString completeMessage;

        while( tokenizer.HasMoreTokens() )
        {
            wxString line = tokenizer.GetNextToken().Trim();
            if( line.IsEmpty() )
                continue;

            try
            {
                nlohmann::json jsonLine = nlohmann::json::parse( line.ToUTF8().data() );
                if( jsonLine.contains( "response" ) )
                {
                    wxString chunk = wxString::FromUTF8( jsonLine["response"].get<std::string>() );
                    completeMessage += chunk;
                    aStreamCallback( chunk );
                }
                if( jsonLine.contains( "done" ) && jsonLine["done"].get<bool>() )
                    break;
            }
            catch( ... )
            {
                // Skip invalid JSON lines
                continue;
            }
        }

        response.success = true;
        response.message = completeMessage;
        response.isComplete = true;
    }
    else
    {
        // Non-streaming: parse full response
        try
        {
            nlohmann::json jsonResponse = nlohmann::json::parse( responseStr );
            if( jsonResponse.contains( "response" ) )
            {
                response.success = true;
                response.message = wxString::FromUTF8( jsonResponse["response"].get<std::string>() );
                response.isComplete = true;
            }
            else if( jsonResponse.contains( "error" ) )
            {
                response.error = wxString::FromUTF8( jsonResponse["error"].get<std::string>() );
            }
            else
            {
                response.error = _( "Unexpected response format from Ollama" );
            }
        }
        catch( const std::exception& e )
        {
            response.error = wxString::Format( _( "Failed to parse Ollama response: %s" ),
                                              wxString::FromUTF8( e.what() ) );
        }
        catch( ... )
        {
            response.error = _( "Failed to parse Ollama response" );
        }
    }

    return response;
}


bool OLLAMA_AI_SERVICE::IsAvailable() const
{
    if( m_availabilityChecked )
        return m_isAvailable;

    m_isAvailable = TestConnection();
    m_availabilityChecked = true;
    return m_isAvailable;
}


bool OLLAMA_AI_SERVICE::TestConnection() const
{
    // Try to list models as a connection test (GET request)
    wxString urlStr = m_baseUrl + wxT( "/api/tags" );

    std::ostringstream responseStream;
    KICAD_CURL_EASY curl;
    curl.SetURL( urlStr.ToUTF8().data() );
    // Note: SetOutputStream signature takes const but implementation casts to non-const for writing
    curl.SetOutputStream( &responseStream );

    int curlCode = curl.Perform();
    return curlCode == CURLE_OK;
}


std::vector<wxString> OLLAMA_AI_SERVICE::GetAvailableModels() const
{
    std::vector<wxString> models;
    
    // Use GET request for /api/tags
    wxString urlStr = m_baseUrl + wxT( "/api/tags" );

    std::ostringstream responseStream;
    KICAD_CURL_EASY curl;
    curl.SetURL( urlStr.ToUTF8().data() );
    // Note: SetOutputStream signature takes const but implementation casts to non-const for writing
    curl.SetOutputStream( &responseStream );

    int curlCode = curl.Perform();
    if( curlCode != CURLE_OK )
        return models;

    std::string responseStr = responseStream.str();

    try
    {
        nlohmann::json jsonResponse = nlohmann::json::parse( responseStr );
        if( jsonResponse.contains( "models" ) && jsonResponse["models"].is_array() )
        {
            for( const auto& model : jsonResponse["models"] )
            {
                if( model.contains( "name" ) )
                {
                    models.push_back( wxString::FromUTF8( model["name"].get<std::string>() ) );
                }
            }
        }
    }
    catch( ... )
    {
        // Return empty list on parse error
    }

    return models;
}


void OLLAMA_AI_SERVICE::SetModel( const wxString& aModelName )
{
    m_model = aModelName;
}


wxString OLLAMA_AI_SERVICE::GetCurrentModel() const
{
    return m_model;
}


void OLLAMA_AI_SERVICE::SetBaseUrl( const wxString& aBaseUrl )
{
    m_baseUrl = aBaseUrl;
    m_availabilityChecked = false;
    m_isAvailable = false;
}
