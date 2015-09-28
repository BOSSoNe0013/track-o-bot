# Build Dependencies

* Qt 5.5
* (OS X) Sparkle
* (Windows) WinSparkle

## Mac OS X

```
brew install qt5
brew link qt5 --force
```

* Install [Sparkle](http://sparkle.andymatuschak.org/) 
 * Move _Sparkle.framework_ to ``/Library/Frameworks``.

## Windows

* Install [Qt](http://qt-project.org/downloads) 
 * I use the Qt libraries 5.5.0 for Windows VS 2012.
* Install [WinSparkle](https://github.com/vslavik/winsparkle) 
 * Clone the repository and build the library, for example with VS 2012. The precompiled releases are ancient.
* Install libwebm and libvpx

## GNU/Linux

* Install Qt5.5+ with your distribution package manager (apt, etc...)
* Create folder .Hearthstone in your home directory
* Create symlinks of the Hearthstone's output_log ("WINEPREFIX/drive_c/Program Files/Hearthstone/Hearthstone_Data/") and log.config ("WINEPREFIX/drive_c/users/WINEUSER/Local Settings/Application Data/Blizzard/Hearthstone/", create file if not exists) in the newly created folder

# Build Instructions

```
qmake
make
```

The resulting binary can be found in the ``build`` subfolder.

# Install Instructions

```
sudo make install
```

Default install prefix is /usr/local. You can change it by adding PREFIX argument to qmake command eg:

```
qmake PREFIX=/usr
```

# Contributing

Feel free to submit pull requests, suggest new ideas and discuss issues. Track-o-Bot is about simplicity and usability. Only features which benefit all users will be considered. 

# License

GNU Lesser General Public License (LGPL) Version 2.1.

See [LICENSE](LICENSE).

