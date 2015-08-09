#pragma once

#include <QPixmap>
#include <QDir>

#include "WindowCapture.h"

class Hearthstone
{
  DEFINE_SINGLETON( Hearthstone );

private:
  WindowCapture *mCapture;

  bool mRestartRequired; // in case HS needs to be restarted for log changes to take effect

public:
  // Allow to override window capture for test environment
  void SetWindowCapture( WindowCapture *windowCapture );

  bool Running();
  QPixmap Capture( int canvasWidth, int canvasHeight, int cx, int cy, int cw, int ch  );

  void EnableLogging();
  void DisableLogging();

  void SetRestartRequired( bool restartRequired );
  bool RestartRequired() const;

  string LogConfigPath();
  string LogPath();

  int Width();
  int Height();

};
