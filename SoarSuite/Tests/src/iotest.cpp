#include <portability.h>

#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <vector>
#include <sstream>
#include <bitset>

#include "sml_Connection.h"
#include "sml_Client.h"
#include "sml_Utils.h"
#include "thread_Event.h"
#include "sml_Names.h"

enum eKernelOptions
{
	NONE,
	EMBEDDED,
	USE_CLIENT_THREAD,
	FULLY_OPTIMIZED,
	AUTO_COMMIT_ENABLED,
	NUM_KERNEL_OPTIONS,
};

typedef std::bitset<NUM_KERNEL_OPTIONS> KernelBitset;

class IOTest : public CPPUNIT_NS::TestCase
{
	CPPUNIT_TEST_SUITE( IOTest );	// The name of this class

	//CPPUNIT_TEST( testInputLeak );
	//CPPUNIT_TEST( testInputLeak2 );
	//CPPUNIT_TEST( testInputLeak3 );
	//CPPUNIT_TEST( testInputLeak4 );

	CPPUNIT_TEST_SUITE_END();

public:
	void setUp();		// Called before each function outlined by CPPUNIT_TEST
	void tearDown();	// Called after each function outlined by CPPUNIT_TEST

protected:
	void testInputLeak();  // only string
	void testInputLeak2(); // explicitly delete both
	void testInputLeak3(); // only delete identifier
	void testInputLeak4(); // do something with shared ids

	void createKernelAndAgents( const KernelBitset& options, int port = 12121 );

	sml::Kernel* pKernel;
	bool remote;
};

CPPUNIT_TEST_SUITE_REGISTRATION( IOTest ); // Registers the test so it will be used

void IOTest::setUp()
{
	pKernel = NULL;
	remote = false;

	// kernel initialized in test
}

void IOTest::tearDown()
{
	if ( !pKernel )
	{
		return;
	}

	// Agent deletion
	sml::Agent* pAgent = pKernel->GetAgent( "IOTest" ) ;
	CPPUNIT_ASSERT( pAgent != NULL );

	CPPUNIT_ASSERT( pKernel->DestroyAgent( pAgent ) );

	pKernel->Shutdown() ;

	// Delete the kernel.  If this is an embedded connection this destroys the kernel.
	// If it's a remote connection we just disconnect.
	delete pKernel ;
	pKernel = NULL;

	//if ( remote )
	//{
	//	cleanUpListener();
	//	if ( verbose ) std::cout << "Cleaned up listener." << std::endl;
	//}
}

void IOTest::createKernelAndAgents( const KernelBitset& options, int port )
{
	CPPUNIT_ASSERT( pKernel == NULL );

	if ( options.test( EMBEDDED ) )
	{
		CPPUNIT_ASSERT( !remote );
		if ( options.test( USE_CLIENT_THREAD ) )
		{
			pKernel = sml::Kernel::CreateKernelInCurrentThread( sml::Kernel::GetDefaultLibraryName(), options.test( FULLY_OPTIMIZED ), sml::Kernel::GetDefaultPort());
		}
		else
		{
			pKernel = sml::Kernel::CreateKernelInNewThread("SoarKernelSML", sml::Kernel::GetDefaultPort());
		}
	}
	else
	{
		CPPUNIT_ASSERT( remote );
		pKernel = sml::Kernel::CreateRemoteConnection(true, 0, port);
	}

	CPPUNIT_ASSERT( pKernel != NULL );
	CPPUNIT_ASSERT_MESSAGE( pKernel->GetLastErrorDescription(), !pKernel->HadError() );

	pKernel->SetAutoCommit( options.test( AUTO_COMMIT_ENABLED ) ) ;

	// Set this to true to give us lots of extra debug information on remote clients
	// (useful in a test app like this).
    // pKernel->SetTraceCommunications(true) ;

	CPPUNIT_ASSERT( std::string( pKernel->GetSoarKernelVersion() ) == std::string( sml::sml_Names::kSoarVersionValue ) );

	// Report the number of agents (always 0 unless this is a remote connection to a CLI or some such)
	CPPUNIT_ASSERT( pKernel->GetNumberAgents() == 0 );

	// NOTE: We don't delete the agent pointer.  It's owned by the kernel
	sml::Agent* pAgent = pKernel->CreateAgent( "IOTest" ) ;
	CPPUNIT_ASSERT_MESSAGE( pKernel->GetLastErrorDescription(), !pKernel->HadError() );
	CPPUNIT_ASSERT( pAgent != NULL );

	// a number of tests below depend on running full decision cycles.
	pAgent->ExecuteCommandLine( "set-stop-phase --before --input" ) ;
	CPPUNIT_ASSERT_MESSAGE( "set-stop-phase --before --input", pAgent->GetLastCommandLineResult() );

	pAgent->ExecuteCommandLine( "watch 0" ) ;
	pAgent->ExecuteCommandLine( "waitsnc --on" ) ;

	CPPUNIT_ASSERT( pKernel->GetNumberAgents() == 1 );
}

void IOTest::testInputLeak()
{
	KernelBitset options(0);
	options.set( EMBEDDED );
	options.set( USE_CLIENT_THREAD );
	options.set( FULLY_OPTIMIZED );
	options.set( AUTO_COMMIT_ENABLED );
	createKernelAndAgents( options );

	sml::Agent* pAgent = pKernel->GetAgent( "IOTest" ) ;
	CPPUNIT_ASSERT( pAgent != 0 );

	sml::Identifier* pInputLink = pAgent->GetInputLink();
	sml::StringElement* pFooBar = 0;

	//_CrtMemState memState;

	//_CrtMemCheckpoint( &memState );
	//_CrtSetBreakAlloc( 2406 );
	for ( int count = 0; count < 50000; ++count )
	{
		if ( count % 2 == 0 )
		{
			// even case

			// creating the wme
			CPPUNIT_ASSERT( pFooBar == 0 );
			
			pFooBar = pAgent->CreateStringWME(pInputLink, "foo", "bar");
			CPPUNIT_ASSERT( pFooBar != 0 );

			pKernel->RunAllAgents(1);
		} 
		else
		{
			// odd case
			// deleting the wme
			CPPUNIT_ASSERT( pFooBar != 0 );
			pAgent->DestroyWME( pFooBar );

			pFooBar = 0;

			pKernel->RunAllAgents(1);

			//_CrtMemDumpAllObjectsSince( &memState );
		}
	}
}

void IOTest::testInputLeak2()
{
	KernelBitset options(0);
	options.set( EMBEDDED );
	options.set( USE_CLIENT_THREAD );
	options.set( FULLY_OPTIMIZED );
	options.set( AUTO_COMMIT_ENABLED );
	createKernelAndAgents( options );

	sml::Agent* pAgent = pKernel->GetAgent( "IOTest" ) ;
	CPPUNIT_ASSERT( pAgent != 0 );

	sml::Identifier* pInputLink = pAgent->GetInputLink();
	sml::Identifier* pIdentifier = 0;
	sml::StringElement* pFooBar = 0;

	//_CrtMemState memState;

	//_CrtMemCheckpoint( &memState );
	//_CrtSetBreakAlloc( 4951 );
	for ( int count = 0; count < 50000; ++count )
	{
		if ( count % 2 == 0 )
		{
			// even case

			// creating the wme
			CPPUNIT_ASSERT( pIdentifier == 0 );
			CPPUNIT_ASSERT( pFooBar == 0 );
			
			pIdentifier = pAgent->CreateIdWME(pInputLink, "alpha");
			CPPUNIT_ASSERT( pIdentifier != 0 );

			pFooBar = pAgent->CreateStringWME(pIdentifier, "foo", "bar");
			CPPUNIT_ASSERT( pFooBar != 0 );

			pKernel->RunAllAgents(1);
		} 
		else
		{
			// odd case
			// deleting the wme
			CPPUNIT_ASSERT( pFooBar != 0 );
			pAgent->DestroyWME( pFooBar );

			CPPUNIT_ASSERT( pIdentifier != 0 );
			pAgent->DestroyWME( pIdentifier );

			pIdentifier = 0;
			pFooBar = 0;

			pKernel->RunAllAgents(1);

			//_CrtMemDumpAllObjectsSince( &memState );
		}
	}
}

void IOTest::testInputLeak3()
{
	KernelBitset options(0);
	options.set( EMBEDDED );
	options.set( USE_CLIENT_THREAD );
	options.set( FULLY_OPTIMIZED );
	options.set( AUTO_COMMIT_ENABLED );
	createKernelAndAgents( options );

	sml::Agent* pAgent = pKernel->GetAgent( "IOTest" ) ;
	CPPUNIT_ASSERT( pAgent != 0 );

	sml::Identifier* pInputLink = pAgent->GetInputLink();
	sml::Identifier* pIdentifier = 0;
	sml::StringElement* pFooBar = 0;

	//_CrtMemState memState;

	//_CrtMemCheckpoint( &memState );
	//_CrtSetBreakAlloc( 2451 );
	for ( int count = 0; count < 50000; ++count )
	{
		if ( count % 2 == 0 )
		{
			// even case

			// creating the wme
			CPPUNIT_ASSERT( pIdentifier == 0 );
			CPPUNIT_ASSERT( pFooBar == 0 );
			
			pIdentifier = pAgent->CreateIdWME(pInputLink, "alpha");
			CPPUNIT_ASSERT( pIdentifier != 0 );

			pFooBar = pAgent->CreateStringWME(pIdentifier, "foo", "bar");
			CPPUNIT_ASSERT( pFooBar != 0 );

			pKernel->RunAllAgents(1);
		} 
		else
		{
			// odd case
			// deleting the wme
			CPPUNIT_ASSERT( pIdentifier != 0 );
			pAgent->DestroyWME( pIdentifier );

			pIdentifier = 0;
			pFooBar = 0;

			pKernel->RunAllAgents(1);

			//_CrtMemDumpAllObjectsSince( &memState );
		}
	}
}

void IOTest::testInputLeak4()
{
	KernelBitset options(0);
	options.set( EMBEDDED );
	options.set( USE_CLIENT_THREAD );
	options.set( FULLY_OPTIMIZED );
	options.set( AUTO_COMMIT_ENABLED );
	createKernelAndAgents( options );

	sml::Agent* pAgent = pKernel->GetAgent( "IOTest" ) ;
	CPPUNIT_ASSERT( pAgent != 0 );

	sml::Identifier* pInputLink = pAgent->GetInputLink();
	sml::Identifier* pIdentifier = 0;
	sml::StringElement* pFooBar = 0;
	sml::Identifier* pSharedIdentifier = 0;

	//_CrtMemState memState;

	//_CrtMemCheckpoint( &memState );
	//_CrtSetBreakAlloc( 2451 );
	for ( int count = 0; count < 50000; ++count )
	{
		if ( count % 2 == 0 )
		{
			// even case

			// creating the wme
			CPPUNIT_ASSERT( pIdentifier == 0 );
			CPPUNIT_ASSERT( pFooBar == 0 );
			CPPUNIT_ASSERT( pSharedIdentifier == 0 );
			
			pIdentifier = pAgent->CreateIdWME(pInputLink, "alpha");
			CPPUNIT_ASSERT( pIdentifier != 0 );

			pFooBar = pAgent->CreateStringWME(pIdentifier, "foo", "bar");
			CPPUNIT_ASSERT( pFooBar != 0 );

			pSharedIdentifier = pAgent->CreateSharedIdWME(pInputLink, "alpha", pIdentifier);
			CPPUNIT_ASSERT( pSharedIdentifier != 0 );

			pKernel->RunAllAgents(1);
		} 
		else
		{
			// odd case
			// deleting the wme
			CPPUNIT_ASSERT( pIdentifier != 0 );
			pAgent->DestroyWME( pIdentifier );

			CPPUNIT_ASSERT( pSharedIdentifier != 0 );
			pAgent->DestroyWME( pSharedIdentifier );

			pIdentifier = 0;
			pFooBar = 0;
			pSharedIdentifier = 0;

			pKernel->RunAllAgents(1);

			//_CrtMemDumpAllObjectsSince( &memState );
		}
	}
}