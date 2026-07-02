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

/** Default guest session lifetime before auto-disconnect (minutes). */
#define HYBBX_DEFAULT_GUEST_TIMEOUT_MINUTES 30u

#endif /* HYBBX_LIMITS_H */
