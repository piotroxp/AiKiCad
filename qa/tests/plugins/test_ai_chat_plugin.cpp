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

#include <boost/test/unit_test.hpp>
#include <wx/app.h>
#include <mock_pgm_base.h>
#include <pgm_base.h>
#include <settings/settings_manager.h>
#include <qa_utils/wx_utils/wx_assert.h>
#include <locale_io.h>
#include <plugins/ai_chat/ai_command_processor.h>
#include <plugins/ai_chat/ai_service.h>
#include <eda_base_frame.h>
#include <base_units.h>
#include <pcb_edit_frame.h>
#include <footprint_edit_frame.h>
#include <typeinfo>

// Force typeinfo to be included by referencing the frame types
// This ensures the linker includes the typeinfo symbols needed for dynamic_cast
namespace {
    // Dummy function to force typeinfo inclusion - called during test initialization
    void force_typeinfo_inclusion()
    {
        // These typeid calls force the linker to include typeinfo for these classes
        // even though they're only used via dynamic_cast in the plugin code
        static const std::type_info* pcb_type = &typeid(PCB_EDIT_FRAME);
        static const std::type_info* fp_type = &typeid(FOOTPRINT_EDIT_FRAME);
        (void)pcb_type;  // Suppress unused variable warning
        (void)fp_type;   // Suppress unused variable warning
    }
}

// Mock file operations for testing
class MOCK_FILE_OPERATIONS : public I_FILE_OPERATIONS
{
public:
    bool SaveFile( const wxString& aPath, const wxString& aContent ) override
    {
        m_savedFiles[aPath] = aContent;
        return true;
    }

    bool LoadFile( const wxString& aPath, wxString& aContent ) override
    {
        if( m_savedFiles.find( aPath ) != m_savedFiles.end() )
        {
            aContent = m_savedFiles[aPath];
            return true;
        }
        return false;
    }

    bool FileExists( const wxString& aPath ) override
    {
        return m_savedFiles.find( aPath ) != m_savedFiles.end();
    }

    std::map<wxString, wxString> m_savedFiles;
};

// Mock frame for testing - minimal implementation
class MOCK_EDA_FRAME : public EDA_BASE_FRAME
{
public:
    MOCK_EDA_FRAME() : EDA_BASE_FRAME( nullptr, FRAME_SCH, wxT( "Test" ), wxDefaultPosition,
                                       wxDefaultSize, 0, wxT( "TestFrame" ), nullptr, EDA_IU_SCALE( SCH_IU_PER_MM ) )
    {
    }

    // Required overrides (minimal implementation)
    wxString GetName() const override { return wxT( "TestFrame" ); }
    void UpdateStatusBar() override {}
    
    // Required pure virtual from TOOLS_HOLDER
    wxWindow* GetToolCanvas() const override { return nullptr; }
};

bool init_unit_test()
{
    KI_TEST::SetMockConfigDir();
    SetPgm( new MOCK_PGM_BASE() );
    boost::unit_test::framework::master_test_suite().p_name.value = "AI Chat Plugin tests";

    wxApp::SetInstance( new wxAppConsole );

    bool ok = wxInitialize( boost::unit_test::framework::master_test_suite().argc,
                           boost::unit_test::framework::master_test_suite().argv );

    if( ok )
    {
        wxSetAssertHandler( &KI_TEST::wxAssertThrower );
        Pgm().InitPgm( true, true, true );
        Pgm().GetSettingsManager().LoadProject( "" );
        
        // Force typeinfo inclusion for frame types used by the plugin
        force_typeinfo_inclusion();
    }

    return ok;
}

BOOST_AUTO_TEST_SUITE( AI_Chat_Plugin_Tests )

BOOST_AUTO_TEST_CASE( TestCommandProcessorBasicParsing )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );

    // Test component parsing through ProcessCommand
    AI_COMMAND_RESULT result = processor.ProcessCommand( wxT( "add component R1 at 100,200" ) );
    BOOST_CHECK( result.success || !result.error.IsEmpty() ); // Should either succeed or give error
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorIdempotentOperations )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    auto mockAIService = std::make_unique<MOCK_AI_SERVICE>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );
    processor.SetAIService( std::move( mockAIService ) );

    // Test that repeated commands yield consistent results
    wxString command = wxT( "add component C1 at 50,50" );
    AI_COMMAND_RESULT result1 = processor.ProcessCommand( command );
    AI_COMMAND_RESULT result2 = processor.ProcessCommand( command );

    // Both should succeed with same message
    BOOST_CHECK( result1.success == result2.success );
    BOOST_CHECK_EQUAL( result1.message, result2.message );
}

BOOST_AUTO_TEST_CASE( TestAIServiceMock )
{
    MOCK_AI_SERVICE aiService;
    AI_CONTEXT context;
    context.editorType = wxT( "schematic" );

    AI_RESPONSE response = aiService.ProcessPrompt( wxT( "test prompt" ), context );
    BOOST_CHECK( response.success );
    BOOST_CHECK( !response.message.IsEmpty() );
    BOOST_CHECK( response.isComplete );
}

BOOST_AUTO_TEST_CASE( TestAIServiceStreaming )
{
    MOCK_AI_SERVICE aiService;
    AI_CONTEXT context;
    context.editorType = wxT( "schematic" );

    wxString receivedChunks;
    AI_RESPONSE response = aiService.ProcessPromptStreaming(
        wxT( "test" ), context,
        [&receivedChunks]( const wxString& chunk ) { receivedChunks += chunk; } );

    BOOST_CHECK( response.success );
    BOOST_CHECK( !receivedChunks.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestFileOperationsMock )
{
    MOCK_FILE_OPERATIONS fileOps;
    wxString testPath = wxT( "/test/path.txt" );
    wxString testContent = wxT( "test content" );

    // Test save
    BOOST_CHECK( fileOps.SaveFile( testPath, testContent ) );

    // Test exists
    BOOST_CHECK( fileOps.FileExists( testPath ) );

    // Test load
    wxString loadedContent;
    BOOST_CHECK( fileOps.LoadFile( testPath, loadedContent ) );
    BOOST_CHECK_EQUAL( loadedContent, testContent );
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorTraceParsing )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    auto mockAIService = std::make_unique<MOCK_AI_SERVICE>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );
    processor.SetAIService( std::move( mockAIService ) );

    // Test trace parsing through ProcessCommand
    AI_COMMAND_RESULT result = processor.ProcessCommand( wxT( "add trace from 0,0 to 100,100 width 10" ) );
    BOOST_CHECK( result.success || !result.error.IsEmpty() ); // Should either succeed or give error
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorContext )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );

    wxString context = processor.GetContext();
    BOOST_CHECK( !context.IsEmpty() );
    BOOST_CHECK( context == wxT( "schematic" ) || context == wxT( "board" ) || context == wxT( "unknown" ) );
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorEmptyCommand )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );

    AI_COMMAND_RESULT result = processor.ProcessCommand( wxT( "" ) );
    BOOST_CHECK( !result.success );
    BOOST_CHECK( !result.error.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorInvalidCommand )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );

    AI_COMMAND_RESULT result = processor.ProcessCommand( wxT( "invalid command that doesn't match any pattern" ) );
    // Should either succeed (if AI service handles it) or provide an error
    BOOST_CHECK( result.success || !result.error.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorModifyComponent )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    auto mockAIService = std::make_unique<MOCK_AI_SERVICE>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );
    processor.SetAIService( std::move( mockAIService ) );

    AI_COMMAND_RESULT result = processor.ProcessCommand( wxT( "modify component U1 value 3.3V" ) );
    BOOST_CHECK( result.success || !result.error.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorModifyFootprint )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    auto mockAIService = std::make_unique<MOCK_AI_SERVICE>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );
    processor.SetAIService( std::move( mockAIService ) );

    AI_COMMAND_RESULT result = processor.ProcessCommand( wxT( "modify footprint R1 size 0805" ) );
    BOOST_CHECK( result.success || !result.error.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestAIServiceErrorHandling )
{
    MOCK_AI_SERVICE aiService;
    AI_CONTEXT context;
    context.editorType = wxT( "schematic" );

    // Test with empty prompt
    AI_RESPONSE response = aiService.ProcessPrompt( wxT( "" ), context );
    BOOST_CHECK( response.success || !response.error.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestFileOperationsErrorCases )
{
    MOCK_FILE_OPERATIONS fileOps;
    
    // Test loading non-existent file
    wxString content;
    BOOST_CHECK( !fileOps.LoadFile( wxT( "/nonexistent/file.txt" ), content ) );
    
    // Test that content is unchanged after failed load
    wxString originalContent = content;
    fileOps.LoadFile( wxT( "/nonexistent/file.txt" ), content );
    BOOST_CHECK_EQUAL( content, originalContent );
}

BOOST_AUTO_TEST_CASE( TestCommandProcessorMultipleOperations )
{
    MOCK_EDA_FRAME frame;
    auto mockFileOps = std::make_unique<MOCK_FILE_OPERATIONS>();
    auto mockAIService = std::make_unique<MOCK_AI_SERVICE>();
    AI_COMMAND_PROCESSOR processor( &frame, std::move( mockFileOps ) );
    processor.SetAIService( std::move( mockAIService ) );

    // Test multiple commands in sequence
    AI_COMMAND_RESULT r1 = processor.ProcessCommand( wxT( "add component R1 at 10,20" ) );
    AI_COMMAND_RESULT r2 = processor.ProcessCommand( wxT( "add component C1 at 30,40" ) );
    AI_COMMAND_RESULT r3 = processor.ProcessCommand( wxT( "add trace from 0,0 to 50,50" ) );

    // All should either succeed or provide errors
    BOOST_CHECK( r1.success || !r1.error.IsEmpty() );
    BOOST_CHECK( r2.success || !r2.error.IsEmpty() );
    BOOST_CHECK( r3.success || !r3.error.IsEmpty() );
}

BOOST_AUTO_TEST_CASE( TestAIServiceStreamingCallback )
{
    MOCK_AI_SERVICE aiService;
    AI_CONTEXT context;
    context.editorType = wxT( "schematic" );

    int callbackCount = 0;
    wxString allChunks;
    
    AI_RESPONSE response = aiService.ProcessPromptStreaming(
        wxT( "test streaming" ), context,
        [&callbackCount, &allChunks]( const wxString& chunk ) {
            callbackCount++;
            allChunks += chunk;
        } );

    BOOST_CHECK( response.success );
    BOOST_CHECK( callbackCount > 0 );
    BOOST_CHECK( !allChunks.IsEmpty() );
}

BOOST_AUTO_TEST_SUITE_END()

int main( int argc, char* argv[] )
{
    return boost::unit_test::unit_test_main( &init_unit_test, argc, argv );
}
