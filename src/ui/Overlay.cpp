#include "Overlay.h"
#include "ui_Overlay.h"

#include "../Hearthstone.h"
#include "../Settings.h"

#ifdef Q_OS_MAC
#include <objc/objc-runtime.h>
#endif

#include <cassert>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QMouseEvent>
#include "../LinuxWindowCapture.h"
#include <X11/Xlib.h>

#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#define CHECK_FOR_OVERLAY_HOVER_INTERVAL_MS 100

class OverlayHistoryWindow {
private:
  QString mTitle;
  OverlayHistoryList mHistory;

  QFont mRowFont;
  QFont mTitleFont;

  int mWidth;

  int mPadding;
  int mRowSpacing;


  int TitleHeight() const {
    QFontMetrics titleMetrics( mTitleFont );
    return titleMetrics.ascent() - titleMetrics.descent();
  }

  int Padding() const {
    return mPadding;
  }

  int RowSpacing() const {
    return mRowSpacing;
  }

  int RowHeight() const {
    QFontMetrics rowMetrics( mRowFont );
    return rowMetrics.ascent() - rowMetrics.descent();
  }

  int RowWidth() const {
    return Width() - Padding() * 2;
  }

  void DrawMana( QPainter& painter, int x, int y, int width, int height, int mana ) const {
    // Draw mana
    QPen origPen = painter.pen();
    QPen pen( QColor( 0, 52, 113 ) );
    pen.setCosmetic( true );
    pen.setWidth( 1 );
    painter.setPen( pen );

    QBrush brush( QColor( 40, 119, 238 ) );
    painter.setBrush( brush );

    QTransform transform;
    painter.translate( x + width * 0.5, y + height * 0.5 );
    painter.scale( width * 0.8, height * 0.8 );

    static const QPointF points[5] = {
      QPointF( 0.0, -1.0 ),
      QPointF( 1.0, -0.2 ),
      QPointF( 0.6, 1.0 ),
      QPointF( -0.6, 1.0 ),
      QPointF( -1.0, -0.2 ),
    };
    painter.drawConvexPolygon( points, 5 );
    painter.resetTransform();
    painter.setPen( origPen );

    painter.drawText( x, y, width, height, Qt::AlignCenter | Qt::AlignVCenter, QString::number( mana ) );
  }

  void DrawCardLine( QPainter& painter, int x, int y, int width, int height, const QString& name, int count ) const {
    painter.save();
    painter.drawText( x, y, width, height, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, name );

    if( count > 1 ) {
      int nameWidth = QFontMetrics( painter.font() ).width( name + " " );
      QString countString = QString( "x%1" ).arg( count );
      QFont font = painter.font();
      font.setBold( true );
      painter.setFont( font );
      painter.drawText( x + nameWidth, y, width - nameWidth, height, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, countString );
    }
    painter.restore();
  }

public:
  OverlayHistoryWindow( const QString& title, const OverlayHistoryList& history, int width, int padding, int rowSpacing, float titleFontSize, float rowFontSize )
    : mTitle( title ), mHistory( history ), mWidth( width ), mPadding( padding ), mRowSpacing( rowSpacing )
  {
    mRowFont.setPointSize( titleFontSize );
    mTitleFont.setPointSize( rowFontSize );
    mTitleFont.setUnderline( true );
    mTitleFont.setBold( true );
  }

  int Width() const {
    return mWidth;
  }

  int Height() const {
    return ( mHistory.count() - 1 ) * RowSpacing() + // Spacing between items
      mHistory.count() * RowHeight() + // Height per item
      TitleHeight() + RowSpacing() + // Title
      Padding() * 2; // Overall padding
  }

  void Paint( QPainter& painter, int x, int y ) const {
    painter.save();

    QRect rect( x, y, Width(), Height() );
    painter.setClipRect( rect );

    // BG
    QPen pen = QPen( QColor( 160, 160, 160 ) );
    pen.setWidth( 3 );
    painter.setPen( pen );
    painter.setBrush( QBrush( QColor( 70, 70, 70, 175 ) ) );
    painter.drawRoundedRect( rect, 10, 10 );

    // Title
    y += Padding();
    painter.setPen( QPen( Qt::white) );
    painter.setFont( mTitleFont );
    painter.drawText( x, y, Width(), TitleHeight(), Qt::AlignCenter | Qt::AlignVCenter | Qt::TextDontClip, mTitle );
    y += TitleHeight() + RowSpacing();

    // Lines
    painter.setPen( QPen( Qt::white) );
    painter.setFont( mRowFont );
    for( const QVariantMap& it : mHistory ) {
      int mx = x + Padding();
      DrawMana( painter, mx, y, RowHeight(), RowHeight(), it["mana"].toInt() );
      int cx = mx + RowHeight() + 5;
      DrawCardLine( painter, cx, y, RowWidth() - cx, RowHeight(), it["name"].toString(), it["count"].toInt() );
      y += RowHeight();
      y += RowSpacing();
    }

    painter.restore();
  }

};

Overlay::Overlay( QWidget *parent )
  : QMainWindow( parent ), mUI( new Ui::Overlay ), mShowPlayerHistory( PLAYER_UNKNOWN )
{
  mUI->setupUi( this );
  setWindowFlags( Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput );

#ifdef Q_OS_WIN
  setWindowFlags( windowFlags() | Qt::Tool );
#elif defined Q_OS_LINUX
  setWindowFlags( windowFlags() | Qt::Window | Qt::X11BypassWindowManagerHint);
  setParent(0);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  //setAttribute(Qt::WA_PaintOnScreen);
#else
  setWindowFlags( windowFlags() | Qt::Window );
#endif

  setAttribute( Qt::WA_TranslucentBackground );
  setAttribute( Qt::WA_ShowWithoutActivating );

  connect( Hearthstone::Instance(), &Hearthstone::GameWindowChanged, this, &Overlay::HandleGameWindowChanged );
  connect( Hearthstone::Instance(), &Hearthstone::GameStarted, this, &Overlay::HandleGameStarted );
  connect( Hearthstone::Instance(), &Hearthstone::GameStopped, this, &Overlay::HandleGameStopped );
  connect( Hearthstone::Instance(), &Hearthstone::FocusChanged, this, &Overlay::HandleFocusChanged);

  connect( &mCheckForHoverTimer, &QTimer::timeout, this, &Overlay::CheckForHover );

  connect( Settings::Instance(), &Settings::OverlayEnabledChanged, this, &Overlay::HandleOverlaySettingChanged );

  LoadCards();

  hide();

#ifdef Q_OS_MAC
  WId windowObject = this->winId();
  objc_object* nsviewObject = reinterpret_cast<objc_object*>(windowObject);
  objc_object* nsWindowObject = objc_msgSend( nsviewObject, sel_registerName("window") );
  int NSWindowCollectionBehaviorCanJoinAllSpaces = 1 << 0;
  objc_msgSend( nsWindowObject, sel_registerName("setCollectionBehavior:"), NSWindowCollectionBehaviorCanJoinAllSpaces );

  // Ignore mouse events on Mac
  // Qt::WindowTransparentForInput bug
  // https://bugreports.qt.io/browse/QTBUG-45498
  objc_msgSend( nsWindowObject, sel_registerName("setIgnoresMouseEvents:"), 1 );
#endif
}

Overlay::~Overlay() {
  delete mUI;
}

void Overlay::CheckForHover() {
  QPoint mouseLoc = mapFromGlobal( QCursor::pos() );

  Player showPlayerHistory = PLAYER_UNKNOWN;

  if( mPlayerDeckRect.contains( mouseLoc ) ) {
    showPlayerHistory = PLAYER_SELF;
  } else if( mOpponentDeckRect.contains( mouseLoc ) ) {
    showPlayerHistory = PLAYER_OPPONENT;
  }

  if( mShowPlayerHistory != showPlayerHistory ) {
    mShowPlayerHistory = showPlayerHistory;
    update();
  }
}

void Overlay::LoadCards() {
  QFile file( ":/data/cards.json" );
  bool opened = file.open( QIODevice::ReadOnly | QIODevice::Text );
  assert( opened );

  QByteArray jsonData = file.readAll();
  QJsonParseError error;
  QJsonArray jsonCards = QJsonDocument::fromJson( jsonData, &error ).array();
  assert( error.error == QJsonParseError::NoError );

  mCardDB.clear();

  for( QJsonValueRef jsonCardRef : jsonCards ) {
    QJsonObject jsonCard = jsonCardRef.toObject();
    mCardDB[ jsonCard["id"].toString() ] = jsonCard.toVariantMap();
  }
}

void PaintHistoryInScreen( QPainter& painter, const OverlayHistoryWindow& wnd, const QPoint& pos ) {
  int padding = 10;

  QRect rect( pos.x() + 20, pos.y(), wnd.Width(), wnd.Height() );
  rect.translate( -qMax( rect.right() - painter.device()->width() + padding, 0 ), -qMax( rect.bottom() - painter.device()->height() + padding, 0 ) ); // fit to window
  wnd.Paint( painter, rect.x(), rect.y() );
}

void Overlay::paintEvent( QPaintEvent* ) {
  QString title;
  QRect rect;
  OverlayHistoryList *history = NULL;

  if( mShowPlayerHistory == PLAYER_SELF && mPlayerHistory.count() > 0 ) {
    title = "Cards drawn";
    history = &mPlayerHistory;
    rect = mPlayerDeckRect;
  } else if( mShowPlayerHistory == PLAYER_OPPONENT && mOpponentHistory.count() > 0 ) {
    title = "Cards played by opponent";
    history = &mOpponentHistory;
    rect = mOpponentDeckRect;
  }

  QPainter painter( this );
  painter.setRenderHint( QPainter::Antialiasing );

#ifdef Q_OS_WIN
  float rowFontSize = 9;
  float titleFontSize = 9;
#else
  float rowFontSize = 12;
  float titleFontSize = 12;
#endif

  int spacing = 8;
  int overlayWidth = 200;

  if( history ) {
    OverlayHistoryWindow wnd( title, *history, overlayWidth, spacing, spacing, titleFontSize, rowFontSize );
    QPoint pos = rect.center() + QPoint( rect.width() / 2 + 10, -wnd.Height() / 2 );
    PaintHistoryInScreen( painter, wnd, pos );
  }
}

void Overlay::HandleGameWindowChanged( int x, int y, int w, int h ) {
  move( x, y );
  setFixedSize( w, h );
  int minWidth = h * 4 / 3;
  mPlayerDeckRect = QRect( w / 2 + 0.440 * minWidth, h * 0.510, 0.05 * minWidth, h * 0.170 );
  mOpponentDeckRect = mPlayerDeckRect.translated( -0.005 * minWidth, -0.275 * h );

#ifdef Q_OS_LINUX
  QRect maskRect = QRect(mPlayerDeckRect.x(), 0, w - mPlayerDeckRect.x(), h - 100);
  QRegion region = QRegion(maskRect, QRegion::Rectangle);
  setMask(region);
#endif

  update();
}

void Overlay::Update() {
  if( Hearthstone::Instance()->GameRunning() && Settings::Instance()->OverlayEnabled() ) {
    show();
#ifdef Q_OS_WIN
    setAttribute( Qt::WA_QuitOnClose ); // otherwise taskkill /IM Track-o-Bot.exe does not work (http://www.qtcentre.org/threads/11713-Qt-Tool?p=62466#post62466)
#endif
  } else {
    hide();
  }

  update();
}

void Overlay::HandleGameStarted() {
  mCheckForHoverTimer.start( CHECK_FOR_OVERLAY_HOVER_INTERVAL_MS );
  Update();
}

void Overlay::HandleGameStopped() {
  mCheckForHoverTimer.stop();
  Update();
}

void Overlay::UpdateHistoryFor( Player player, const ::CardHistoryList& list ) {
  QMap< QString, QVariantMap > entries;

  for( const CardHistoryItem& it : list ) {
    const QString& cardId = it.cardId;

    if( cardId.isEmpty() ) {
      continue;
    }

    if( !mCardDB.contains( cardId ) ) {
      DBG( "Card %s not found", qt2cstr( cardId ) );
      continue;
    }

    if( mCardDB[ cardId ][ "type" ] == "hero" ) {
      continue;
    }

    if( it.player != player ) {
      continue;
    }

    QVariantMap& entry = entries[ cardId ];
    entry[ "count" ] = entry.value( "count", 0 ).toInt() + 1;
    entry[ "mana" ] = mCardDB[ cardId ][ "mana" ];
    entry[ "name" ] = mCardDB[ cardId ][ "name" ];
  }

  OverlayHistoryList* ref;
  if( player == PLAYER_SELF ) {
    ref = &mPlayerHistory;
  } else {
    ref = &mOpponentHistory;
  }

  *ref = entries.values();

  qSort( ref->begin(), ref->end(), []( const QVariantMap& a, const QVariantMap& b ) {
    if( a["mana"].toInt() == b["mana"].toInt() ) {
      return a["name"].toString() < b["name"].toString();
    } else {
      return a["mana"].toInt() < b["mana"].toInt();
    }
  });
}

void Overlay::HandleCardsDrawnUpdate( const ::CardHistoryList& cardsDrawn ) {
  UpdateHistoryFor( PLAYER_OPPONENT, cardsDrawn );
  UpdateHistoryFor( PLAYER_SELF, cardsDrawn );
  Update();
}

void Overlay::HandleOverlaySettingChanged( bool enabled ) {
  UNUSED_ARG( enabled );

  Update();
}

void Overlay::HandleFocusChanged(bool focus)
{
  if(focus)
    show();
  else
    hide();
  update();
}
