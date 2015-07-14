#include <QScopedPointer>

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

#define DEFINE_SINGLETON(CLASSNAME) \
public: \
  ~CLASSNAME( void ); \
  static CLASSNAME* Instance( void ) { \
    if( !_singletonptr ) \
      _singletonptr.reset(new CLASSNAME()); \
    return _singletonptr.data(); \
  } \
  static void Reset() { \
    _singletonptr.reset(); \
  } \
private: \
  static QScopedPointer<CLASSNAME> _singletonptr; \
  CLASSNAME( void ); \
  CLASSNAME( const CLASSNAME& ); \

#define DEFINE_SINGLETON_SCOPE( CLASSNAME ) \
  QScopedPointer<CLASSNAME> CLASSNAME::_singletonptr;

typedef enum {
  PLAYER_SELF = 0,
  PLAYER_OPPONENT,
  PLAYER_UNKNOWN
} Player;

const char PLAYER_NAMES[][128] = {
  "self",
  "opponent",
  "unknown"
};

typedef enum {
  ORDER_FIRST = 0,
  ORDER_SECOND,
  ORDER_UNKNOWN
} GoingOrder;

const char ORDER_NAMES[][128] = {
  "first",
  "second",
  "unknown"
};

typedef enum {
  MODE_RANKED = 0,
  MODE_CASUAL,
  MODE_SOLO_ADVENTURES,
  MODE_ARENA,
  MODE_FRIENDLY,
  MODE_TAVERN_BRAWL,
  MODE_UNKNOWN
} GameMode;

const char MODE_NAMES[][128] = {
  "ranked",
  "casual",
  "solo",
  "arena",
  "friendly",
  "unknown"
};

typedef enum {
  OUTCOME_VICTORY = 0,
  OUTCOME_DEFEAT,
  OUTCOME_UNKNOWN
} Outcome;

const char OUTCOME_NAMES[][128] = {
  "victory",
  "defeat",
  "unknown"
};

typedef enum {
  CLASS_PRIEST = 0,
  CLASS_ROGUE,
  CLASS_MAGE,
  CLASS_PALADIN,
  CLASS_WARRIOR,
  CLASS_WARLOCK,
  CLASS_HUNTER,
  CLASS_SHAMAN,
  CLASS_DRUID,
  CLASS_UNKNOWN
} Class;

const char CLASS_NAMES[][128] = {
  "priest",
  "rogue",
  "mage",
  "paladin",
  "warrior",
  "warlock",
  "hunter",
  "shaman",
  "druid",
  "unknown"
};

#define NUM_CLASSES 9

class CardHistoryItem {
public:
  int turn;
  Player player;
  string cardId;
  int internalId;

  CardHistoryItem( int turn, Player player, const string& cardId, int internalId = 0 )
    : turn( turn ), player( player ), cardId( cardId ), internalId( internalId )
  {
  }
};
typedef vector< CardHistoryItem > CardHistoryList;


#include "Logger.h"
#define LOG(str, ...) Logger::Instance()->Add(str, ##__VA_ARGS__)

#ifdef _DEBUG
#define DEBUG(str, ...) Logger::Instance()->Add("[DEBUG] "str, ##__VA_ARGS__)
#else
#define DEBUG(str, ...)
#endif

