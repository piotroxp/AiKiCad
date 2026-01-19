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

#ifndef AI_CHAT_PLUGIN_H
#define AI_CHAT_PLUGIN_H

#include <wx/event.h>
#include <eda_base_frame.h>
// Forward declaration to avoid including panel header in header file
class PANEL_AI_CHAT;

/**
 * Plugin manager for the AI Chat window.
 * Handles registration and lifecycle of the chat panel across different editors.
 */
class AI_CHAT_PLUGIN
{
public:
    static AI_CHAT_PLUGIN& GetInstance();

    /**
     * Register the chat panel with a frame.
     * Called when an editor frame is initialized.
     */
    void RegisterFrame( EDA_BASE_FRAME* aFrame );

    /**
     * Unregister a frame.
     * Called when an editor frame is closed.
     */
    void UnregisterFrame( EDA_BASE_FRAME* aFrame );

    /**
     * Show the chat panel for a frame.
     */
    void ShowChatPanel( EDA_BASE_FRAME* aFrame );

    /**
     * Hide the chat panel for a frame.
     */
    void HideChatPanel( EDA_BASE_FRAME* aFrame );

    /**
     * Check if chat panel is visible for a frame.
     */
    bool IsChatPanelVisible( EDA_BASE_FRAME* aFrame ) const;

private:
    AI_CHAT_PLUGIN() = default;
    ~AI_CHAT_PLUGIN() = default;
    AI_CHAT_PLUGIN( const AI_CHAT_PLUGIN& ) = delete;
    AI_CHAT_PLUGIN& operator=( const AI_CHAT_PLUGIN& ) = delete;

    PANEL_AI_CHAT* getOrCreatePanel( EDA_BASE_FRAME* aFrame );
    void addPanelToFrame( EDA_BASE_FRAME* aFrame, PANEL_AI_CHAT* aPanel );

    static AI_CHAT_PLUGIN* s_instance;
};

#endif // AI_CHAT_PLUGIN_H
