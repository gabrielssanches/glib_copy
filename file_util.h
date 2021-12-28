#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#define G_LOG_USE_STRUCTURED
#include <glib.h>
#include <gio/gio.h>

typedef struct fileutil_file_prop_st {
    gchar *name;            // file name
    gchar *path;            // absolute path
    gchar *path_relative;   // relative path
    gchar *target;          // symlink target
    gint64 size;
    GFileType type;
} fileutil_file_prop_t;

typedef enum fileutil_cp_code_en {
    FUCP_START,
    FUCP_DONE,
    FUCP_ERROR,
} fileutil_cp_code_t;

typedef struct fileutil_cp_st {
    goffset current_fileno;
    goffset total_fileno;
    fileutil_file_prop_t *f_prop;
} fileutil_cp_t;

typedef gboolean (*progress_cbk_t)(fileutil_cp_code_t, fileutil_cp_t *, gchar *);

gchar *fileutil_get_line(const gchar *fname, const gint line);
gint64 fileutil_get_size(const gchar *fname);
void fileutil_delete(const gchar *fname);
GList *fileutil_ls(const gchar *path, gboolean recursive);
void fileutil_cp(const gchar *src, const gchar *dest, progress_cbk_t progress_cbk);
void fileutil_ls_clear(GList *f_list);

#endif /* FILEUTIL_H_ */
