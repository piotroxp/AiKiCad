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

#ifndef PANEL_AI_CHAT_H
#define PANEL_AI_CHAT_H

#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/gauge.h>
#include <wx/stattext.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/menu.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <eda_base_frame.h>
#include "ai_command_processor.h"
#include "dialog_ai_chat_settings.h"

class AI_COMMAND_PROCESSOR;

/**
 * Dockable chat panel for AI-assisted design actions.
 * Embeds into Eeschema, Pcbnew, and Footprint Editor.
 */
class PANEL_AI_CHAT : public wxPanel
{
public:
    PANEL_AI_CHAT( EDA_BASE_FRAME* aParent );
    ~PANEL_AI_CHAT() override;

    /**
     * Add a message to the chat history.
     */
    void AddMessage( const wxString& aMessage, bool aIsUser = true );

    /**
     * Add a streaming message that can be updated incrementally.
     * @return Message ID for later updates
     */
    wxString AddStreamingMessage();

    /**
     * Update a streaming message with new content.
     */
    void UpdateStreamingMessage( const wxString& aMessageId, const wxString& aContent );

    /**
     * Finalize a streaming message.
     */
    void FinalizeStreamingMessage( const wxString& aMessageId );

    /**
     * Clear the chat history.
     */
    void ClearHistory();

    /**
     * Save conversation history to file.
     */
    bool SaveHistory( const wxString& aFilePath );

    /**
     * Load conversation history from file.
     */
    bool LoadHistory( const wxString& aFilePath );

    /**
     * Auto-save history if enabled.
     */
    void AutoSaveHistory();

    /**
     * Set the command processor (for dependency injection in tests).
     */
    void SetCommandProcessor( std::unique_ptr<AI_COMMAND_PROCESSOR> aProcessor );

private:
    void onSendButton( wxCommandEvent& aEvent );
    void onClearButton( wxCommandEvent& aEvent );
    void onCancelButton( wxCommandEvent& aEvent );
    void onSettingsButton( wxCommandEvent& aEvent );
    void onInputEnter( wxCommandEvent& aEvent );
    void onInputText( wxCommandEvent& aEvent );
    void onTimer( wxTimerEvent& aEvent );
    void onContextMenu( wxContextMenuEvent& aEvent );
    void onKeyDown( wxKeyEvent& aEvent );
    void onCharHook( wxKeyEvent& aEvent );

    void processCommand( const wxString& aCommand );
    void updateSendButtonState();
    void scrollToBottom();
    void hideProgressIndicators();
    void addToCommandHistory( const wxString& aCommand );

    /**
     * Parse markdown and format with rich text styling.
     */
    wxString parseMarkdown( const wxString& aText );

    /**
     * Add code block with syntax highlighting.
     */
    void addCodeBlock( const wxString& aCode, const wxString& aLanguage = wxEmptyString );

    /**
     * Add inline code with formatting.
     */
    void addInlineCode( const wxString& aCode );

    EDA_BASE_FRAME* m_parentFrame;
    wxRichTextCtrl* m_chatHistory;
    wxTextCtrl* m_inputField;
    wxButton* m_sendButton;
    wxButton* m_clearButton;
    wxButton* m_cancelButton;
    wxButton* m_settingsButton;
    wxGauge* m_progressBar;
    wxStaticText* m_statusText;
    wxTimer* m_typingTimer;

    std::unique_ptr<AI_COMMAND_PROCESSOR> m_commandProcessor;
    std::unique_ptr<DIALOG_AI_CHAT_SETTINGS> m_settingsDialog;
    
    // Thread safety
    mutable std::mutex m_processingMutex;
    std::atomic<bool> m_isProcessing{ false };
    std::atomic<bool> m_cancelRequested{ false };
    
    // Streaming message management
    std::map<wxString, long> m_streamingMessages;
    int m_nextMessageId;

    // Conversation history
    struct ChatMessage
    {
        wxDateTime timestamp;
        bool isUser;
        wxString content;
    };
    std::vector<ChatMessage> m_history;
    wxString m_historyFilePath;
    
    // Command history for keyboard shortcuts
    std::vector<wxString> m_commandHistory;
    size_t m_historyIndex;

    static constexpr int TYPING_DELAY_MS = 500;
    static constexpr int MAX_COMMAND_HISTORY = 50;
};

#endif // PANEL_AI_CHAT_H