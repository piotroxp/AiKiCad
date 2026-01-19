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

#ifndef DIALOG_AI_CHAT_SETTINGS_H
#define DIALOG_AI_CHAT_SETTINGS_H

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/notebook.h>
#include <wx/string.h>
#include <vector>

/**
 * AI Chat Settings Dialog
 * Provides configuration options for AI service connection, models, and privacy settings.
 */
class DIALOG_AI_CHAT_SETTINGS : public wxDialog
{
public:
    DIALOG_AI_CHAT_SETTINGS( wxWindow* aParent );
    ~DIALOG_AI_CHAT_SETTINGS() override;

    /**
     * Get the configured Ollama base URL.
     */
    wxString GetOllamaUrl() const;

    /**
     * Set the Ollama base URL.
     */
    void SetOllamaUrl( const wxString& aUrl );

    /**
     * Get the selected AI model.
     */
    wxString GetSelectedModel() const;

    /**
     * Set the selected AI model.
     */
    void SetSelectedModel( const wxString& aModel );

    /**
     * Get the connection timeout in seconds.
     */
    int GetTimeoutSeconds() const;

    /**
     * Set the connection timeout in seconds.
     */
    void SetTimeoutSeconds( int aTimeout );

    /**
     * Get the maximum number of retry attempts.
     */
    int GetMaxRetries() const;

    /**
     * Set the maximum number of retry attempts.
     */
    void SetMaxRetries( int aMaxRetries );

    /**
     * Check if file paths should be sent to AI.
     */
    bool GetSendFilePaths() const;

    /**
     * Set whether to send file paths to AI.
     */
    void SetSendFilePaths( bool aSend );

    /**
     * Check if component lists should be sent to AI.
     */
    bool GetSendComponentLists() const;

    /**
     * Set whether to send component lists to AI.
     */
    void SetSendComponentLists( bool aSend );

    /**
     * Check if conversation history should be saved.
     */
    bool GetSaveHistory() const;

    /**
     * Set whether to save conversation history.
     */
    void SetSaveHistory( bool aSave );

    /**
     * Get the maximum context size (number of components/footprints).
     */
    int GetMaxContextSize() const;

    /**
     * Set the maximum context size.
     */
    void SetMaxContextSize( int aMaxSize );

    /**
     * Check if response caching is enabled.
     */
    bool GetEnableCaching() const;

    /**
     * Set whether to enable response caching.
     */
    void SetEnableCaching( bool aEnable );

    /**
     * Update the list of available models.
     */
    void UpdateAvailableModels( const std::vector<wxString>& aModels );

    /**
     * Test the connection to the AI service.
     */
    bool TestConnection();

private:
    void onOk( wxCommandEvent& aEvent );
    void onCancel( wxCommandEvent& aEvent );
    void onTestConnection( wxCommandEvent& aEvent );
    void onUrlChanged( wxCommandEvent& aEvent );
    void onModelChanged( wxCommandEvent& aEvent );

    void createConnectionPage();
    void createPrivacyPage();
    void createPerformancePage();

    void loadSettings();
    void saveSettings();
    void validateSettings();

    // UI Controls
    wxNotebook* m_notebook;

    // Connection Page
    wxTextCtrl* m_ollamaUrl;
    wxComboBox* m_modelCombo;
    wxSpinCtrl* m_timeoutSeconds;
    wxSpinCtrl* m_maxRetries;
    wxButton* m_testButton;
    wxStaticText* m_connectionStatus;

    // Privacy Page
    wxCheckBox* m_sendFilePaths;
    wxCheckBox* m_sendComponentLists;
    wxCheckBox* m_sendProjectData;
    wxCheckBox* m_saveHistory;
    wxTextCtrl* m_historyPath;

    // Performance Page
    wxSpinCtrl* m_maxContextSize;
    wxCheckBox* m_enableCaching;
    wxSpinCtrl* m_cacheTTLHours;
    wxSpinCtrl* m_maxConcurrentRequests;

    // Dialog buttons
    wxButton* m_okButton;
    wxButton* m_cancelButton;

    // Internal state
    std::vector<wxString> m_availableModels;
    bool m_settingsChanged;

    // Default values
    static const wxString DEFAULT_OLLAMA_URL;
    static constexpr int DEFAULT_TIMEOUT_SECONDS = 30;
    static constexpr int DEFAULT_MAX_RETRIES = 3;
    static constexpr int DEFAULT_MAX_CONTEXT_SIZE = 100;
    static constexpr int DEFAULT_CACHE_TTL_HOURS = 24;
    static constexpr int DEFAULT_MAX_CONCURRENT_REQUESTS = 1;
};

#endif // DIALOG_AI_CHAT_SETTINGS_H