#include <QGuiApplication>
#include <QScreen>
#include <QX11Info>
#include "LinuxWindowCapture.h"
#include <xcb/xcb_icccm.h>

#define WM_WINDOW_INSTANCE "Hearthstone.exe"
#define WM_CLASS "Hearthstone.exe"

LinuxWindowCapture::LinuxWindowCapture()
  : mWindow( XCB_WINDOW_NONE ), mFocus( false )
{
}

bool LinuxWindowCapture::WindowFound() {
  if ( !mWindow )
    mWindow = FindWindow( WM_WINDOW_INSTANCE, WM_CLASS );

  if ( mWindow && !ExtractWindowProperties( mWindow, &mRect, &mFocus) )
    mWindow = XCB_WINDOW_NONE;

  return mWindow != XCB_WINDOW_NONE;
}

int LinuxWindowCapture::Width() {
  return mRect.width();
}

int LinuxWindowCapture::Height() {
  return mRect.height();
}

int LinuxWindowCapture::Left() {
  return mRect.x();
}

int LinuxWindowCapture::Top() {
  return mRect.y();
}

QPixmap LinuxWindowCapture::Capture( int x, int y, int w, int h ) {
    LOG("Capturing window: %d, %d, %d, %d", x, y, h, w );
    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow( mWindow, x, y, w, h );
  return pixmap;
}

QList< xcb_window_t > LinuxWindowCapture::listWindowsRecursive( const xcb_window_t& window ) {
  QList< xcb_window_t > windows;
  xcb_connection_t* xcbConn = QX11Info::connection();
  xcb_query_tree_cookie_t queryC = xcb_query_tree( xcbConn, window );
  CScopedPointer< xcb_query_tree_reply_t > queryR( xcb_query_tree_reply( xcbConn, queryC, nullptr ) );

  if ( queryR ) {
    xcb_window_t* children = xcb_query_tree_children( queryR.data() );
    for( auto c = 0; c < xcb_query_tree_children_length( queryR.data() ); ++c ) {
      windows << children[ c ];
      windows << listWindowsRecursive( children[ c ] );
    }
  }
  return windows;
}

bool LinuxWindowCapture::ExtractWindowProperties( xcb_window_t winId, QRect* winRect, bool* winFocus ) {
  if( !winId )
    return false;

  xcb_connection_t* xcbConn = QX11Info::connection();

  xcb_get_geometry_cookie_t geometryC = xcb_get_geometry( xcbConn, winId );
  CScopedPointer< xcb_get_geometry_reply_t > geometryR( xcb_get_geometry_reply( xcbConn, geometryC, nullptr ) );
  if( !geometryR ) return false;

  // assume the parent window is the screen then we can just use the coordinates directly
  int x = geometryR->x;
  int y = geometryR->y;

  CScopedPointer< xcb_query_tree_reply_t > treeR( xcb_query_tree_reply( xcbConn, xcb_query_tree( xcbConn, winId ), nullptr ) );
  if ( !treeR ) return false;

  // if the parent isn't screen translate coords
  if ( treeR->parent != QX11Info::appRootWindow() ) {
    xcb_translate_coordinates_cookie_t
                    translateC = xcb_translate_coordinates( xcbConn, winId, QX11Info::appRootWindow(), x, y );
    CScopedPointer< xcb_translate_coordinates_reply_t >
                    translateR( xcb_translate_coordinates_reply( xcbConn, translateC, nullptr ) );
    if ( !translateR ) return false;

    x = translateR->dst_x;
    y = translateR->dst_y;
  }
  winRect->setRect( x, y, geometryR->width, geometryR->height );

  xcb_get_input_focus_cookie_t focusC = xcb_get_input_focus( xcbConn );
  CScopedPointer < xcb_get_input_focus_reply_t > focusR( xcb_get_input_focus_reply( xcbConn, focusC, nullptr ) );
  if ( !focusR ) return false;

  *winFocus = ( focusR->focus == winId );
  return true;
}

xcb_window_t LinuxWindowCapture::FindWindow( const QString& instanceName, const QString& windowClass ) {
  xcb_window_t winID = XCB_WINDOW_NONE;
  QList< xcb_window_t > windows = listWindowsRecursive( QX11Info::appRootWindow() );
  xcb_connection_t* xcbConn = QX11Info::connection();

  foreach( const xcb_window_t& win, windows ) {

    xcb_icccm_get_wm_class_reply_t wmNameR;
    xcb_get_property_cookie_t wmClassC = xcb_icccm_get_wm_class( xcbConn, win );

    if ( xcb_icccm_get_wm_class_reply( xcbConn, wmClassC, &wmNameR, nullptr ) ) {
      if( !qstricmp( wmNameR.class_name, qt2cstr( windowClass ) ) ||
          !qstricmp( wmNameR.instance_name, qt2cstr ( instanceName ) ) ) {
        winID = win;
        break;
      }
    }
  }
  return winID;
}

bool LinuxWindowCapture::HasFocus() {
  return mFocus;
}
