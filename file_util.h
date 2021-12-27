#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#define G_LOG_USE_STRUCTURED
#include <glib.h>
#include <gio/gio.h>

typedef struct fileutil_file_prop_st {
    gchar *name;            // file name
    gchar *path;            // absolute path
    gint64 size;
    GFileType type;
} fileutil_file_prop_t;

gchar *fileutil_get_line(const gchar *fname, const gint line);
gint64 fileutil_get_size(const gchar *fname);
void fileutil_delete(const gchar *fname);
GList *fileutil_dir_list(gchar *path, gboolean recursive);
GList *fileutil_ls(const gchar *path, gboolean recursive);
void fileutil_cp(const gchar *src, const gchar *dest);
void fileutil_ls_clear(GList *f_list);

#endif /* FILEUTIL_H_ */
