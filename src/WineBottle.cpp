#include <algorithm>
#include <QRegularExpression>
#include <QTextStream>
#include <QDirIterator>
#include "WineBottle.h"

#include "Local.h"  // error reporting

DEFINE_SINGLETON_SCOPE( WineBottle )


namespace {
/*
const int REG_NONE = 0;                 // no type
const int REG_SZ = 1;                 	// string type (ASCII)
const int REG_EXPAND_SZ = 2;	          // string, includes %ENVVAR% (expanded by caller) (ASCII)
const int REG_BINARY = 3;	              // binary format, callerspecific
// YES, REG_DWORD == REG_DWORD_LITTLE_ENDIAN
const int REG_DWORD = 4;	              // DWORD in little endian format
const int REG_DWORD_LITTLE_ENDIAN = 4;	// DWORD in little endian format
const int REG_DWORD_BIG_ENDIAN = 5;	    // DWORD in big endian format
const int REG_LINK = 6;	                // symbolic link (UNICODE)
const int REG_MULTI_SZ = 7;	            // multiple strings, delimited by \0, terminated by \0\0 (ASCII)
const int REG_RESOURCE_LIST = 8;	      // resource list? huh?
const int REG_FULL_RESOURCE_DESCRIPTOR= 9;	// full resource descriptor? huh?
const int REG_RESOURCE_REQUIREMENTS_LIST = 10;
const int REG_QWORD = 11;	              // QWORD in little endian format
const int REG_QWORD_LITTLE_ENDIAN =	11;	// QWORD in little endian format
*/
const int MAX_REG_FILES = 3;
const QString SYSTEM_REG( "system.reg" );
//const int SYSTEM_REG_ID = 0;
const QString USER_REG( "user.reg" );
//const int USER_REG_ID = 1;
const QString USERDEF_REG( "userdef.reg" );
//const int USERDEF_REG_ID = 2;

const QString REG_KEY_LOCAL_APPDATA( "HKEY_USERS/.Default/Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders/Local AppData" );
const QString REG_KEY_HS_INSTALL_LOCATION( "HKEY_LOCAL_MACHINE/Software/Microsoft/Windows/CurrentVersion/Uninstall/Hearthstone/InstallLocation" );
const QString REG_KEY_APPDATA( "HKEY_USERS/.Default/Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders/AppData" );

const QString DOSDEVICES( "dosdevices" );
const QString DRIVE_C( "drive_c" );

}

WineBottle::WineBottle()
  : mRegFiles ( QVector< QPointer< QSettings >>( ( QPointer< QSettings >( nullptr ), 3 ) ) ),
    mRegFormat( QSettings::registerFormat( "reg", ReadWineRegFile, WriteWineRegFile ) ) {
}

WineBottle::~WineBottle() {
  mRegFiles.clear();
}

void WineBottle::SetPath( const QString& path )
{
  mWinePrefix = path;
  //is valid?
  QStringList difference;
  const QStringList defaultContent = { DOSDEVICES, DRIVE_C, SYSTEM_REG, USER_REG, USERDEF_REG };
  QStringList dirContent = QDir( mWinePrefix ).entryList( QDir::NoDotAndDotDot |
                                                          QDir::Files | QDir::Dirs,
                                                          QDir::Name );
  // no need to sort since defaultContent and dirContent are alrady sorted
  std::set_difference( defaultContent.begin(), defaultContent.end(),
                       dirContent.begin(), dirContent.end(),
                       std::inserter( difference, difference.begin() ) );

  if ( !( mIsValid = difference.size() ? false : true ) ) {
    ERR( "Directory: %s is not a valid wine prefix!", qt2cstr( path ) );
  }
  SetupDosDevices();
  SetupRegFiles();
}

const QString& WineBottle::Path() const {
  return mWinePrefix;
}

QString WineBottle::DosDevicePath( const QChar& drive ) const {
  return mDosDevices[ drive.toLower() ];
}

bool WineBottle::IsValid() const {
  return mIsValid;
}

bool WineBottle::WriteWineRegFile( QIODevice& device, const QSettings::SettingsMap& map )
{
  Q_UNUSED( device );
  Q_UNUSED( map );
  Q_ASSERT_X( true, "WineBottle::WriteWineRegFile",
              "Dummy metod: this method is not implemented!" );
  return false;
}

void WineBottle::SetupRegFiles() {
  const QString reg_files[ MAX_REG_FILES ] = { SYSTEM_REG, USER_REG, USERDEF_REG };
  for( auto id = 0; id < MAX_REG_FILES; ++id ) {
    if ( !mRegFiles.at( id ) ) {
      mRegFiles[ id ] = new QSettings( mWinePrefix + "/" + reg_files[ id ], mRegFormat );
    }
  }
}

QVariant WineBottle::ReadRegistryValue( const QString& key ) const {
  const QString reg_files[ MAX_REG_FILES ] = { SYSTEM_REG, USER_REG, USERDEF_REG };
  for( auto id = 0; id < MAX_REG_FILES; ++id ) {
    if ( mRegFiles[ id ]->value( key ).isValid() ) {
      return mRegFiles[ id ]->value( key );
    }
  }
  return QVariant();
}

QString WineBottle::LocalAppData() const {
  return ToSystemPath( ReadRegistryValue( REG_KEY_LOCAL_APPDATA ).toString() );
}

QString WineBottle::HearthstoneInstallLocation() const {
  return ToSystemPath( ReadRegistryValue( REG_KEY_HS_INSTALL_LOCATION ).toString() );
}

QString WineBottle::AppData() const {
  return ToSystemPath( ReadRegistryValue( REG_KEY_APPDATA ).toString() );
}

QString WineBottle::ToSystemPath( const QString& windowsStylePath ) const {
  QRegularExpression driveReg( "^[a-zA-Z]:");

  if( driveReg.match( windowsStylePath ).hasMatch() ) {
    QString path = windowsStylePath;
    path.replace( "\\\\", QDir::separator() );
    return path.replace( driveReg, DosDevicePath( windowsStylePath.at( 0 ) ) );
  }
  return QString();
}

void WineBottle::SetupDosDevices() {
  QRegularExpression driveReg( "^[a-zA-Z]:$" );
  QDirIterator dirItr( mWinePrefix + "/" +  DOSDEVICES, QDirIterator::FollowSymlinks	);

  while( dirItr.hasNext() ) {
    dirItr.next();
    if ( driveReg.match( dirItr.fileName() ).hasMatch() &&
         dirItr.fileInfo().exists() ) {
      mDosDevices.insert( dirItr.fileName().at( 0 ), dirItr.fileInfo().symLinkTarget() );
    }
  }
}

bool WineBottle::ReadWineRegFile( QIODevice& device, QSettings::SettingsMap& map ) {
  if ( !device.isOpen() )
    return false;

  device.setTextModeEnabled( true );
  QTextStream inputStream( &device );

  // first line should be: WINE REGISTRY Version 2
  if ( inputStream.readLine() != "WINE REGISTRY Version 2" ) {
    return false;
  }
  // determine what prefix needs to be prepend

  QString line = inputStream.readLine();
  QString defaultPrefix, prefix;

  if ( line == ";; All keys relative to \\\\Machine" ) {
    // system.reg
    // ;; All keys relative to \\Machine
    defaultPrefix = "HKEY_LOCAL_MACHINE/";
  }
  else if ( line == ";; All keys relative to \\\\User\\\\.Default") {
    // userdef.reg
    defaultPrefix = "HKEY_USERS/.Default/";
  }
  else if ( line.left( 32 ) == ";; All keys relative to \\\\User\\\\") {
    // has to be user.reg since we cought userdef earlier
    // ;; All keys relative to \\User\\x-x-x-x-x-x-x-xxxx
    // resides in two places:
    // HKEY_CURRENT_USER/ and HKEY_USERS/User/x-x-x-x-x-x-x-xxxx
    // howerever we need only current user one
    defaultPrefix = "HKEY_CURRENT_USER/";
  }
  else {
    ERR( "%s: Couldn't determin prefix ", Q_FUNC_INFO );
    return false;
  }

  // now we fill SettingsMap
  QString key, valueString;
  QStringList explode;
  QVariant value;

  while ( !inputStream.atEnd() ) {
    line = inputStream.readLine();

    if ( line.startsWith( '#' ) ) {} 				/* skip comment */
    /* empty line means that next section is starting so prefix has to be cleared */
    else if ( line.startsWith( '\n' ) ) {
      prefix.clear();
    }
    else if ( line.startsWith( '[' ) ) { 		/* found new section */
      prefix = ( line.mid( 1, line.indexOf( "]" ) - 1 ) ).
               replace( "\\\\", QDir::separator() );
      prefix.prepend( defaultPrefix );
    }
    /* found key */
    //else if ( line.startsWith( '\' ) || line.startsWith( '@' ) )
    else if ( line.startsWith( '"' ) ) {
      explode = line.split( '=' );

      key =  explode[ 0 ].replace( "\"" , "" );
      // cannot replace " cuz we dont know the type
      valueString = explode[ 1 ];

      // REG_SZ
      if ( valueString.startsWith( '"' ) && valueString.endsWith( '"' )) {
        value.setValue( valueString.mid( 1, valueString.length() - 2 ));
      }
      // REG_DWORD
      else if ( valueString.startsWith( "dword:" )) {
        unsigned long dword = valueString.mid( 6 ).toULong();
        value.setValue( dword );
      }
      // REG_BINARY do i even need this?
      // TODO: finish this someday
      //			else if ( valueString.startsWith( "hex:")) {
      //			}
      //			// REG_MULTI_SZ */
      //			else if ( valueString.startsWith( "str(" ) ) {
      //			}
      //
      /* insert key and value to map */
      map [ prefix + "/" + key ] = value;
      key.clear();
      value.clear();
    } else {
      // unhandeled stuff we dont need
    }
  }
  return true;
}
