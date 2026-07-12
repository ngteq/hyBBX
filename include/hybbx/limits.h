#ifndef HYBBX_LIMITS_H
#define HYBBX_LIMITS_H

#include "hybbx/traffic.h"

/** INI parser line buffer size. */
#define HYBBX_CONFIG_LINE_MAX 512

/** Maximum path length for files and directories. */
#define HYBBX_PATH_MAX 512

/** Relative paths under the HyBBX install root (see @ref HYBBX_ENV_ROOT). */
#define HYBBX_DIR_DATA "data"
#define HYBBX_DIR_TEXT "text"
#define HYBBX_DIR_LOGS "logs"
#define HYBBX_FILE_CONFIG "hybbx.ini"
#define HYBBX_FILE_COMMANDS "commands.yaml"

/** Command registry (share/commands.yaml). */
#define HYBBX_COMMANDS_MAX             64u
#define HYBBX_COMMANDS_GROUP_MAX       8u
#define HYBBX_COMMANDS_VERBS_PER_GROUP 16u
#define HYBBX_COMMANDS_ALIASES_MAX     48u
#define HYBBX_COMMANDS_ALIAS_PER      8u
#define HYBBX_COMMANDS_MENU_LEVELS    5u
#define HYBBX_COMMANDS_MENU_BLOCKS    12u
#define HYBBX_COMMANDS_MENU_LAYOUTS   16u
#define HYBBX_COMMANDS_ALIAS_LINES    8u
#define HYBBX_COMMANDS_HELP_LINE_MAX  96u
#define HYBBX_COMMANDS_HEADER_MAX     128u

/** Environment variable: absolute path to the HyBBX install directory. */
#define HYBBX_ENV_ROOT "HYBBX_ROOT"

#define HYBBX_CONFIG_SECTION_MAX 128
#define HYBBX_CONFIG_KEY_MAX 128
#define HYBBX_CONFIG_VALUE_MAX 256

/** Maximum tokens per HyBBX command line (verb + args). */
#define HYBBX_CMD_TOKEN_MAX 16

#define HYBBX_CMD_VERB_MAX 32

/** Maximum registered transport plugins. */
#define HYBBX_MAX_PLUGINS 16

/** Maximum length of the global session prompt (empty = no visible prompt). */
#define HYBBX_PROMPT_MAX 32

/** Maximum single allocation for duplicated strings (bytes). */
#define HYBBX_ALLOC_MAX (256u * 1024u)

/** Default maximum simultaneous online sessions (guests included). */
#define HYBBX_DEFAULT_MAX_ONLINE 35u

/** Maximum concurrent HBX circuit links (secondaries) on Main. */
#define HYBBX_CIRCUIT_MAX_LINKS 16u

/** Default [circuit] max_links when not set in INI. */
#define HYBBX_CIRCUIT_DEFAULT_MAX_LINKS 8u

/** Maximum concurrent TNC devices in one packet_radio transport. */
#define HYBBX_PACKET_RADIO_MAX_INSTANCES 8u

/** Separates multiple INI sections in one packet_radio start config. */
#define HYBBX_PACKET_RADIO_INSTANCE_SEP '\x1e'

/** Maximum concurrent BayCom modems in one baycom transport. */
#define HYBBX_BAYCOM_MAX_INSTANCES 4u

/** Separates multiple INI sections in one baycom start config. */
#define HYBBX_BAYCOM_INSTANCE_SEP '\x1e'

/** Separates multiple INI sections in one mains_proxy start config. */
#define HYBBX_MAINS_PROXY_INSTANCE_SEP '\x1e'

/** Default guest session lifetime before auto-disconnect (minutes). */
#define HYBBX_DEFAULT_GUEST_TIMEOUT_MINUTES 30u

/** Command history depth for interactive line editors. */
#define HYBBX_HISTORY_MAX 25u

/** AX.25 auto-beacon minimum interval (seconds); INI may only increase this. */
#define HYBBX_BROADCAST_AX25_INTERVAL_MIN_SEC 600u

/** RF channel must be idle this long before an auto AX.25 beacon may TX. */
#define HYBBX_BROADCAST_AX25_BAND_IDLE_SEC 180u

/** Default max25d TCP listen port (MAX25 max25d.ini.example). */
#define HYBBX_MAX25_DEFAULT_PORT 7325u

/** Default TCP connect timeout for max25d reachability probe (ms). */
#define HYBBX_MAX25_PROBE_TIMEOUT_MS 3000u

/** AX.25 on-air payload cap (1200 baud; keep UI frames short). */
#define HYBBX_BROADCAST_AX25_MESSAGE_MAX 48u

/** SQLite fallback copy suffix (see `[storage] backup_path`). */
#define HYBBX_STORAGE_BACKUP_SUFFIX ".flb"

/** Default seconds between SQLite DB fallback copies. */
#define HYBBX_STORAGE_BACKUP_INTERVAL_DEFAULT_SEC 300u

/** [security] defaults — short cool-down bans (seconds). */
#define HYBBX_SECURITY_DEFAULT_MAXRETRY 5u
#define HYBBX_SECURITY_DEFAULT_FINDTIME_SEC 600u
#define HYBBX_SECURITY_DEFAULT_BANTIME_SEC 600u
/** Abuse (excessive spam/flood) — higher bar than login failures; no ban for normal use. */
#define HYBBX_SECURITY_DEFAULT_ABUSE_MAXRETRY 30u
#define HYBBX_SECURITY_DEFAULT_ABUSE_FINDTIME_SEC 600u
#define HYBBX_SECURITY_DEFAULT_RATE_LIMIT 30u
#define HYBBX_SECURITY_DEFAULT_RATE_WINDOW_SEC 60u
#define HYBBX_SECURITY_BAN_MAX 256u
#define HYBBX_SECURITY_TRACK_MAX 512u

/** AX.25 CALL-SSID or HBX link_id (same ban table). */
#define HYBBX_CALLID_MAX 64u

#endif /* HYBBX_LIMITS_H */
