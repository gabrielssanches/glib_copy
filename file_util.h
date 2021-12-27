#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#define G_LOG_USE_STRUCTURED
#include <glib.h>
#include <gio/gio.h>

typedef enum fileutil_ftype_en {
    FTYPE_REGULAR_FILE,
    FTYPE_DIRECTORY,
    FTYPE_SYMLINK
} fileutil_ftype_t;

typedef struct fileutil_file_prop_st {
    gchar *name;
    gchar *path;
    gint64 size;
    fileutil_ftype_t type;
} fileutil_file_prop_t;

typedef struct fileutil_file_prop2_st {
    gchar *name;
    gchar *path;
    gint64 size;
    GFileType type;
} fileutil_file_prop2_t;

gchar *fileutil_get_line(const gchar *fname, const gint line);
gint64 fileutil_get_size(const gchar *fname);
void fileutil_delete(const gchar *fname);
GList *fileutil_dir_list(gchar *path, gboolean recursive);
fileutil_ftype_t fileutil_get_type(const gchar *fname);
GList *fileutil_ls(const gchar *path, gboolean recursive);
void fileutil_cp(const gchar *src, const gchar *dest);

#endif /* FILEUTIL_H_ */
