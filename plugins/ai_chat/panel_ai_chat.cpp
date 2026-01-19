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

#include "panel_ai_chat.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/config.h>
#include <eda_base_frame.h>
#include <kiplatform/ui.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>

PANEL_AI_CHAT::PANEL_AI_CHAT( EDA_BASE_FRAME* aParent ) :
        wxPanel( aParent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL ),
        m_parentFrame( aParent ),
        m_nextMessageId( 1 )
{
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    // Chat history area - use rich text for formatting support
    m_chatHistory = new wxRichTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                        wxDefaultSize, wxRE_MULTILINE | wxRE_READONLY );
    m_chatHistory->SetMinSize( wxSize( -1, 200 ) );
    mainSizer->Add( m_chatHistory, 1, wxEXPAND | wxALL, 5 );

    // Progress area (initially hidden)
    wxBoxSizer* progressSizer = new wxBoxSizer( wxHORIZONTAL );
    m_progressBar = new wxGauge( this, wxID_ANY, 100 );
    m_progressBar->SetMinSize( wxSize( 100, -1 ) );
    m_progressBar->Hide();
    progressSizer->Add( m_progressBar, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    m_statusText = new wxStaticText( this, wxID_ANY, _( "Ready" ) );
    m_statusText->Hide();
    progressSizer->Add( m_statusText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    m_cancelButton = new wxButton( this, wxID_ANY, _( "Cancel" ) );
    m_cancelButton->Hide();
    progressSizer->Add( m_cancelButton, 0, wxALL, 5 );
    
    mainSizer->Add( progressSizer, 0, wxEXPAND, 5 );

    // Input area
    wxBoxSizer* inputSizer = new wxBoxSizer( wxHORIZONTAL );
    m_inputField = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxDefaultSize, wxTE_PROCESS_ENTER );
    inputSizer->Add( m_inputField, 1, wxEXPAND | wxALL, 5 );

    m_sendButton = new wxButton( this, wxID_ANY, _( "Send" ) );
    m_sendButton->Enable( false );
    inputSizer->Add( m_sendButton, 0, wxALL, 5 );

    m_clearButton = new wxButton( this, wxID_ANY, _( "Clear" ) );
    inputSizer->Add( m_clearButton, 0, wxALL, 5 );

    m_settingsButton = new wxButton( this, wxID_ANY, _( "Settings" ) );
    inputSizer->Add( m_settingsButton, 0, wxALL, 5 );

    mainSizer->Add( inputSizer, 0, wxEXPAND, 5 );

    SetSizer( mainSizer );
    Layout();

    // Create command processor
    m_commandProcessor = std::make_unique<AI_COMMAND_PROCESSOR>( m_parentFrame );

    // Create typing timer
    m_typingTimer = new wxTimer( this );

    // Bind events
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &PANEL_AI_CHAT::onSendButton, this, m_sendButton->GetId() );
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &PANEL_AI_CHAT::onClearButton, this, m_clearButton->GetId() );
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &PANEL_AI_CHAT::onCancelButton, this, m_cancelButton->GetId() );
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &PANEL_AI_CHAT::onSettingsButton, this, m_settingsButton->GetId() );
    Bind( wxEVT_TEXT_ENTER, &PANEL_AI_CHAT::onInputEnter, this, m_inputField->GetId() );
    Bind( wxEVT_TEXT, &PANEL_AI_CHAT::onInputText, this, m_inputField->GetId() );
    Bind( wxEVT_TIMER, &PANEL_AI_CHAT::onTimer, this, m_typingTimer->GetId() );
    Bind( wxEVT_CONTEXT_MENU, &PANEL_AI_CHAT::onContextMenu, this );
    Bind( wxEVT_KEY_DOWN, &PANEL_AI_CHAT::onKeyDown, this );
    
    // Global key event hook for Ctrl+F (find) and Ctrl+L (clear)
    Bind( wxEVT_CHAR_HOOK, &PANEL_AI_CHAT::onCharHook, this );

    // Add welcome message
    AddMessage( _( "AI Chat Assistant ready. Type a command to get started." ), false );
}


PANEL_AI_CHAT::~PANEL_AI_CHAT()
{
    if( m_typingTimer )
    {
        m_typingTimer->Stop();
        delete m_typingTimer;
    }
}


void PANEL_AI_CHAT::AddMessage( const wxString& aMessage, bool aIsUser )
{
    if( aMessage.IsEmpty() )
        return;

    wxString prefix = aIsUser ? _( "You: " ) : _( "AI: " );
    
    // Add timestamp
    wxDateTime now = wxDateTime::Now();
    wxString timestamp = now.Format( wxT( "[%H:%M:%S] " ) );
    
    wxRichTextAttr attr;
    if( aIsUser )
    {
        attr.SetTextColour( wxColour( 0, 100, 200 ) );  // Blue for user
        attr.SetFontWeight( wxFONTWEIGHT_BOLD );
    }
    else
    {
        attr.SetTextColour( wxColour( 50, 50, 50 ) );    // Dark gray for AI
        attr.SetFontWeight( wxFONTWEIGHT_NORMAL );
    }
    
    m_chatHistory->WriteText( timestamp );
    m_chatHistory->SetDefaultStyle( attr );
    m_chatHistory->WriteText( prefix );
    m_chatHistory->WriteText( aMessage + wxT( "\n" ) );
    
    // Reset to default style
    wxRichTextAttr defaultAttr;
    m_chatHistory->SetDefaultStyle( defaultAttr );
    
    // Add to history
    ChatMessage msg;
    msg.timestamp = now;
    msg.isUser = aIsUser;
    msg.content = aMessage;
    m_history.push_back( msg );
    
    scrollToBottom();
    
    // Auto-save if enabled
    AutoSaveHistory();
}


void PANEL_AI_CHAT::ClearHistory()
{
    m_chatHistory->Clear();
    AddMessage( _( "Chat history cleared." ), false );
}


wxString PANEL_AI_CHAT::AddStreamingMessage()
{
    std::lock_guard<std::mutex> lock( m_processingMutex );
    
    wxString messageId = wxString::Format( wxT( "msg_%d" ), m_nextMessageId++ );
    
    // Add timestamp and AI prefix
    wxDateTime now = wxDateTime::Now();
    wxString timestamp = now.Format( wxT( "[%H:%M:%S] " ) );
    
    wxRichTextAttr attr;
    attr.SetTextColour( wxColour( 50, 50, 50 ) );  // Dark gray for AI
    attr.SetFontWeight( wxFONTWEIGHT_NORMAL );
    
    m_chatHistory->WriteText( timestamp );
    m_chatHistory->SetDefaultStyle( attr );
    m_chatHistory->WriteText( _( "AI: " ) );
    long endPos = m_chatHistory->GetLastPosition();
    
    // Store the position for later updates
    m_streamingMessages[messageId] = endPos;
    
    // Reset to default style
    wxRichTextAttr defaultAttr;
    m_chatHistory->SetDefaultStyle( defaultAttr );
    
    scrollToBottom();
    return messageId;
}


void PANEL_AI_CHAT::UpdateStreamingMessage( const wxString& aMessageId, const wxString& aContent )
{
    std::lock_guard<std::mutex> lock( m_processingMutex );
    
    auto it = m_streamingMessages.find( aMessageId );
    if( it == m_streamingMessages.end() )
        return;
    
    // Remove the old content and append new content
    long pos = it->second;
    
    // Move cursor to the end of streaming message
    m_chatHistory->SetInsertionPoint( pos );
    m_chatHistory->WriteText( aContent );
    m_streamingMessages[aMessageId] = m_chatHistory->GetLastPosition();
    
    scrollToBottom();
}


void PANEL_AI_CHAT::FinalizeStreamingMessage( const wxString& aMessageId )
{
    std::lock_guard<std::mutex> lock( m_processingMutex );
    
    auto it = m_streamingMessages.find( aMessageId );
    if( it == m_streamingMessages.end() )
        return;
    
    // Add newline and remove from streaming messages
    m_chatHistory->WriteText( wxT( "\n" ) );
    m_streamingMessages.erase( it );
    
    scrollToBottom();
}


void PANEL_AI_CHAT::SetCommandProcessor( std::unique_ptr<AI_COMMAND_PROCESSOR> aProcessor )
{
    m_commandProcessor = std::move( aProcessor );
}


void PANEL_AI_CHAT::onSendButton( wxCommandEvent& aEvent )
{
    wxString command = m_inputField->GetValue();
    if( command.Trim().IsEmpty() || m_isProcessing.load() )
        return;

    std::lock_guard<std::mutex> lock( m_processingMutex );
    
    // Add to command history
    addToCommandHistory( command );
    
    m_inputField->Clear();
    m_isProcessing.store( true );
    m_cancelRequested.store( false );
    updateSendButtonState();

    AddMessage( command, true );
    
    // Show progress indicators
    m_progressBar->Show();
    m_progressBar->SetValue( 0 );
    m_statusText->Show();
    m_statusText->SetLabel( _( "Processing..." ) );
    m_cancelButton->Show();
    m_cancelButton->Enable( true );
    Layout();
    
    // Start processing in background thread
    CallAfter( [this, command]() { processCommand( command ); } );
}


void PANEL_AI_CHAT::onClearButton( wxCommandEvent& aEvent )
{
    ClearHistory();
}


void PANEL_AI_CHAT::onCancelButton( wxCommandEvent& aEvent )
{
    std::lock_guard<std::mutex> lock( m_processingMutex );
    m_cancelRequested.store( true );
    
    // Update UI to show cancellation
    m_statusText->SetLabel( _( "Cancelling..." ) );
    m_cancelButton->Enable( false );
}


void PANEL_AI_CHAT::onSettingsButton( wxCommandEvent& aEvent )
{
    if( !m_settingsDialog )
    {
        m_settingsDialog = std::make_unique<DIALOG_AI_CHAT_SETTINGS>( this );
        
        // Update available models from AI service
        if( m_commandProcessor )
        {
            I_AI_SERVICE* aiService = m_commandProcessor->GetAIService();
            if( aiService && aiService->IsAvailable() )
            {
                m_settingsDialog->UpdateAvailableModels( aiService->GetAvailableModels() );
            }
        }
    }
    
    if( m_settingsDialog->ShowModal() == wxID_OK )
    {
        // Apply settings to AI service if needed
        // This would involve updating the AI service with new URL, model, etc.
        // For now, just show a message that settings were saved
        AddMessage( _( "Settings saved successfully." ), false );
    }
}


void PANEL_AI_CHAT::onInputEnter( wxCommandEvent& aEvent )
{
    onSendButton( aEvent );
}


void PANEL_AI_CHAT::onInputText( wxCommandEvent& aEvent )
{
    m_typingTimer->Stop();
    m_typingTimer->StartOnce( TYPING_DELAY_MS );
    updateSendButtonState();
}


void PANEL_AI_CHAT::onTimer( wxTimerEvent& aEvent )
{
    updateSendButtonState();
}


void PANEL_AI_CHAT::processCommand( const wxString& aCommand )
{
    if( !m_commandProcessor )
    {
        std::lock_guard<std::mutex> lock( m_processingMutex );
        AddMessage( _( "Error: Command processor not initialized." ), false );
        hideProgressIndicators();
        m_isProcessing.store( false );
        updateSendButtonState();
        return;
    }

    // Get AI service for streaming
    I_AI_SERVICE* aiService = m_commandProcessor->GetAIService();
    
    if( aiService && aiService->IsAvailable() )
    {
        // Use streaming if available
        AI_CONTEXT context = m_commandProcessor->gatherContext();
        
        // Create streaming message placeholder
        wxString messageId = AddStreamingMessage();
        
        // Update progress
        CallAfter( [this]() { 
            if( !m_cancelRequested.load() )
            {
                m_progressBar->SetValue( 30 );
                m_statusText->SetLabel( _( "Generating response..." ) );
            }
        } );
        
        // Process with streaming
        AI_RESPONSE response = aiService->ProcessPromptStreaming(
            aCommand, context,
            [this, messageId]( const wxString& chunk )
            {
                // Check for cancellation
                if( m_cancelRequested.load() )
                    return;
                
                // Update streaming message in UI thread
                CallAfter( [this, messageId, chunk]()
                {
                    if( !m_cancelRequested.load() )
                    {
                        UpdateStreamingMessage( messageId, chunk );
                        // Pulse progress bar during streaming
                        m_progressBar->Pulse();
                    }
                } );
            } );
        
        // Finalize UI
        CallAfter( [this, messageId, response, aCommand]()
        {
            if( m_cancelRequested.load() )
            {
                // Remove incomplete streaming message
                std::lock_guard<std::mutex> lock( m_processingMutex );
                m_streamingMessages.erase( messageId );
                AddMessage( _( "Request cancelled." ), false );
            }
            else
            {
                FinalizeStreamingMessage( messageId );
                
                if( response.success )
                {
                    // Extract and execute commands from the AI response
                    AI_COMMAND_RESULT result = m_commandProcessor->ProcessCommandsFromResponse( response.message );
                    if( result.success )
                    {
                        // Commands were executed, show the result message
                        if( !result.message.IsEmpty() )
                        {
                            AddMessage( result.message, false );
                        }
                    }
                    else if( !result.error.IsEmpty() && result.error != _( "No commands found in response" ) )
                    {
                        // Show error if command execution failed (but not if just no commands found)
                        AddMessage( _( "Error executing commands: " ) + result.error, false );
                    }
                    // If no commands were found, the AI response was already shown via streaming
                }
                else
                {
                    wxString errorMsg = response.error.IsEmpty() ? response.message : response.error;
                    AddMessage( _( "Error: " ) + errorMsg, false );
                }
            }
            
            hideProgressIndicators();
            m_isProcessing.store( false );
            updateSendButtonState();
        } );
    }
    else
    {
        // Fallback to non-streaming processing
        CallAfter( [this, aCommand]()
        {
            AI_COMMAND_RESULT result = m_commandProcessor->ProcessCommand( aCommand );

            if( result.success )
            {
                AddMessage( result.message, false );
            }
            else
            {
                wxString errorMsg = result.error.IsEmpty() ? result.message : result.error;
                AddMessage( _( "Error: " ) + errorMsg, false );
            }

            hideProgressIndicators();
            m_isProcessing.store( false );
            updateSendButtonState();
        } );
    }
}


void PANEL_AI_CHAT::updateSendButtonState()
{
    wxString text = m_inputField->GetValue().Trim();
    m_sendButton->Enable( !text.IsEmpty() && !m_isProcessing );
}


void PANEL_AI_CHAT::scrollToBottom()
{
    long pos = m_chatHistory->GetLastPosition();
    m_chatHistory->ShowPosition( pos );
}


void PANEL_AI_CHAT::hideProgressIndicators()
{
    std::lock_guard<std::mutex> lock( m_processingMutex );
    m_progressBar->Hide();
    m_statusText->Hide();
    m_cancelButton->Hide();
    m_statusText->SetLabel( _( "Ready" ) );
    Layout();
}


void PANEL_AI_CHAT::onContextMenu( wxContextMenuEvent& aEvent )
{
    wxMenu menu;
    menu.Append( wxID_SETUP, _( "Settings..." ) );
    menu.Append( wxID_CLEAR, _( "Clear History" ) );
    menu.AppendSeparator();
    menu.Append( wxID_SAVE, _( "Save History..." ) );
    menu.Append( wxID_OPEN, _( "Load History..." ) );
    menu.AppendSeparator();
    menu.Append( wxID_ABOUT, _( "About AI Chat" ) );
    
    menu.Bind( wxEVT_MENU, [this]( wxCommandEvent& event )
    {
        switch( event.GetId() )
        {
            case wxID_SETUP:
                onSettingsButton( event );
                break;
            case wxID_CLEAR:
                ClearHistory();
                break;
            case wxID_SAVE:
            {
                wxFileDialog saveDlg( this, _( "Save Conversation History" ), wxEmptyString,
                                   wxT( "ai_chat_history.json" ), 
                                   wxT( "JSON files (*.json)|*.json" ),
                                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT );
                if( saveDlg.ShowModal() == wxID_OK )
                {
                    SaveHistory( saveDlg.GetPath() );
                }
                break;
            }
            case wxID_OPEN:
            {
                wxFileDialog openDlg( this, _( "Load Conversation History" ), wxEmptyString,
                                   wxT( "ai_chat_history.json" ), 
                                   wxT( "JSON files (*.json)|*.json" ),
                                   wxFD_OPEN | wxFD_FILE_MUST_EXIST );
                if( openDlg.ShowModal() == wxID_OK )
                {
                    LoadHistory( openDlg.GetPath() );
                }
                break;
            }
            case wxID_ABOUT:
                wxMessageBox( _( "AI Chat Assistant for KiCad\n\nProvides natural language commands for electronic design automation." ),
                           _( "About AI Chat" ), wxOK | wxICON_INFORMATION, this );
                break;
        }
    } );
    
    PopupMenu( &menu );
}


bool PANEL_AI_CHAT::SaveHistory( const wxString& aFilePath )
{
    try
    {
        nlohmann::json root = nlohmann::json::array();
        
        for( const ChatMessage& msg : m_history )
        {
            nlohmann::json jmsg;
            jmsg["timestamp"] = msg.timestamp.FormatISOCombined();
            jmsg["is_user"] = msg.isUser;
            jmsg["content"] = msg.content.ToUTF8().data();
            root.push_back( jmsg );
        }
        
        std::ofstream file( aFilePath.ToUTF8().data() );
        if( !file.is_open() )
        {
            wxMessageBox( _( "Failed to open file for writing: " ) + aFilePath, _( "Error" ), wxOK | wxICON_ERROR, this );
            return false;
        }
        
        file << root.dump( 2 );
        file.close();
        
        AddMessage( wxString::Format( _( "History saved to: %s" ), aFilePath ), false );
        return true;
    }
    catch( const std::exception& e )
    {
        wxMessageBox( _( "Failed to save history: " ) + wxString( e.what(), wxConvUTF8 ), 
                     _( "Error" ), wxOK | wxICON_ERROR, this );
        return false;
    }
}


bool PANEL_AI_CHAT::LoadHistory( const wxString& aFilePath )
{
    try
    {
        std::ifstream file( aFilePath.ToUTF8().data() );
        if( !file.is_open() )
        {
            wxMessageBox( _( "Failed to open file for reading: " ) + aFilePath, _( "Error" ), wxOK | wxICON_ERROR, this );
            return false;
        }
        
        std::string content( (std::istreambuf_iterator<char>( file )), std::istreambuf_iterator<char>() );
        file.close();
        
        nlohmann::json root = nlohmann::json::parse( content );
        
        if( !root.is_array() )
        {
            wxMessageBox( _( "Invalid history file format" ), _( "Error" ), wxOK | wxICON_ERROR, this );
            return false;
        }
        
        // Clear current history
        m_history.clear();
        m_chatHistory->Clear();
        
        // Load messages
        for( const auto& jmsg : root )
        {
            if( jmsg.contains( "timestamp" ) && jmsg.contains( "is_user" ) && jmsg.contains( "content" ) )
            {
                ChatMessage msg;
                std::string timestampStr = jmsg["timestamp"].get<std::string>();
                std::string contentStr = jmsg["content"].get<std::string>();
                msg.timestamp.ParseFormat( wxString( timestampStr ), wxT( "%Y-%m-%dT%H:%M:%S" ) );
                msg.isUser = jmsg["is_user"].get<bool>();
                msg.content = wxString( contentStr );
                m_history.push_back( msg );
                
                // Add to display
                AddMessage( msg.content, msg.isUser );
            }
        }
        
        AddMessage( wxString::Format( _( "Loaded %zu messages from: %s" ), m_history.size(), aFilePath ), false );
        return true;
    }
    catch( const std::exception& e )
    {
        wxMessageBox( _( "Failed to load history: " ) + wxString( e.what(), wxConvUTF8 ), 
                     _( "Error" ), wxOK | wxICON_ERROR, this );
        return false;
    }
}


void PANEL_AI_CHAT::AutoSaveHistory()
{
    wxConfig config( wxT( "KiCad" ), wxT( "KiCad" ) );
    
    if( config.ReadBool( wxT( "AI_Chat/SaveHistory" ), true ) )
    {
        wxString historyPath = config.Read( wxT( "AI_Chat/HistoryPath" ), 
                                       wxFileName::GetHomeDir() + wxFileName::GetPathSeparator() + wxT( ".kicad" ) + wxFileName::GetPathSeparator() + wxT( "ai_chat_history.json" ) );
        
        if( !m_history.empty() )
        {
            SaveHistory( historyPath );
        }
    }
}


wxString PANEL_AI_CHAT::parseMarkdown( const wxString& aText )
{
    wxString result = aText;
    
    // Simple markdown parsing for basic formatting
    // Handle code blocks ```
    size_t codeBlockStart = result.find( wxT( "```" ) );
    while( codeBlockStart != wxString::npos )
    {
        size_t codeBlockEnd = result.find( wxT( "```" ), codeBlockStart + 3 );
        if( codeBlockEnd != wxString::npos )
        {
            // Extract language if specified
            wxString lang, code;
            wxString header = result.substr( codeBlockStart, codeBlockEnd - codeBlockStart );
            size_t newlinePos = header.find( wxT( '\n' ) );
            if( newlinePos != wxString::npos )
            {
                lang = header.substr( 3, newlinePos - 3 ).Trim();
                code = result.substr( codeBlockStart + newlinePos + 1, codeBlockEnd - codeBlockStart - newlinePos - 1 );
            }
            else
            {
                code = result.substr( codeBlockStart + 3, codeBlockEnd - codeBlockStart - 3 );
            }
            
            // Replace with placeholder for now
            result.replace( codeBlockStart, codeBlockEnd + 3 - codeBlockStart, 
                         wxT( "[CODE BLOCK: " ) + lang + wxT( "]" ) );
        }
        
        codeBlockStart = result.find( wxT( "```" ), codeBlockStart + 1 );
    }
    
    // Handle inline code `
    size_t inlineStart = result.find( wxT( '`' ) );
    while( inlineStart != wxString::npos )
    {
        size_t inlineEnd = result.find( wxT( '`' ), inlineStart + 1 );
        if( inlineEnd != wxString::npos )
        {
            wxString code = result.substr( inlineStart + 1, inlineEnd - inlineStart - 1 );
            result.replace( inlineStart, inlineEnd + 1 - inlineStart, 
                         wxT( "[INLINE: " ) + code + wxT( "]" ) );
        }
        
        inlineStart = result.find( wxT( '`' ), inlineStart + 1 );
    }
    
    // Handle bold **text**
    size_t boldStart = result.find( wxT( "**" ) );
    while( boldStart != wxString::npos )
    {
        size_t boldEnd = result.find( wxT( "**" ), boldStart + 2 );
        if( boldEnd != wxString::npos )
        {
            wxString text = result.substr( boldStart + 2, boldEnd - boldStart - 2 );
            result.replace( boldStart, boldEnd + 2 - boldStart, 
                         wxT( "*BOLD:" ) + text + wxT( "*" ) );
        }
        
        boldStart = result.find( wxT( "**" ), boldStart + 2 );
    }
    
    // Handle italic *text*
    size_t italicStart = result.find( wxT( '*' ) );
    while( italicStart != wxString::npos )
    {
        size_t italicEnd = result.find( wxT( '*' ), italicStart + 1 );
        if( italicEnd != wxString::npos )
            {
                wxString text = result.substr( italicStart + 1, italicEnd - italicStart - 1 );
                result.replace( italicStart, italicEnd + 1 - italicStart, 
                             wxT( "_ITALIC:" ) + text + wxT( "_" ) );
            }
        
        italicStart = result.find( wxT( '*' ), italicStart + 1 );
    }
    
    return result;
}


void PANEL_AI_CHAT::addCodeBlock( const wxString& aCode, const wxString& aLanguage )
{
    // Add code block with gray background and monospace font
    wxFont codeFont( 10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    wxRichTextAttr codeAttr;
    codeAttr.SetBackgroundColour( wxColour( 245, 245, 245 ) );
    codeAttr.SetTextColour( wxColour( 0, 0, 0 ) );
    codeAttr.SetFont( codeFont );
    
    // Add language label if specified
    if( !aLanguage.IsEmpty() )
    {
        wxFont langFont( 8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
        wxRichTextAttr langAttr;
        langAttr.SetTextColour( wxColour( 100, 100, 100 ) );
        langAttr.SetFont( langFont );
        
        m_chatHistory->WriteText( wxT( " (" ) );
        m_chatHistory->SetDefaultStyle( langAttr );
        m_chatHistory->WriteText( aLanguage );
        m_chatHistory->SetDefaultStyle( wxRichTextAttr() );
        m_chatHistory->WriteText( wxT( ")\n" ) );
    }
    
    m_chatHistory->SetDefaultStyle( codeAttr );
    m_chatHistory->WriteText( aCode + wxT( "\n" ) );
    m_chatHistory->SetDefaultStyle( wxRichTextAttr() );
}


void PANEL_AI_CHAT::addInlineCode( const wxString& aCode )
{
    // Add inline code with gray background
    wxFont codeFont( 9, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    wxRichTextAttr codeAttr;
    codeAttr.SetBackgroundColour( wxColour( 235, 235, 235 ) );
    codeAttr.SetTextColour( wxColour( 0, 0, 139 ) );
    codeAttr.SetFont( codeFont );
    
    m_chatHistory->SetDefaultStyle( codeAttr );
    m_chatHistory->WriteText( aCode );
    m_chatHistory->SetDefaultStyle( wxRichTextAttr() );
}


void PANEL_AI_CHAT::onKeyDown( wxKeyEvent& aEvent )
{
    int keyCode = aEvent.GetKeyCode();
    
    // Handle up/down arrows for command history
    if( keyCode == WXK_UP && m_historyIndex > 0 )
    {
        m_historyIndex--;
        m_inputField->SetValue( m_commandHistory[m_historyIndex] );
        m_inputField->SetInsertionPointEnd();
        aEvent.Skip( false );
    }
    else if( keyCode == WXK_DOWN && !m_commandHistory.empty() && m_historyIndex < m_commandHistory.size() - 1 )
    {
        m_historyIndex++;
        m_inputField->SetValue( m_commandHistory[m_historyIndex] );
        m_inputField->SetInsertionPointEnd();
        aEvent.Skip( false );
    }
    // Handle Escape to cancel processing
    else if( keyCode == WXK_ESCAPE && m_isProcessing.load() )
    {
        wxCommandEvent* cancelEvent = new wxCommandEvent();
        onCancelButton( *cancelEvent );
        delete cancelEvent;
        aEvent.Skip( false );
    }
    else
    {
        aEvent.Skip();
    }
}


void PANEL_AI_CHAT::onCharHook( wxKeyEvent& aEvent )
{
    // Ctrl+Enter to send (Ctrl key modifier)
    if( aEvent.ControlDown() && aEvent.GetKeyCode() == WXK_RETURN )
    {
        wxString inputText = m_inputField->GetValue();
        if( !inputText.Trim().IsEmpty() && !m_isProcessing.load() )
        {
            wxCommandEvent* sendEvent = new wxCommandEvent();
            onSendButton( *sendEvent );
            delete sendEvent;
        }
        aEvent.Skip( false );
    }
    // Ctrl+L to clear history
    else if( aEvent.ControlDown() && aEvent.GetKeyCode() == 'L' )
    {
        ClearHistory();
        aEvent.Skip( false );
    }
    // Ctrl+F to find (not implemented yet, but prepare)
    else if( aEvent.ControlDown() && aEvent.GetKeyCode() == 'F' )
    {
        // TODO: Implement find functionality
        AddMessage( _( "Find functionality coming soon!" ), false );
        aEvent.Skip( false );
    }
    else
    {
        aEvent.Skip();
    }
}


void PANEL_AI_CHAT::addToCommandHistory( const wxString& aCommand )
{
    wxString trimmedCommand = aCommand;
    trimmedCommand = trimmedCommand.Trim();
    
    if( trimmedCommand.IsEmpty() )
        return;
        
    // Remove if already exists (avoid duplicates)
    auto it = std::find( m_commandHistory.begin(), m_commandHistory.end(), trimmedCommand );
    if( it != m_commandHistory.end() )
    {
        m_commandHistory.erase( it );
    }
    
    // Add to front
    m_commandHistory.insert( m_commandHistory.begin(), trimmedCommand );
    
    // Limit history size
    if( m_commandHistory.size() > MAX_COMMAND_HISTORY )
    {
        m_commandHistory.resize( MAX_COMMAND_HISTORY );
    }
    
    // Reset history index to newest
    m_historyIndex = 0;
}
