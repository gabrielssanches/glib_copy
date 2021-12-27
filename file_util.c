#include "file_util.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <sys/stat.h>

static gchar *strip_newline(const gchar *string2strip, const gint line) {
    gchar *stripped_str = NULL;
    gchar **sl = g_strsplit_set(string2strip, "\n", -1);
    gint k;
    for (k = 0; k < line && sl[k] != NULL; k++);
    if (sl[k] != NULL) {
        stripped_str = g_strdup(sl[k]);
    }
    g_strfreev(sl);
    return stripped_str;
}

gchar *fileutil_get_line(const gchar *fname, const gint line) {
    g_autofree gchar *tmpstr = NULL;
    gchar *filestr = NULL;
    if (g_file_get_contents(fname, &tmpstr, NULL, NULL)) {
        filestr = strip_newline(tmpstr, line);
    } else {
        g_debug("[FILEUTIL] Could not get line from file: %s\n", fname);
    }
    return filestr;
}

gint64 fileutil_get_size(const gchar *fname) {
    struct stat st;
    if (lstat(fname, &st) == -1) {
        g_warning("[FILEUTIL] File %s not found\n", fname);
        return -1;
    }
    return st.st_size;
}

fileutil_ftype_t fileutil_get_type(const gchar *fname) {
    struct stat st;
    if (lstat(fname, &st) == -1) {
        g_warning("[FILEUTIL] File %s not found\n", fname);
        return -1;
    }
    if ((st.st_mode & S_IFDIR) == S_IFDIR) return FTYPE_DIRECTORY;
    if ((st.st_mode & S_IFLNK) == S_IFLNK) return FTYPE_SYMLINK;
    return FTYPE_REGULAR_FILE;
}

void fileutil_delete(const gchar *fname) {
    GFile *gfile = g_file_new_for_path(fname);
    gboolean res = g_file_delete(gfile, NULL, NULL);
    if (res) {
        g_warning("[FILEUTIL] Deleted file %s\n", fname);
    } else {
        g_warning("[FILEUTIL] Couldn't delete file %s\n", fname);
    }
    g_object_unref(gfile);
}

GList *fileutil_dir_list(gchar *path, gboolean recursive) {
    GList *file_list = NULL;

    GError *err = NULL;
    GDir *dir = g_dir_open(path, 0, &err);
    if (dir == NULL) return NULL;

    for (guint32 fid = 0; ; fid++) {
        gchar *fname = g_strdup(g_dir_read_name(dir));
        if (fname == NULL) break;
        fileutil_file_prop_t *file = g_malloc0(sizeof(fileutil_file_prop_t));

        file->name = fname;
        file->path = g_strdup_printf("%s/%s", path, fname);
        file->size = fileutil_get_size(file->path);
        file->type = fileutil_get_type(file->path);
        file_list = g_list_append(file_list, file);
    }
    g_dir_close(dir);
    return file_list;
}

static fileutil_file_prop2_t *fileutil_get_prop(GFileInfo *f_info, const gchar *parent_path) {
    fileutil_file_prop2_t *f_prop = g_malloc0(sizeof(fileutil_file_prop2_t));

    f_prop->name = g_strdup(g_file_info_get_name(f_info));
    if (parent_path != NULL) {
        f_prop->path = g_strdup_printf("%s/%s", parent_path, f_prop->name);
    } else {
        f_prop->path = g_strdup_printf("./%s", f_prop->name);
    }
    f_prop->size = g_file_info_get_size(f_info);
    gboolean is_symlink = g_file_info_get_is_symlink(f_info);
    (void)is_symlink; // TODO g_file_info_get_file_type is not returning G_FILE_TYPE_SYMBOLIC_LINK on symbolic files. fix?
    f_prop->type = g_file_info_get_file_type(f_info);
    return f_prop;
}

#define LS_ATTR G_FILE_ATTRIBUTE_STANDARD_TYPE","G_FILE_ATTRIBUTE_STANDARD_NAME","G_FILE_ATTRIBUTE_STANDARD_SIZE","G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN
GList *fileutil_ls_dir(GFile *file, gboolean recursive, GList *f_list) {
    gchar *path = g_file_get_path(file);
    GFileEnumerator *f_en = g_file_enumerate_children(file, LS_ATTR, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    for (;;) {
        GFileInfo *f_info = g_file_enumerator_next_file(f_en, NULL, NULL);
        if (f_info == NULL) break;

        fileutil_file_prop2_t *f_prop = fileutil_get_prop(f_info, path);
        g_object_unref(f_info);
        f_list = g_list_append(f_list, f_prop);
        if (f_prop->type == G_FILE_TYPE_DIRECTORY && recursive) {
            GFile *file_p = g_file_new_for_path(f_prop->path);
            f_list = fileutil_ls_dir(file_p, recursive, f_list);
            g_object_unref(file_p);
        }
    }
    g_free(path);
    g_object_unref(f_en);
    return f_list;
}

GList *fileutil_ls(const gchar *path, gboolean recursive) {
    GList *f_list = NULL;

    GFile *file = g_file_new_for_path(path);
    g_assert_nonnull(file);

    GFileType f_type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL);
    if (f_type == G_FILE_TYPE_DIRECTORY) {
        f_list = fileutil_ls_dir(file, recursive, f_list);
    } else {
        GFileInfo* f_info = g_file_query_info(file, LS_ATTR, G_FILE_QUERY_INFO_NONE, NULL, NULL);
        fileutil_file_prop2_t *f_prop = fileutil_get_prop(f_info, NULL);
        f_list = g_list_append(f_list, f_prop);
        g_object_unref(f_info);
    }
    g_object_unref(file);
    return f_list;
}

static void fileutil_cp_cbk (
    goffset current_num_bytes,
    goffset total_num_bytes,
    gpointer user_data
) {
    gchar *f_name = user_data;
    g_printf("%s[%lld/%lld]\n", f_name, current_num_bytes, total_num_bytes);
}

void fileutil_cp(const gchar *src, const gchar *dest) {
    GList *f_list = fileutil_ls(src, TRUE);
    if (f_list == NULL) return;

    GFile *test_path_dest = g_file_new_for_path(dest);
    GFileType f_type_dest = g_file_query_file_type(test_path_dest, G_FILE_QUERY_INFO_NONE, NULL);
    //g_file_query_exists
    if (f_type_dest == G_FILE_TYPE_UNKNOWN) {
        // most likely it doesn't exists
    }


    g_printf("File Type=%d\n", f_type_dest);
    //if (!g_file_make_directory_with_parents(gfile, NULL, NULL)) {

    GList *it = f_list;
    GError *error = NULL;
    for (;it != NULL; it = g_list_next(it)) {
        fileutil_file_prop2_t *f_prop = it->data;
        gchar *path_src = g_strdup_printf("%s", f_prop->path);
        gchar *path_dest = g_strdup_printf("%s/%s", dest, f_prop->name);
        GFile *file_src = g_file_new_for_path(path_src);
        GFile *file_dest = g_file_new_for_path(path_dest);

        gboolean res = g_file_copy(
            file_src,
            file_dest,
            G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_ALL_METADATA,
            NULL, // cancelable
            fileutil_cp_cbk,
            NULL,
            &error
        );
        g_printf("[%s]%s\n", res?"OK":"FAILED", f_prop->name);
        if (error != NULL) {
            g_warning("Error on connman GetTechnologies: [%d|%d] %s", error->domain, error->code, error->message);
            g_error_free(error);
        }
    }
}