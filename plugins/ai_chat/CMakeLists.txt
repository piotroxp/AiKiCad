# AI Chat Plugin for KiCad
# Provides contextual AI-assisted design actions via natural language commands

add_library( ai_chat_plugin STATIC
    panel_ai_chat.cpp
    panel_ai_chat.h
    dialog_ai_chat_settings.cpp
    dialog_ai_chat_settings.h
    ai_command_processor.cpp
    ai_command_processor.h
    ai_service.cpp
    ai_service.h
    ai_chat_plugin.cpp
    ai_chat_plugin.h
    ai_chat_integration.cpp
    ai_chat_integration.h
)

target_link_libraries( ai_chat_plugin
    PUBLIC
        kicommon
        common
)

target_include_directories( ai_chat_plugin
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/eeschema
        ${CMAKE_SOURCE_DIR}/pcbnew
)

# PCBNew build variant: compile with PCBNEW define to avoid schematic-only references.
add_library( ai_chat_plugin_pcbnew STATIC
    panel_ai_chat.cpp
    panel_ai_chat.h
    dialog_ai_chat_settings.cpp
    dialog_ai_chat_settings.h
    ai_command_processor.cpp
    ai_command_processor.h
    ai_service.cpp
    ai_service.h
    ai_chat_plugin.cpp
    ai_chat_plugin.h
    ai_chat_integration.cpp
    ai_chat_integration.h
)

target_link_libraries( ai_chat_plugin_pcbnew
    PUBLIC
        kicommon
        common
)

target_include_directories( ai_chat_plugin_pcbnew
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/eeschema
        ${CMAKE_SOURCE_DIR}/pcbnew
)

target_compile_definitions( ai_chat_plugin_pcbnew
    PRIVATE
        PCBNEW
)
