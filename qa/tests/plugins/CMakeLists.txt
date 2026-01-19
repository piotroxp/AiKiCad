# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2024 KiCad Developers
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, you may find one here:
# http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
# or you may search the http://www.gnu.org website for the version 2 license,
# or you may write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

find_package( wxWidgets 3.0.0 COMPONENTS gl aui adv html core net base xml stc REQUIRED )

set( QA_PLUGINS_SRCS
    test_ai_chat_plugin.cpp
    ${CMAKE_SOURCE_DIR}/qa/mocks/kicad/common_mocks.cpp
)

add_executable( qa_plugins ${QA_PLUGINS_SRCS} )

# Pcbnew tests, so pretend to be pcbnew (for units, etc)
target_compile_definitions( qa_plugins
    PRIVATE PCBNEW
)

# Ensure dependencies are built first (pcbcommon generates pcb_lexer.h)
add_dependencies( qa_plugins pcbnew pcbcommon eeschema_kiface_objects pcbnew_kiface_objects )

target_link_libraries( qa_plugins
    PRIVATE
        qa_utils
        qa_pcbnew_utils
        ai_chat_plugin
        kicommon
        common
        eeschema_kiface_objects
        pcbnew_kiface_objects
        pcbcommon
        connectivity
        scripting
        gal
        3d-viewer
        pnsrouter
        dxflib_qcad
        tinyspline_lib
        nanosvg
        idf3
        markdown_lib
        ${PCBNEW_IO_LIBRARIES}
        ${wxWidgets_LIBRARIES}
        ${GDI_PLUS_LIBRARIES}
        ${PYTHON_LIBRARIES}
        Boost::headers
        Boost::unit_test_framework
        ${PCBNEW_EXTRA_LIBS}    # -lrt must follow Boost
)

# Allow multiple definitions for shared dialogs when linking both eeschema and pcbnew
# The dialogs have the same class names but different implementations, and qa_plugins
# doesn't actually use them - it only needs the AI chat plugin functionality
# This flag tells the linker to accept multiple definitions and use the first one encountered
if( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
    target_link_options( qa_plugins PRIVATE -Wl,--allow-multiple-definition )
endif()

target_include_directories( qa_plugins
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/3d-viewer
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_SOURCE_DIR}/pcbnew
        ${CMAKE_SOURCE_DIR}/pcbnew/router
        ${CMAKE_SOURCE_DIR}/pcbnew/tools
        ${CMAKE_SOURCE_DIR}/pcbnew/dialogs
        ${CMAKE_SOURCE_DIR}/polygon
        ${CMAKE_SOURCE_DIR}/common/geometry
        ${CMAKE_SOURCE_DIR}/qa
        ${CMAKE_SOURCE_DIR}/qa/mocks/include
        ${wxWidgets_INCLUDE_DIRS}
)

kicad_add_boost_test( qa_plugins qa_plugins )

setup_qa_env( qa_plugins )
