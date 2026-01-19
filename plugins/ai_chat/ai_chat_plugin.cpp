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

#include "ai_chat_plugin.h"
#include "panel_ai_chat.h"  // Include here for getOrCreatePanel implementation
#include <eda_base_frame.h>
#include <wx/aui/aui.h>
#include <wx/log.h>

AI_CHAT_PLUGIN* AI_CHAT_PLUGIN::s_instance = nullptr;

AI_CHAT_PLUGIN& AI_CHAT_PLUGIN::GetInstance()
{
    if( !s_instance )
        s_instance = new AI_CHAT_PLUGIN();
    return *s_instance;
}


void AI_CHAT_PLUGIN::RegisterFrame( EDA_BASE_FRAME* aFrame )
{
    if( !aFrame )
        return;

    // Only register for supported frame types
    FRAME_T frameType = aFrame->GetFrameType();
    if( frameType == FRAME_SCH || frameType == FRAME_PCB_EDITOR ||
        frameType == FRAME_FOOTPRINT_EDITOR )
    {
        // Don't create/add panel here - do it lazily when first shown
        // This avoids issues with AUI initialization and icon loading
    }
}


void AI_CHAT_PLUGIN::UnregisterFrame( EDA_BASE_FRAME* aFrame )
{
    // Panel will be destroyed automatically when frame is destroyed
    // No explicit cleanup needed
}


// Helper class to access protected m_auimgr member
// Since m_auimgr is protected in EDA_BASE_FRAME, we use a workaround
// by creating a derived class that exposes access through inheritance
class EDA_BASE_FRAME_ACCESSOR : public EDA_BASE_FRAME
{
public:
    // Expose protected member access
    static wxAuiManager* GetAuiManager( EDA_BASE_FRAME* aFrame )
    {
        // Use reinterpret_cast to access as derived class
        // This is safe because we're only accessing the m_auimgr member
        // which is at the same offset in both classes
        return &( reinterpret_cast<EDA_BASE_FRAME_ACCESSOR*>( aFrame )->m_auimgr );
    }
};

void AI_CHAT_PLUGIN::ShowChatPanel( EDA_BASE_FRAME* aFrame )
{
    PANEL_AI_CHAT* panel = getOrCreatePanel( aFrame );
    if( panel )
    {
        wxAuiManager* auiMgr = EDA_BASE_FRAME_ACCESSOR::GetAuiManager( aFrame );
        if( auiMgr )
        {
            wxAuiPaneInfo& pane = auiMgr->GetPane( panel );
            if( !pane.IsOk() )
            {
                // Panel not yet added to AUI manager - add it now
                addPanelToFrame( aFrame, panel );
            }
            
            // Now show it
            wxAuiPaneInfo& pane2 = auiMgr->GetPane( panel );
            if( pane2.IsOk() )
            {
                pane2.Show();
                auiMgr->Update();
            }
        }
    }
}


void AI_CHAT_PLUGIN::HideChatPanel( EDA_BASE_FRAME* aFrame )
{
    if( !aFrame )
        return;
    
    // Check if panel exists first - if not, it's already hidden
    wxWindow* existing = aFrame->FindWindowByName( wxT( "AIChatPanel" ) );
    if( !existing )
        return;
    
    PANEL_AI_CHAT* panel = dynamic_cast<PANEL_AI_CHAT*>( existing );
    if( panel )
    {
        wxAuiManager* auiMgr = EDA_BASE_FRAME_ACCESSOR::GetAuiManager( aFrame );
        if( auiMgr )
        {
            wxAuiPaneInfo& pane = auiMgr->GetPane( panel );
            if( pane.IsOk() )
            {
                pane.Hide();
                auiMgr->Update();
            }
        }
    }
}


bool AI_CHAT_PLUGIN::IsChatPanelVisible( EDA_BASE_FRAME* aFrame ) const
{
    // Check if panel exists first
    wxWindow* existing = aFrame ? aFrame->FindWindowByName( wxT( "AIChatPanel" ) ) : nullptr;
    if( !existing )
        return false;
    
    PANEL_AI_CHAT* panel = dynamic_cast<PANEL_AI_CHAT*>( existing );
    if( panel )
    {
        wxAuiManager* auiMgr = EDA_BASE_FRAME_ACCESSOR::GetAuiManager( aFrame );
        if( auiMgr )
        {
            wxAuiPaneInfo& pane = auiMgr->GetPane( panel );
            if( pane.IsOk() )
                return pane.IsShown();
        }
    }
    return false;
}


PANEL_AI_CHAT* AI_CHAT_PLUGIN::getOrCreatePanel( EDA_BASE_FRAME* aFrame )
{
    if( !aFrame )
        return nullptr;

    // Check if panel already exists
    wxWindow* existing = aFrame->FindWindowByName( wxT( "AIChatPanel" ) );
    if( existing )
        return dynamic_cast<PANEL_AI_CHAT*>( existing );

    // Create new panel
    PANEL_AI_CHAT* panel = new PANEL_AI_CHAT( aFrame );
    panel->SetName( wxT( "AIChatPanel" ) );
    return panel;
}


void AI_CHAT_PLUGIN::addPanelToFrame( EDA_BASE_FRAME* aFrame, PANEL_AI_CHAT* aPanel )
{
    if( !aFrame || !aPanel )
        return;

    wxAuiManager* auiMgr = EDA_BASE_FRAME_ACCESSOR::GetAuiManager( aFrame );
    if( !auiMgr )
        return;

    // Check if panel is already added
    wxAuiPaneInfo& existingPane = auiMgr->GetPane( aPanel );
    if( existingPane.IsOk() )
        return;

    // Add panel to right side by default
    auiMgr->AddPane( aPanel, EDA_PANE()
                    .Name( wxT( "AIChatPanel" ) )
                    .Caption( _( "AI Chat Assistant" ) )
                    .Right()
                    .Layer( 1 )
                    .Position( 0 )
                    .MinSize( wxSize( 300, 200 ) )
                    .BestSize( wxSize( 400, 400 ) )
                    .FloatingSize( wxSize( 500, 600 ) )
                    .CloseButton( true )
                    .DestroyOnClose( false )
                    .Show( false ) ); // Hidden by default

    // Don't call Update() here - let the caller do it when ready
    // This avoids triggering icon loading/resizing during initialization
}
