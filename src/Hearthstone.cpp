#include "Hearthstone.h"

#include <QFile>
#include <QDesktopServices>
#include <QSettings>
#include <QTextStream>
#include <QRegExp>

#include <QJsonDocument>
#include <QJsonObject>

#include <QDirIterator>
#include <QDateTime>

#ifdef Q_OS_MAC
#include "OSXWindowCapture.h"
#elif defined Q_OS_WIN
#include "WinWindowCapture.h"
#include "Shlobj.h"
#elif defined Q_OS_LINUX
#include <math.h>
#include "LinuxWindowCapture.h"
#include "Settings.h"
#include "WineBottle.h"
#include "PeVersionExtractor.h"
#endif

#include "Settings.h"

DEFINE_SINGLETON_SCOPE( Hearthstone );

Hearthstone::Hearthstone()
 : mCapture( NULL ), mGameRunning( false ), mGameHasFocus( false ), mBuild( 0 )
{
#ifdef Q_OS_MAC
    LOG("OS X host");
  mCapture = new OSXWindowCapture();
#elif defined Q_OS_WIN
    LOG("Windows host");
  mCapture = new WinWindowCapture();
#elif defined Q_OS_LINUX
 LOG("GNU/Linux host");
 mCapture = new LinuxWindowCapture();
#endif

  // On OS X, WindowFound is quite CPU intensive
  // Starting time for HS is also long
  // So just check only once in a while
  mTimer = new QTimer( this );
  connect( mTimer, &QTimer::timeout, this, &Hearthstone::Update );
  connect( this, &Hearthstone::GameStarted, this, &Hearthstone::DetectBuild );
#ifdef Q_OS_LINUX
  connect( this, &Hearthstone::GameStarted, this, &Hearthstone::SetFastUpdates );
  connect( this, &Hearthstone::GameStopped, this, &Hearthstone::SetSlowUpdates );
  mTimer->start( 5000 );
#elif defined Q_OS_MAC
  mTimer->start( 5000 );
#else
  mTimer->start( 250 );
#endif
}

Hearthstone::~Hearthstone() {
  if( mCapture != NULL )
    delete mCapture;
}

void Hearthstone::Update() {
  bool isRunning = mCapture->WindowFound();

  if( isRunning ) {
    bool hasFocus = mCapture->HasFocus();
    if( mGameHasFocus != hasFocus ) {
      mGameHasFocus = hasFocus;
      emit FocusChanged( hasFocus );
    }

    static int lastLeft = 0, lastTop = 0, lastWidth = 0, lastHeight = 0;
    if( lastLeft != mCapture->Left() || lastTop != mCapture->Top() ||
        lastWidth != mCapture->Width() || lastHeight != mCapture->Height() )
    {
      lastLeft = mCapture->Left(),
               lastTop = mCapture->Top(),
               lastWidth = mCapture->Width(),
               lastHeight = mCapture->Height();

      DBG( "HS window changed %d %d %d %d", lastLeft, lastTop, lastWidth, lastHeight );
      emit GameWindowChanged( lastLeft, lastTop, lastWidth, lastHeight );
    }
  }

  if( mGameRunning != isRunning ) {
    mGameRunning = isRunning;

    if( isRunning ) {
      LOG( "Hearthstone is running" );
      emit GameStarted();
    } else {
      LOG( "Hearthstone stopped" );
      emit GameStopped();
    }
  }
}

QString Hearthstone::ReadAgentAttribute( const char *attributeName ) const {
    LOG("HS ReadAgentAttribute: %s", attributeName);
#ifdef Q_OS_MAC
  QString path = "/Users/Shared/Battle.net/Agent/agent.db";
#elif defined Q_OS_WIN
  wchar_t buffer[ MAX_PATH ];
  SHGetSpecialFolderPathW( NULL, buffer, CSIDL_COMMON_APPDATA, FALSE );
  QString programData = QString::fromWCharArray( buffer );
  QString path = programData + "\\Battle.net\\Agent\\agent.db";
#elif defined Q_OS_LINUX
  QString homeLocation = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
  QString path = homeLocation + "/.Hearthstone/agent.db";
  if(QFile(path).exists()){
    LOG("HS Agent DB file (agent.db): FOUND");
  }
  else{
      LOG("HS Agent DB file (agent.db): NOT FOUND");
  }
#endif

  QFile file( path );
  if( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    DBG( "Couldn't open %s (%d)", qt2cstr( path ), file.error() );
    return "";
  }

  QString contents = file.readAll();
  QJsonDocument doc = QJsonDocument::fromJson( contents.toUtf8() );
  QJsonObject root = doc.object();

  QJsonObject hs = root["/game/hs_beta"].toObject()["resource"].toObject()["game"].toObject();
  return hs[ QString( attributeName ) ].toString();
}

bool Hearthstone::GameRunning() const {
  return mGameRunning;
}

#ifdef Q_OS_WIN
inline float roundf( float x ) {
   return x >= 0.0f ? floorf( x + 0.5f ) : ceilf( x - 0.5f );
}
#endif

bool Hearthstone::CaptureWholeScreen( QPixmap *screen ) {
  *screen = mCapture->Capture( 0, 0, Width(), Height() );
  return true;
}

QPixmap Hearthstone::Capture( int canvasWidth, int canvasHeight, int cx, int cy, int cw, int ch )
{
  UNUSED_ARG( canvasWidth );

  int x, y, w, h;

  int windowHeight = mCapture->Height();
  float scale = windowHeight / float( canvasHeight );

  x = roundf( cx * scale );
  y = roundf( cy * scale );

  w = roundf( cw * scale );
  h = roundf( ch * scale );

  return mCapture->Capture( x, y, w, h );
}

void Hearthstone::SetWindowCapture( WindowCapture *windowCapture ) {
  if( mCapture != NULL )
    delete mCapture;

  mCapture = windowCapture;
}

void Hearthstone::EnableLogging() {
  QString path = LogConfigPath();
  QFile file( path );

  bool logModified = false;

  // Read file contents
  QString contents;
  if( file.exists() ) {
    file.open( QIODevice::ReadOnly | QIODevice::Text );
    QTextStream in( &file );
    contents = in.readAll();
    file.close();
  }


  // Check what modules we have to activate
  QStringList modulesToActivate;
  for( int i = 0; i < NUM_LOG_MODULES; i++ ) {
    const char *moduleName = LOG_MODULE_NAMES[ i ];
    QString moduleLine = QString( "[%1]" ).arg( moduleName ) ;
    if( !contents.contains( moduleLine ) ) {
      contents += "\n";
      contents += moduleLine + "\n";
      contents += "LogLevel=1\n";
      contents += "FilePrinting=true\n";

      DBG( "Activate module %s", moduleName );
      logModified = true;
    }
  }

  QRegExp regexEnabledConsolePrinting( "ConsolePrinting\\s*=\\s*true",
      Qt::CaseInsensitive );
  QRegExp regexDisabledFilePrinting( "FilePrinting\\s*=\\s*false",
      Qt::CaseInsensitive );
  if( contents.contains( regexEnabledConsolePrinting ) ||
      contents.contains( regexDisabledFilePrinting ) )
  {
    contents.replace( regexEnabledConsolePrinting, "FilePrinting=true" );
    contents.replace( regexDisabledFilePrinting, "FilePrinting=true" );

    DBG( "FilePrinting enabled" );
    logModified = true;
  }

  // Finally write updated log.config
  if( logModified ) {
    DBG( "Log modified. Write new version" );

    if( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
      ERR( "Couldn't create file %s", qt2cstr( path ) );
    } else {
      QTextStream out( &file );
      out << contents;
    }
  }

  // Notify about restart if game is running
  Update();
  if( GameRunning() && logModified ) {
    emit GameRequiresRestart();
  }
}

void Hearthstone::DisableLogging() {
  QFile file( LogConfigPath() );
  if( file.exists() ) {
    file.remove();
    LOG( "Ingame log deactivated." );
  }
}

QString Hearthstone::LogConfigPath() const {
#ifdef Q_OS_MAC
  QString homeLocation = QStandardPaths::standardLocations( QStandardPaths::HomeLocation ).first();
  QString configPath = homeLocation + "/Library/Preferences/Blizzard/Hearthstone/log.config";
#elif defined Q_OS_WIN
  wchar_t buffer[ MAX_PATH ];
  SHGetSpecialFolderPathW( NULL, buffer, CSIDL_LOCAL_APPDATA, FALSE );
  QString localAppData = QString::fromWCharArray( buffer );
#elif defined Q_OS_LINUX
  QString localAppData;
  if ( !Wine->IsValid() )
    LOG( "Invalid wine prefix. Cannot determine path for log.config" );
  else {
    QString path = Wine->ReadRegistryValue( REG_KEY_LOCAL_APPDATA ).toString();
    localAppData = Wine->ToSystemPath( path );
  }
#endif
  QString configPath = localAppData + "/Blizzard/Hearthstone/log.config";
  return configPath;
}

QString Hearthstone::DetectHearthstonePath() const {
  static QString hsPath;

#ifdef Q_OS_LINUX
  if( hsPath.isEmpty() ) {
      QString path = Wine->ReadRegistryValue( REG_KEY_HS_INSTALL_LOCATION ).toString();
      if ( path.isEmpty() ) {
          ERR( "Game folder not found (%s). Is the wine prefix configured correctly?", qt2cstr( path ) );
      }
      else {
        hsPath = Wine->ToSystemPath( path );
      }
  }
#elif defined Q_OS_WIN
    if( hsPath.isEmpty() ) {
        QString hsPathByAgent = ReadAgentAttribute( "install_dir" );
        QSettings hsKey( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Hearthstone", QSettings::NativeFormat );
        QString hsPathByRegistry = hsKey.value( "InstallLocation" ).toString();

        if( hsPathByAgent.isEmpty() && hsPathByRegistry.isEmpty() ) {
            LOG( "Game folder not found. Fall back to default game path for now. You should set the path manually in the settings!" );
            hsPath = QString( getenv("PROGRAMFILES") ) + "/Hearthstone";
        } else if( !hsPathByRegistry.isEmpty() ) {
          hsPath = hsPathByRegistry;
        } else {
          hsPath = hsPathByAgent;
        }

        LOG( "Use Hearthstone location %s", qt2cstr( hsPath ) );
    }
#elif defined Q_OS_MAC
    if( hsPath.isEmpty() ) {

      QStringList dirs = QStandardPaths::standardLocations( QStandardPaths::ApplicationsLocation );
      foreach( const QString& dir, dirs ) {
        hsPath = dir + "/Hearthstone";
        if( QDir( hsPath ).exists() ) {
          break;
        }
      }

      LOG( "Auto-detected %s as game path. Make sure it's the right one!", qt2cstr( hsPath ) );
    }
#endif
  return hsPath;
}

QString Hearthstone::DetectRegion() const {
  QString region = "";

#ifdef Q_OS_MAC
  QString homeLocation = QStandardPaths::standardLocations( QStandardPaths::HomeLocation ).first();
  QString path = homeLocation + "/Library/Application Support/Battle.net/";
#elif defined Q_OS_WIN
  wchar_t buffer[ MAX_PATH ];
  SHGetSpecialFolderPathW( NULL, buffer, CSIDL_APPDATA, FALSE );
  QString localAppData = QString::fromWCharArray( buffer );
  QString path = localAppData + "/Battle.net/";
#elif defined Q_OS_LINUX
  if ( !Wine->IsValid() ) {
    DBG( "Cannot detect region. Wine prefix is not valid!" );
    return region;
  }
  QString path = Wine->ToSystemPath( Wine->ReadRegistryValue( REG_KEY_APPDATA ).toString() )
                 .append( "/Battle.net/" );
#endif

  QDirIterator it( path, QStringList() << "*.config" );
  uint lastModifiedTime = 0;
  QString lastModifiedPath;
  while( it.hasNext() ) {
    it.next();

#ifdef Q_OS_LINUX
    /* it seems that bnet updates both battle.net.config and game config at
     * "the same time" and half of the time battle.net.config is the last one to
     * be updated and we dont want to read it.
     */
    if ( it.fileName() == "Battle.net.config" )
      continue;
#endif

    uint modifiedTime = it.fileInfo().lastModified().toTime_t();
    if( modifiedTime > lastModifiedTime ) {
      lastModifiedPath = it.fileInfo().absoluteFilePath();
      lastModifiedTime = modifiedTime;
    }
  }

  if( !lastModifiedPath.isEmpty() ) {
    QFile file(lastModifiedPath);
    LOG( "File %s", qt2cstr( lastModifiedPath ) );
    if( file.open( QIODevice::ReadOnly ) ) {
      QByteArray data = file.readAll();
      QJsonDocument doc = QJsonDocument::fromJson( data );
      QJsonObject root = doc.object();

      region = root["User"].toObject()
        ["Client"].toObject()
        ["PlayScreen"].toObject()
        ["GameFamily"].toObject()
        ["WTCG"].toObject()
        ["LastSelectedGameRegion"].toString();
    } else {
      LOG( "Couldn't open config file %s", qt2cstr( lastModifiedPath ) );
    }
  }

  return region;
}

int Hearthstone::Build() const {
  return mBuild;
}

#ifdef Q_OS_WIN
// http://stackoverflow.com/questions/940707/how-do-i-programatically-get-the-version-of-a-dll-or-exe-file
int Win32ExtractBuildFromPE( const wchar_t *szVersionFile ) {
  int build        = 0;
  DWORD  verHandle = NULL;
  UINT   size      = 0;
  LPBYTE lpBuffer  = NULL;
  DWORD  verSize   = GetFileVersionInfoSize( szVersionFile, &verHandle);

  if( verSize != NULL ) {
    LPSTR verData = new char[ verSize ];

    if( GetFileVersionInfo( szVersionFile, verHandle, verSize, verData) ) {
      if( VerQueryValue( verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size ) ) {
        if( size ) {
          VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;

          if( verInfo->dwSignature == 0xfeef04bd ) {
            build = ( verInfo->dwFileVersionLS >>  0 ) & 0xffff;
          }
        }
      }
    }

    delete[] verData;
  } else {
    DBG( "Could not extract build from PE: %s", szVersionFile );
  }

  return build;
}
#endif

void Hearthstone::DetectBuild() {
  QString hsPath = Settings::Instance()->HearthstoneDirectoryPath();
  QString buildPath;

  if( !hsPath.isEmpty() ) {
#ifdef Q_OS_MAC
    buildPath = QString( "%1/Hearthstone.app/Contents/Info.plist" ).arg( hsPath );
    QSettings settings( buildPath, QSettings::NativeFormat );
    QString version = settings.value( "BlizzardFileVersion", "1.0.0.0" ).toString();

    if( !version.isEmpty() ) {
      mBuild = version.split(".").last().toInt();
    }
#elif defined Q_OS_WIN
    buildPath = QString( "%1/Hearthstone.exe" ).arg( hsPath );
      mBuild = Win32ExtractBuildFromPE( buildPath.toStdWString().c_str() );
#elif defined Q_OS_LINUX
    mBuild = PeVersionExtractor::Extract( QString( "%1/Hearthstone.exe" ).arg( hsPath ) );
#endif
  }
}

QString Hearthstone::DetectLocale() const {
  QString locale = "enUS";

  QString path = QString( "%1/Launcher.db" ).arg( Settings::Instance()->HearthstoneDirectoryPath() );
  QFile file( path );

  if( file.open( QIODevice::ReadOnly ) )  {
    QTextStream stream( &file );
    locale = stream.readLine();
  } else {
    DBG( "Couldn't open %s to determine locale", qt2cstr( path ) );
  }

  return locale;
}


int Hearthstone::Width() const {
  return mCapture->Width();
}

int Hearthstone::Height() const {
  return mCapture->Height();
}

#ifdef Q_OS_LINUX
QString Hearthstone::DetectWinePrefixPath() const
{
  static QString winePrefix;

  if ( winePrefix.isEmpty() ) {
    // default prefix
    Wine->SetPath( QDir::homePath() + "/.wine" );
    if ( Wine->IsValid() )
      winePrefix = Wine->Path();
    else
      ERR( "Couldn't detect Wine prefix path!" );
  }
  return winePrefix;
}
#endif

bool Hearthstone::HasFocus() const {
  return mGameHasFocus;
}
