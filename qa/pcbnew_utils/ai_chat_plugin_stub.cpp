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

/**
 * Stub implementation of RegisterAIChatPlugin for QA tests.
 * QA tests link to pcbnew_kiface_objects which contains calls to this function,
 * but they don't link to ai_chat_plugin. This stub provides a no-op implementation.
 */

#include <eda_base_frame.h>

// Forward declaration matching the plugin's interface
void RegisterAIChatPlugin( EDA_BASE_FRAME* aFrame )
{
    // Stub implementation - do nothing for QA tests
    (void)aFrame;  // Suppress unused parameter warning
}
