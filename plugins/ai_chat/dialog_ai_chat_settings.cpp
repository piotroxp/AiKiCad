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

#include "dialog_ai_chat_settings.h"
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/panel.h>

// Static member definition
const wxString DIALOG_AI_CHAT_SETTINGS::DEFAULT_OLLAMA_URL = wxT( "http://localhost:11434" );

DIALOG_AI_CHAT_SETTINGS::DIALOG_AI_CHAT_SETTINGS( wxWindow* aParent )
        : wxDialog( aParent, wxID_ANY, _( "AI Chat Settings" ), wxDefaultPosition, wxSize( 500, 400 ),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
          m_settingsChanged( false )
{
    // Create main sizer
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    // Create notebook for tabbed interface
    m_notebook = new wxNotebook( this, wxID_ANY );
    
    createConnectionPage();
    createPrivacyPage();
    createPerformancePage();

    mainSizer->Add( m_notebook, 1, wxEXPAND | wxALL, 5 );

    // Dialog buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer( wxHORIZONTAL );
    buttonSizer->AddStretchSpacer();
    
    m_testButton = new wxButton( this, wxID_ANY, _( "Test Connection" ) );
    buttonSizer->Add( m_testButton, 0, wxALL, 5 );
    
    m_okButton = new wxButton( this, wxID_OK, _( "OK" ) );
    buttonSizer->Add( m_okButton, 0, wxALL, 5 );
    
    m_cancelButton = new wxButton( this, wxID_CANCEL, _( "Cancel" ) );
    buttonSizer->Add( m_cancelButton, 0, wxALL, 5 );

    mainSizer->Add( buttonSizer, 0, wxEXPAND | wxALL, 5 );

    SetSizer( mainSizer );
    Layout();
    Centre( wxBOTH );

    // Bind events
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &DIALOG_AI_CHAT_SETTINGS::onOk, this, wxID_OK );
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &DIALOG_AI_CHAT_SETTINGS::onCancel, this, wxID_CANCEL );
    Bind( wxEVT_COMMAND_BUTTON_CLICKED, &DIALOG_AI_CHAT_SETTINGS::onTestConnection, this, m_testButton->GetId() );
    Bind( wxEVT_TEXT, &DIALOG_AI_CHAT_SETTINGS::onUrlChanged, this, m_ollamaUrl->GetId() );
    Bind( wxEVT_COMBOBOX, &DIALOG_AI_CHAT_SETTINGS::onModelChanged, this, m_modelCombo->GetId() );

    // Load current settings
    loadSettings();
}


DIALOG_AI_CHAT_SETTINGS::~DIALOG_AI_CHAT_SETTINGS()
{
}


void DIALOG_AI_CHAT_SETTINGS::createConnectionPage()
{
    wxPanel* panel = new wxPanel( m_notebook );
    wxBoxSizer* sizer = new wxBoxSizer( wxVERTICAL );

    // Connection Settings Group
    wxStaticBoxSizer* connBox = new wxStaticBoxSizer( new wxStaticBox( panel, wxID_ANY, _( "Connection" ) ), wxVERTICAL );

    // Ollama URL
    wxBoxSizer* urlSizer = new wxBoxSizer( wxHORIZONTAL );
    urlSizer->Add( new wxStaticText( panel, wxID_ANY, _( "Ollama URL:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_ollamaUrl = new wxTextCtrl( panel, wxID_ANY, DEFAULT_OLLAMA_URL );
    urlSizer->Add( m_ollamaUrl, 1, wxEXPAND | wxALL, 5 );
    connBox->Add( urlSizer, 0, wxEXPAND );

    // Model Selection
    wxBoxSizer* modelSizer = new wxBoxSizer( wxHORIZONTAL );
    modelSizer->Add( new wxStaticText( panel, wxID_ANY, _( "AI Model:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_modelCombo = new wxComboBox( panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY );
    modelSizer->Add( m_modelCombo, 1, wxEXPAND | wxALL, 5 );
    connBox->Add( modelSizer, 0, wxEXPAND );

    // Timeout
    wxBoxSizer* timeoutSizer = new wxBoxSizer( wxHORIZONTAL );
    timeoutSizer->Add( new wxStaticText( panel, wxID_ANY, _( "Timeout (seconds):" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_timeoutSeconds = new wxSpinCtrl( panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 300, DEFAULT_TIMEOUT_SECONDS );
    timeoutSizer->Add( m_timeoutSeconds, 0, wxALL, 5 );
    connBox->Add( timeoutSizer, 0, wxEXPAND );

    // Max Retries
    wxBoxSizer* retrySizer = new wxBoxSizer( wxHORIZONTAL );
    retrySizer->Add( new wxStaticText( panel, wxID_ANY, _( "Max Retries:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_maxRetries = new wxSpinCtrl( panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10, DEFAULT_MAX_RETRIES );
    retrySizer->Add( m_maxRetries, 0, wxALL, 5 );
    connBox->Add( retrySizer, 0, wxEXPAND );

    sizer->Add( connBox, 0, wxEXPAND | wxALL, 5 );

    // Connection Status
    m_connectionStatus = new wxStaticText( panel, wxID_ANY, _( "Not tested" ) );
    m_connectionStatus->SetForegroundColour( wxColour( 128, 128, 128 ) );
    sizer->Add( m_connectionStatus, 0, wxALL, 5 );

    panel->SetSizer( sizer );
    m_notebook->AddPage( panel, _( "Connection" ) );
}


void DIALOG_AI_CHAT_SETTINGS::createPrivacyPage()
{
    wxPanel* panel = new wxPanel( m_notebook );
    wxBoxSizer* sizer = new wxBoxSizer( wxVERTICAL );

    // Data Privacy Group
    wxStaticBoxSizer* privacyBox = new wxStaticBoxSizer( new wxStaticBox( panel, wxID_ANY, _( "Data Privacy" ) ), wxVERTICAL );

    m_sendFilePaths = new wxCheckBox( panel, wxID_ANY, _( "Send file paths to AI service" ) );
    privacyBox->Add( m_sendFilePaths, 0, wxALL, 5 );

    m_sendComponentLists = new wxCheckBox( panel, wxID_ANY, _( "Send component lists to AI service" ) );
    privacyBox->Add( m_sendComponentLists, 0, wxALL, 5 );

    m_sendProjectData = new wxCheckBox( panel, wxID_ANY, _( "Send project metadata to AI service" ) );
    privacyBox->Add( m_sendProjectData, 0, wxALL, 5 );

    sizer->Add( privacyBox, 0, wxEXPAND | wxALL, 5 );

    // History Management Group
    wxStaticBoxSizer* historyBox = new wxStaticBoxSizer( new wxStaticBox( panel, wxID_ANY, _( "History Management" ) ), wxVERTICAL );

    m_saveHistory = new wxCheckBox( panel, wxID_ANY, _( "Save conversation history" ) );
    historyBox->Add( m_saveHistory, 0, wxALL, 5 );

    wxBoxSizer* pathSizer = new wxBoxSizer( wxHORIZONTAL );
    pathSizer->Add( new wxStaticText( panel, wxID_ANY, _( "History Path:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_historyPath = new wxTextCtrl( panel, wxID_ANY, wxEmptyString );
    pathSizer->Add( m_historyPath, 1, wxEXPAND | wxALL, 5 );
    historyBox->Add( pathSizer, 0, wxEXPAND );

    sizer->Add( historyBox, 0, wxEXPAND | wxALL, 5 );

    panel->SetSizer( sizer );
    m_notebook->AddPage( panel, _( "Privacy" ) );
}


void DIALOG_AI_CHAT_SETTINGS::createPerformancePage()
{
    wxPanel* panel = new wxPanel( m_notebook );
    wxBoxSizer* sizer = new wxBoxSizer( wxVERTICAL );

    // Performance Settings Group
    wxStaticBoxSizer* perfBox = new wxStaticBoxSizer( new wxStaticBox( panel, wxID_ANY, _( "Performance" ) ), wxVERTICAL );

    // Max Context Size
    wxBoxSizer* contextSizer = new wxBoxSizer( wxHORIZONTAL );
    contextSizer->Add( new wxStaticText( panel, wxID_ANY, _( "Max Context Size:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_maxContextSize = new wxSpinCtrl( panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 10, 1000, DEFAULT_MAX_CONTEXT_SIZE );
    contextSizer->Add( m_maxContextSize, 0, wxALL, 5 );
    contextSizer->Add( new wxStaticText( panel, wxID_ANY, _( "components/footprints" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    perfBox->Add( contextSizer, 0, wxEXPAND );

    // Response Caching
    m_enableCaching = new wxCheckBox( panel, wxID_ANY, _( "Enable response caching" ) );
    perfBox->Add( m_enableCaching, 0, wxALL, 5 );

    // Cache TTL
    wxBoxSizer* cacheSizer = new wxBoxSizer( wxHORIZONTAL );
    cacheSizer->Add( new wxStaticText( panel, wxID_ANY, _( "Cache TTL:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_cacheTTLHours = new wxSpinCtrl( panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 168, DEFAULT_CACHE_TTL_HOURS );
    cacheSizer->Add( m_cacheTTLHours, 0, wxALL, 5 );
    cacheSizer->Add( new wxStaticText( panel, wxID_ANY, _( "hours" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    perfBox->Add( cacheSizer, 0, wxEXPAND );

    // Max Concurrent Requests
    wxBoxSizer* concurrentSizer = new wxBoxSizer( wxHORIZONTAL );
    concurrentSizer->Add( new wxStaticText( panel, wxID_ANY, _( "Max Concurrent Requests:" ) ), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    m_maxConcurrentRequests = new wxSpinCtrl( panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 5, DEFAULT_MAX_CONCURRENT_REQUESTS );
    concurrentSizer->Add( m_maxConcurrentRequests, 0, wxALL, 5 );
    perfBox->Add( concurrentSizer, 0, wxEXPAND );

    sizer->Add( perfBox, 0, wxEXPAND | wxALL, 5 );

    panel->SetSizer( sizer );
    m_notebook->AddPage( panel, _( "Performance" ) );
}


wxString DIALOG_AI_CHAT_SETTINGS::GetOllamaUrl() const
{
    return m_ollamaUrl->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetOllamaUrl( const wxString& aUrl )
{
    m_ollamaUrl->SetValue( aUrl );
}


wxString DIALOG_AI_CHAT_SETTINGS::GetSelectedModel() const
{
    return m_modelCombo->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetSelectedModel( const wxString& aModel )
{
    m_modelCombo->SetValue( aModel );
}


int DIALOG_AI_CHAT_SETTINGS::GetTimeoutSeconds() const
{
    return m_timeoutSeconds->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetTimeoutSeconds( int aTimeout )
{
    m_timeoutSeconds->SetValue( aTimeout );
}


int DIALOG_AI_CHAT_SETTINGS::GetMaxRetries() const
{
    return m_maxRetries->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetMaxRetries( int aMaxRetries )
{
    m_maxRetries->SetValue( aMaxRetries );
}


bool DIALOG_AI_CHAT_SETTINGS::GetSendFilePaths() const
{
    return m_sendFilePaths->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetSendFilePaths( bool aSend )
{
    m_sendFilePaths->SetValue( aSend );
}


bool DIALOG_AI_CHAT_SETTINGS::GetSendComponentLists() const
{
    return m_sendComponentLists->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetSendComponentLists( bool aSend )
{
    m_sendComponentLists->SetValue( aSend );
}


bool DIALOG_AI_CHAT_SETTINGS::GetSaveHistory() const
{
    return m_saveHistory->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetSaveHistory( bool aSave )
{
    m_saveHistory->SetValue( aSave );
}


int DIALOG_AI_CHAT_SETTINGS::GetMaxContextSize() const
{
    return m_maxContextSize->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetMaxContextSize( int aMaxSize )
{
    m_maxContextSize->SetValue( aMaxSize );
}


bool DIALOG_AI_CHAT_SETTINGS::GetEnableCaching() const
{
    return m_enableCaching->GetValue();
}


void DIALOG_AI_CHAT_SETTINGS::SetEnableCaching( bool aEnable )
{
    m_enableCaching->SetValue( aEnable );
}


void DIALOG_AI_CHAT_SETTINGS::UpdateAvailableModels( const std::vector<wxString>& aModels )
{
    m_availableModels = aModels;
    m_modelCombo->Clear();
    
    for( const wxString& model : aModels )
    {
        m_modelCombo->Append( model );
    }
    
    if( !aModels.empty() && m_modelCombo->GetValue().IsEmpty() )
    {
        m_modelCombo->SetSelection( 0 );
    }
}


bool DIALOG_AI_CHAT_SETTINGS::TestConnection()
{
    // Basic URL validation
    wxString url = m_ollamaUrl->GetValue().Trim();
    
    if( url.IsEmpty() )
    {
        m_connectionStatus->SetLabel( _( "Error: URL cannot be empty" ) );
        m_connectionStatus->SetForegroundColour( wxColour( 255, 0, 0 ) );
        return false;
    }
    
    // Simple URL format validation
    wxRegEx urlRegex( wxT( "^https?://[^/]+" ) );
    if( !urlRegex.Matches( url ) )
    {
        m_connectionStatus->SetLabel( _( "Error: Invalid URL format" ) );
        m_connectionStatus->SetForegroundColour( wxColour( 255, 0, 0 ) );
        return false;
    }
    
    m_connectionStatus->SetLabel( _( "Connection test not implemented yet" ) );
    m_connectionStatus->SetForegroundColour( wxColour( 128, 128, 128 ) );
    return true;
}


void DIALOG_AI_CHAT_SETTINGS::onOk( wxCommandEvent& aEvent )
{
    if( m_settingsChanged )
    {
        validateSettings();
        saveSettings();
    }
    EndModal( wxID_OK );
}


void DIALOG_AI_CHAT_SETTINGS::onCancel( wxCommandEvent& aEvent )
{
    EndModal( wxID_CANCEL );
}


void DIALOG_AI_CHAT_SETTINGS::onTestConnection( wxCommandEvent& aEvent )
{
    TestConnection();
}


void DIALOG_AI_CHAT_SETTINGS::onUrlChanged( wxCommandEvent& aEvent )
{
    m_settingsChanged = true;
    // Reset connection status when URL changes
    m_connectionStatus->SetLabel( _( "Not tested" ) );
    m_connectionStatus->SetForegroundColour( wxColour( 128, 128, 128 ) );
}


void DIALOG_AI_CHAT_SETTINGS::onModelChanged( wxCommandEvent& aEvent )
{
    m_settingsChanged = true;
}


void DIALOG_AI_CHAT_SETTINGS::loadSettings()
{
    wxConfig config( wxT( "KiCad" ), wxT( "KiCad" ) );
    
    if( config.HasGroup( wxT( "AI_Chat" ) ) )
    {
        config.SetPath( wxT( "AI_Chat" ) );
        
        m_ollamaUrl->SetValue( config.Read( wxT( "OllamaUrl" ), DEFAULT_OLLAMA_URL ) );
        m_modelCombo->SetValue( config.Read( wxT( "Model" ), wxEmptyString ) );
        m_timeoutSeconds->SetValue( config.ReadLong( wxT( "TimeoutSeconds" ), DEFAULT_TIMEOUT_SECONDS ) );
        m_maxRetries->SetValue( config.ReadLong( wxT( "MaxRetries" ), DEFAULT_MAX_RETRIES ) );
        
        m_sendFilePaths->SetValue( config.ReadBool( wxT( "SendFilePaths" ), true ) );
        m_sendComponentLists->SetValue( config.ReadBool( wxT( "SendComponentLists" ), true ) );
        m_sendProjectData->SetValue( config.ReadBool( wxT( "SendProjectData" ), false ) );
        m_saveHistory->SetValue( config.ReadBool( wxT( "SaveHistory" ), true ) );
        
        wxString defaultHistoryPath = wxFileName::GetHomeDir() + wxFileName::GetPathSeparator() + wxT( ".kicad" ) + wxFileName::GetPathSeparator() + wxT( "ai_chat_history.json" );
        m_historyPath->SetValue( config.Read( wxT( "HistoryPath" ), defaultHistoryPath ) );
        
        m_maxContextSize->SetValue( config.ReadLong( wxT( "MaxContextSize" ), DEFAULT_MAX_CONTEXT_SIZE ) );
        m_enableCaching->SetValue( config.ReadBool( wxT( "EnableCaching" ), true ) );
        m_cacheTTLHours->SetValue( config.ReadLong( wxT( "CacheTTLHours" ), DEFAULT_CACHE_TTL_HOURS ) );
        m_maxConcurrentRequests->SetValue( config.ReadLong( wxT( "MaxConcurrentRequests" ), DEFAULT_MAX_CONCURRENT_REQUESTS ) );
    }
    else
    {
        // Set defaults
        m_ollamaUrl->SetValue( DEFAULT_OLLAMA_URL );
        m_timeoutSeconds->SetValue( DEFAULT_TIMEOUT_SECONDS );
        m_maxRetries->SetValue( DEFAULT_MAX_RETRIES );
        m_maxContextSize->SetValue( DEFAULT_MAX_CONTEXT_SIZE );
        m_enableCaching->SetValue( true );
        m_cacheTTLHours->SetValue( DEFAULT_CACHE_TTL_HOURS );
        m_maxConcurrentRequests->SetValue( DEFAULT_MAX_CONCURRENT_REQUESTS );
        
        // Privacy defaults
        m_sendFilePaths->SetValue( true );
        m_sendComponentLists->SetValue( true );
        m_sendProjectData->SetValue( false );
        m_saveHistory->SetValue( true );
        
        wxString defaultHistoryPath = wxFileName::GetHomeDir() + wxFileName::GetPathSeparator() + wxT( ".kicad" ) + wxFileName::GetPathSeparator() + wxT( "ai_chat_history.json" );
        m_historyPath->SetValue( defaultHistoryPath );
    }
}


void DIALOG_AI_CHAT_SETTINGS::saveSettings()
{
    wxConfig config( wxT( "KiCad" ), wxT( "KiCad" ) );
    
    config.SetPath( wxT( "AI_Chat" ) );
    
    config.Write( wxT( "OllamaUrl" ), m_ollamaUrl->GetValue() );
    config.Write( wxT( "Model" ), m_modelCombo->GetValue() );
    config.Write( wxT( "TimeoutSeconds" ), m_timeoutSeconds->GetValue() );
    config.Write( wxT( "MaxRetries" ), m_maxRetries->GetValue() );
    
    config.Write( wxT( "SendFilePaths" ), m_sendFilePaths->GetValue() );
    config.Write( wxT( "SendComponentLists" ), m_sendComponentLists->GetValue() );
    config.Write( wxT( "SendProjectData" ), m_sendProjectData->GetValue() );
    config.Write( wxT( "SaveHistory" ), m_saveHistory->GetValue() );
    config.Write( wxT( "HistoryPath" ), m_historyPath->GetValue() );
    
    config.Write( wxT( "MaxContextSize" ), m_maxContextSize->GetValue() );
    config.Write( wxT( "EnableCaching" ), m_enableCaching->GetValue() );
    config.Write( wxT( "CacheTTLHours" ), m_cacheTTLHours->GetValue() );
    config.Write( wxT( "MaxConcurrentRequests" ), m_maxConcurrentRequests->GetValue() );
    
    config.Flush();
}


void DIALOG_AI_CHAT_SETTINGS::validateSettings()
{
    // Basic validation - could be expanded
    if( m_timeoutSeconds->GetValue() < 1 )
    {
        m_timeoutSeconds->SetValue( 1 );
    }
    
    if( m_maxRetries->GetValue() < 0 )
    {
        m_maxRetries->SetValue( 0 );
    }
    
    if( m_maxContextSize->GetValue() < 10 )
    {
        m_maxContextSize->SetValue( 10 );
    }
}