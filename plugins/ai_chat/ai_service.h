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

#ifndef AI_SERVICE_H
#define AI_SERVICE_H

#include <wx/string.h>
#include <functional>
#include <memory>
#include <vector>

/**
 * Response from AI service
 */
struct AI_RESPONSE
{
    bool success = false;
    wxString message;
    wxString error;
    bool isComplete = false;  // For streaming responses
};

/**
 * Context information about the current editor state
 */
struct AI_CONTEXT
{
    wxString editorType;      // "schematic", "board", "footprint"
    wxString fileName;
    wxString projectPath;
    std::vector<wxString> availableComponents;
    std::vector<wxString> availableFootprints;
};

/**
 * Abstract interface for AI service (mockable for tests)
 */
class I_AI_SERVICE
{
public:
    virtual ~I_AI_SERVICE() = default;

    /**
     * Send a prompt to the AI service and get a response.
     * @param aPrompt The user's prompt/command
     * @param aContext Context about the current editor state
     * @return AI response with message or error
     */
    virtual AI_RESPONSE ProcessPrompt( const wxString& aPrompt,
                                       const AI_CONTEXT& aContext ) = 0;

    /**
     * Send a prompt with streaming response callback.
     * @param aPrompt The user's prompt/command
     * @param aContext Context about the current editor state
     * @param aCallback Called for each chunk of the streaming response
     * @return Final response when streaming is complete
     */
    virtual AI_RESPONSE ProcessPromptStreaming( const wxString& aPrompt,
                                                const AI_CONTEXT& aContext,
                                                std::function<void( const wxString& )> aCallback ) = 0;

    /**
     * Check if the AI service is available/configured.
     */
    virtual bool IsAvailable() const = 0;

    /**
     * Get available models.
     */
    virtual std::vector<wxString> GetAvailableModels() const = 0;

    /**
     * Set the model to use.
     */
    virtual void SetModel( const wxString& aModelName ) = 0;

    /**
     * Get current model name.
     */
    virtual wxString GetCurrentModel() const = 0;
};

/**
 * Ollama-based AI service implementation.
 * Communicates with Ollama API (which can use llama.cpp backend).
 */
class OLLAMA_AI_SERVICE : public I_AI_SERVICE
{
public:
    OLLAMA_AI_SERVICE( const wxString& aBaseUrl = wxT( "http://localhost:11434" ) );
    ~OLLAMA_AI_SERVICE() override;

    AI_RESPONSE ProcessPrompt( const wxString& aPrompt,
                              const AI_CONTEXT& aContext ) override;

    AI_RESPONSE ProcessPromptStreaming( const wxString& aPrompt,
                                       const AI_CONTEXT& aContext,
                                       std::function<void( const wxString& )> aCallback ) override;

    bool IsAvailable() const override;
    std::vector<wxString> GetAvailableModels() const override;
    void SetModel( const wxString& aModelName ) override;
    wxString GetCurrentModel() const override;

    /**
     * Set the base URL for Ollama API.
     */
    void SetBaseUrl( const wxString& aBaseUrl );

    /**
     * Test connection to Ollama service.
     */
    bool TestConnection() const;

private:
    wxString buildSystemPrompt( const AI_CONTEXT& aContext ) const;
    AI_RESPONSE makeApiRequest( const wxString& aEndpoint, const wxString& aJsonPayload,
                               bool aStream = false,
                               std::function<void( const wxString& )> aStreamCallback = nullptr ) const;

    wxString m_baseUrl;
    wxString m_model;
    mutable bool m_availabilityChecked;
    mutable bool m_isAvailable;
};

/**
 * Mock AI service for testing
 */
class MOCK_AI_SERVICE : public I_AI_SERVICE
{
public:
    MOCK_AI_SERVICE() : m_isAvailable( true ), m_model( wxT( "mock-model" ) ) {}

    AI_RESPONSE ProcessPrompt( const wxString& aPrompt,
                              const AI_CONTEXT& aContext ) override
    {
        AI_RESPONSE response;
        response.success = true;
        response.message = wxString::Format( wxT( "Mock response to: %s" ), aPrompt );
        response.isComplete = true;
        return response;
    }

    AI_RESPONSE ProcessPromptStreaming( const wxString& aPrompt,
                                       const AI_CONTEXT& aContext,
                                       std::function<void( const wxString& )> aCallback ) override
    {
        if( aCallback )
        {
            aCallback( wxT( "Mock " ) );
            aCallback( wxT( "streaming " ) );
            aCallback( wxT( "response" ) );
        }
        return ProcessPrompt( aPrompt, aContext );
    }

    bool IsAvailable() const override { return m_isAvailable; }
    std::vector<wxString> GetAvailableModels() const override
    {
        return { wxT( "mock-model" ), wxT( "mock-model-2" ) };
    }
    void SetModel( const wxString& aModelName ) override { m_model = aModelName; }
    wxString GetCurrentModel() const override { return m_model; }

    void SetAvailable( bool aAvailable ) { m_isAvailable = aAvailable; }
    void SetResponse( const wxString& aResponse ) { m_customResponse = aResponse; }

private:
    bool m_isAvailable;
    wxString m_model;
    wxString m_customResponse;
};

#endif // AI_SERVICE_H
