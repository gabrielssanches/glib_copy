#include "file_util.h"

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

static fileutil_file_prop_t *fileutil_get_prop(GFileInfo *f_info, GFile *file_parent, GFile *file_child) {
    fileutil_file_prop_t *f_prop = g_malloc0(sizeof(fileutil_file_prop_t));
    
    f_prop->name = g_strdup(g_file_info_get_name(f_info));
    f_prop->path = g_file_get_path(file_child);
    f_prop->path_relative = g_file_get_relative_path(file_parent, file_child);
    if (f_prop->path_relative == NULL) {
        f_prop->path_relative = g_strdup(f_prop->name);
    }
    f_prop->size = g_file_info_get_size(f_info);
    f_prop->type = g_file_info_get_file_type(f_info);
    f_prop->target = g_strdup(g_file_info_get_symlink_target(f_info));
    g_info("%s;%d;%ld;%s;%s:%s",
        f_prop->name,
        f_prop->type,
        f_prop->size,
        f_prop->path_relative,
        f_prop->path,
        f_prop->target
    );
#if 0
    gboolean is_symlink = g_file_info_get_is_symlink(f_info);
    (void)is_symlink; // TODO g_file_info_get_file_type is not returning G_FILE_TYPE_SYMBOLIC_LINK on symbolic files. fix?
#endif
    return f_prop;
}

#define LS_ATTR G_FILE_ATTRIBUTE_STANDARD_TYPE","G_FILE_ATTRIBUTE_STANDARD_NAME","G_FILE_ATTRIBUTE_STANDARD_SIZE","G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN","G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET
static GList *fileutil_ls_dir(GFile *file, gboolean recursive, GList *f_list, GFile *file_top_dir) {
    GFileEnumerator *f_en = g_file_enumerate_children(file, LS_ATTR, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
    for (;;) {
        GFileInfo *f_info = g_file_enumerator_next_file(f_en, NULL, NULL);
        if (f_info == NULL) break;
        GFile *file_child = g_file_enumerator_get_child(f_en, f_info);

        fileutil_file_prop_t *f_prop = fileutil_get_prop(f_info, file_top_dir, file_child);
        g_object_unref(f_info);
        f_list = g_list_append(f_list, f_prop);
        if (f_prop->type == G_FILE_TYPE_DIRECTORY && recursive) {
            GFile *file_p = g_file_new_for_path(f_prop->path);
            f_list = fileutil_ls_dir(file_p, recursive, f_list, file_top_dir);
            g_object_unref(file_p);
        }
    }
    g_object_unref(f_en);
    return f_list;
}

static void fileutil_prop_free(gpointer data, gpointer user_data) {
    fileutil_file_prop_t *f_prop = data;
    g_free(f_prop->name);
    g_free(f_prop->path);
    g_free(f_prop->path_relative);
    g_free(f_prop->target);
}

void fileutil_ls_clear(GList *f_list) {
    g_list_foreach(f_list, fileutil_prop_free, NULL);
    g_list_free(f_list);
}

GList *fileutil_ls(const gchar *path, gboolean recursive) {
    GList *f_list = NULL;
    if (path == NULL) return NULL;

    GFile *file = g_file_new_for_path(path);
    g_assert_nonnull(file);
    gboolean path_exists = g_file_query_exists(file, NULL);
    if (!path_exists) {
        g_warning("path %s not found", path);
        goto out;
    }

    GFileType f_type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);
    if (f_type == G_FILE_TYPE_DIRECTORY) {
        f_list = fileutil_ls_dir(file, recursive, f_list, file);
    } else {
        GFileInfo* f_info = g_file_query_info(file, LS_ATTR, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
        fileutil_file_prop_t *f_prop = fileutil_get_prop(f_info, file, file);
        f_list = g_list_append(f_list, f_prop);
        g_object_unref(f_info);
    }
out:
    g_object_unref(file);
    return f_list;
}

#if 0
/// \brief Copy progress callback
/// Called at least once when the copy starts
/// Called when copy finishes (not called if size is 0)
static void fileutil_cp_cbk (
    goffset current_num_bytes,
    goffset total_num_bytes,
    gpointer user_data
) {
    fileutil_cp_t *cp_info = user_data;
    //g_debug("%s[%ld/%ld]\n", f_prop->path_relative, current_num_bytes, total_num_bytes);
    g_debug("> %s [%ld/%ld][%ld/%ld]\n", cp_info->f_prop->path_relative, cp_info->current_fileno, cp_info->total_fileno, current_num_bytes, total_num_bytes);
}
#endif

static gboolean dummy_cp_progress_cbk(fileutil_cp_code_t code, fileutil_cp_t *cp_info, gchar *msg) {
    return TRUE;
}

void fileutil_cp(const gchar *src, const gchar *dest, progress_cbk_t progress_cbk) {
    if (src == NULL || dest == NULL) return;

    progress_cbk_t prog_cbk = dummy_cp_progress_cbk;
    if (progress_cbk != NULL) prog_cbk = progress_cbk;

    GList *f_list = fileutil_ls(src, TRUE);
    if (f_list == NULL) {
        g_warning("Could not list path %s", src);
        return;
    }
    GFile *file_dest_top_dir = g_file_new_for_path(dest);
    (void)g_file_make_directory_with_parents(file_dest_top_dir, NULL, NULL);
    g_object_unref(file_dest_top_dir);

    GError *error = NULL;
    gboolean create_dirs = TRUE;
    for (;;) {
        GList *it = f_list;
        fileutil_cp_t cp_info;
        cp_info.current_fileno = 0;
        cp_info.total_fileno = g_list_length(f_list);
        for (;it != NULL; it = g_list_next(it)) {
            fileutil_file_prop_t *f_prop = it->data;
            gboolean res_cbk = TRUE;
            gchar *err_msg = NULL;

            cp_info.f_prop = f_prop;
            gchar *path_dest = g_strdup_printf("%s/%s", dest, f_prop->path_relative);
            GFile *file_dest = g_file_new_for_path(path_dest);
            if (create_dirs) {
                if (f_prop->type == G_FILE_TYPE_DIRECTORY) {
                    // create dest dir instead of file copying (no error checking needed)
                    (void)g_file_make_directory_with_parents(file_dest, NULL, NULL);
                }
            } else {
                goto skip;
            }
            cp_info.current_fileno++;
            res_cbk = prog_cbk(FUCP_START, &cp_info, NULL);
            if (f_prop->type == G_FILE_TYPE_SYMBOLIC_LINK) {
                gboolean res = g_file_make_symbolic_link(file_dest, f_prop->target, NULL, &error);
                if (error != NULL) {
                    err_msg = g_strdup_printf("Error creating symlink for %s: [%d|%d] %s", f_prop->path_relative, error->domain, error->code, error->message);
                    g_error_free(error);
                    error = NULL;
                } else if (!res) {
                    err_msg = g_strdup_printf("Error copying %s: unknown", f_prop->path_relative);
                } else {
                    g_info("symlink created for %s", f_prop->path_relative);
                }
            } else if (f_prop->type == G_FILE_TYPE_REGULAR) {
                GFile *file_src = g_file_new_for_path(f_prop->path);
                gboolean res = g_file_copy(
                    file_src,
                    file_dest,
                    G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_ALL_METADATA,
                    NULL, // cancelable
                    NULL, // progress callback
                    NULL, // progress callback user data
                    &error
                );
                if (error != NULL) {
                    err_msg = g_strdup_printf("Error copying %s: [%d|%d] %s", f_prop->path_relative, error->domain, error->code, error->message);
                    g_error_free(error);
                    error = NULL;
                } else if (!res) {
                    err_msg = g_strdup_printf("Error copying %s: unknown", f_prop->path_relative);
                } else {
                    g_info("file %s copied", f_prop->path_relative);
                }
                g_object_unref(file_src);
            } else if (f_prop->type == G_FILE_TYPE_DIRECTORY) {
            } else {
                err_msg = g_strdup_printf("Unhandled copy of %s, type=%d", f_prop->path, f_prop->type);
            }
            if (err_msg != NULL) {
                g_warning("%s", err_msg);
                res_cbk = prog_cbk(FUCP_ERROR, &cp_info, err_msg);
                g_free(err_msg);
            } else {
                res_cbk = prog_cbk(FUCP_DONE, &cp_info, err_msg);
            }
    skip:
            g_free(path_dest);
            g_object_unref(file_dest);
            if (!res_cbk) goto out;
        }
        if (!create_dirs) break;
        create_dirs = FALSE;
    }
out:
    fileutil_ls_clear(f_list);
}
