#include "hybbx/storage.h"
#include "storage_private.h"

#include <stdio.h>

hybbx_result_t hybbx_storage_sql_open(hybbx_storage_t *storage)
{
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    fprintf(stderr,
            "[storage] SQL backend (%d) is not implemented yet; "
            "use backend=flatfile for now\n",
            (int)storage->backend);
    return HYBBX_ERR_UNSUPPORTED;
}

void hybbx_storage_sql_close(hybbx_storage_t *storage)
{
    (void)storage;
}
