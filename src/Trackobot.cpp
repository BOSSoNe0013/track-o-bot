#include "Trackobot.h"

#include <QDir>
#include <QStandardPaths>

#include <QLocalServer>
#include <QLocalSocket>

#include "Updater.h"
#include "Settings.h"

#if defined Q_OS_MAC
#include "CocoaInitializer.h"
#include "SparkleUpdater.h"
#elif defined Q_OS_WIN
#include "WinSparkleUpdater.h"
#elif defined defined Q_OS_LINUX
#include "SparkleUpdater.h"
#endif

#include "Hearthstone.h"
#include "WebProfile.h"

#include <cassert>

Updater *gUpdater = NULL;

Trackobot::Trackobot( int argc, char **argv )
  : mApp( argc, argv ),
    mCore( NULL ),
    mWindow( NULL ),
    mSingleInstanceServer( NULL )
{
  SetupApplication();
  }

Trackobot::~Trackobot() {
  if( mWindow ) {
    delete mWindow;
    mWindow = NULL;
  }

  if( mCore ) {
    delete mCore;
    mCore = NULL;
  }

  if( mSingleInstanceServer ) {
    delete mSingleInstanceServer;
    mSingleInstanceServer = NULL;
  }
}

int Trackobot::Run() {
  if( IsAlreadyRunning() )
    return 1;

  SetupLogging();

  LOG( "--> Launched v%s on %s", VERSION, qt2cstr( QDate::currentDate().toString( Qt::ISODate ) ) );

  SetupUpdater();

  CreateCore();
  CreateUI();

  Initialize();

  int exitCode = mApp.exec();

  // Tear down
  LOG( "<-- Shutdown" );

  return exitCode;
}

bool Trackobot::IsAlreadyRunning() {
  // Enforce single instance
  const char serverName[] = "trackobot";

  QLocalSocket socket;
  socket.connectToServer( serverName );
  if( socket.waitForConnected(500) ) {
    return true;
  }

  QLocalServer::removeServer( serverName );
  mSingleInstanceServer = new QLocalServer( NULL );
  if( !mSingleInstanceServer->listen(serverName) ) {
    return true;
  }

  return false;
}

void Trackobot::SetupApplication() {
#if defined Q_OS_MAC
  mApp.setAttribute( Qt::AA_UseHighDpiPixmaps );
  QIcon icon( ":/icons/mac_black.png" );
  icon.addFile( ":/icons/mac_black@2x.png" );
#elif defined Q_OS_WIN
  QIcon icon( ":/icons/win.ico" );
#elif defined Q_OS_LINUX
  QIcon icon = QIcon( ":/icons/Track-o-Bot.png" );
#endif
  mApp.setApplicationName( "Track-o-Bot" ); // for proper DataLocation handling
  mApp.setOrganizationName( "spidy.ch" );
  mApp.setOrganizationDomain( "spidy.ch" );
  mApp.setWindowIcon( icon );
}

void Trackobot::SetupLogging() {
  // Logging
  QString dataLocation = QStandardPaths::writableLocation( QStandardPaths::DataLocation );
  if( !QFile::exists(dataLocation) ) {
    QDir dir;
    dir.mkpath( dataLocation );
  }
  QString logFilePath = dataLocation + QDir::separator() + mApp.applicationName() + ".log";
  Logger::Instance()->SetLogPath( logFilePath );
}

void Trackobot::SetupUpdater() {
#if defined Q_OS_MAC
  CocoaInitializer cocoaInitializer;
gUpdater = new SparkleUpdater( mWebProfile.WebserviceURL( "/appcast.xml" ) );
#elif defined Q_OS_WIN
gUpdater = new WinSparkleUpdater( mWebProfile.WebserviceURL( "/appcast_win.xml" ) );
#elif defined Q_OS_LINUX
  gUpdater = NULL;
#endif
}

void Trackobot::CreateCore() {
  // Lo' and behold... it is...
  // the insurmountable...
  // the invincible...
  // the magnificent...
  // COREEEEEEE
  mCore = new Core();
}

void Trackobot::CreateUI() {
  mWindow = new Window();
}

void Trackobot::Initialize() {
  assert( mCore && mWindow );

  // Wire stuff
  connect( mCore, SIGNAL( HandleGameClientRestartRequired(bool) ),
      mWindow, SLOT( HandleGameClientRestartRequired(bool) ) );

  connect( mWindow, SIGNAL( OpenProfile() ),
      &mWebProfile, SLOT( OpenProfile() ) );

  // Make sure Account exists or create one
  mWebProfile.EnsureAccountIsSetUp();

  // Enable HS Logging
  Hearthstone::Instance()->EnableLogging();
}

