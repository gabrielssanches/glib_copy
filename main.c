#include "file_util.h" 

#define G_LOG_USE_STRUCTURED
#include <glib.h>
#include <glib/gprintf.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>


static gboolean arg_verbose = FALSE;
static gchar *arg_log_level = "none";
static gchar *arg_log_files = "all";
static gchar *arg_if = NULL;
static gchar *arg_of = NULL;
static gchar *arg_list = NULL;
static gboolean arg_recursive = FALSE;

static GOptionEntry entries[] = { 
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &arg_verbose, "Be verbose", NULL },
    { "log-level", 'l', 0, G_OPTION_ARG_FILENAME, &arg_log_level, "Message level that gets logged/displayed. Comma separated list: message info debug all", "<log levels>" },
    { "log-files", 'f', 0, G_OPTION_ARG_FILENAME, &arg_log_files, "Files that get logged. Comma separated list ex: main.c test_file.c", "<file list>" },
    { "if", 'i', 0, G_OPTION_ARG_FILENAME, &arg_if, "Source path", "<path>" },
    { "of", 'o', 0, G_OPTION_ARG_FILENAME, &arg_of, "Destination path", "<path>" },
    { "list", 't', 0, G_OPTION_ARG_FILENAME, &arg_list, "List path", "<path>" },
    { "recursive", 'r', 0, G_OPTION_ARG_NONE, &arg_recursive, "Be recursive", NULL },
    { NULL }
};

static GLogLevelFlags _log_level = G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING;

GLogWriterOutput log_writer (
    GLogLevelFlags log_level,
    const GLogField *fields,
    gsize n_fields,
    gpointer data
) {
    if ((log_level & _log_level) == 0) return G_LOG_WRITER_HANDLED;

    const gchar *c_file = NULL;
    const gchar *c_line = NULL;
    const gchar *c_func = NULL;
    const GLogField *field = fields;
    for (gsize k = 0; k < n_fields; k++, field++) {
        if (g_strcmp0("CODE_FILE", field->key) == 0) c_file = field->value;
        if (g_strcmp0("CODE_LINE", field->key) == 0) c_line = field->value;
        if (g_strcmp0("CODE_FUNC", field->key) == 0) c_func = field->value;
    }
    if (g_strcmp0("all", arg_log_files) != 0) {
        if (!g_str_match_string(c_file, arg_log_files, TRUE)) return G_LOG_WRITER_HANDLED;
    }

    gchar *msg = g_log_writer_format_fields(log_level, fields, n_fields, FALSE);

    // writes to stdout
    printf("%s:%s [%s] %s\n", c_file, c_line, c_func, msg);
    // write to syslog
    syslog(LOG_NOTICE, "%s:%s [%s] %s\n", c_file, c_line, c_func, msg);

    g_free(msg);
    return G_LOG_WRITER_HANDLED;
}

typedef struct cp_data_st {
    gchar *src;
    gchar *dest;
} cp_data_t;

static gboolean cp_progress_cbk(fileutil_cp_code_t code, fileutil_cp_t *cp_info, gchar *msg) {
    g_debug("> %d %s [%ld/%ld]\n", code, cp_info->f_prop->path_relative, cp_info->current_fileno, cp_info->total_fileno);
    return TRUE;
}

static gpointer thread_cp(gpointer data) {
    cp_data_t *cp_data = data;

    fileutil_cp(cp_data->src, cp_data->dest, cp_progress_cbk);

    return NULL;
}

int main(int argc, char *argv[]) {
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("- Virtual HMI");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        g_print("%s\n", g_option_context_get_help(context, TRUE, NULL));
        exit(1);
    }   
    g_option_context_free(context);

    g_printf("verbose: %s\n", arg_verbose?"True":"False");
    g_printf("log-level: %s\n", arg_log_level);
    g_printf("log-files: %s\n", arg_log_files);
    
    if (g_str_match_string("message", arg_log_level, TRUE)) {
        _log_level |= G_LOG_LEVEL_MESSAGE;
    }
    if (g_str_match_string("info", arg_log_level, TRUE)) {
        _log_level |= G_LOG_LEVEL_INFO;
    }
    if (g_str_match_string("debug", arg_log_level, TRUE)) {
        _log_level |= G_LOG_LEVEL_DEBUG;
    }
    if (arg_verbose || g_str_match_string("all", arg_log_level, TRUE)) {
        _log_level |= G_LOG_LEVEL_MASK;
    }

    g_log_set_writer_func(log_writer, NULL, NULL);

    openlog("glib_copy", LOG_NDELAY, 0);

    g_message("hey");
   
    GList *f_list = fileutil_ls(arg_list, arg_recursive);
    fileutil_ls_clear(f_list);

    cp_data_t cp_data;
    cp_data.src = arg_if;
    cp_data.dest = arg_of;
    GThread *t_cp = g_thread_new("Copy", thread_cp, &cp_data);

    g_thread_join(t_cp);

    g_printf("Done\n");

    return 0;
}
