/*============================================================================
Copyright (c) 2026 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <libintl.h>
#include <getopt.h>
#include <sys/wait.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

/*----------------------------------------------------------------------------*/
/* Global variables                                                           */
/*----------------------------------------------------------------------------*/

static GObject *msg_dlg, *copy_btn, *open_btn, *quit_btn, *check, *msg;

static char g_filepath[PATH_MAX];

static const char opts[] = "achfpv";

static const struct option long_opts[] =
{
    { "area",   0, NULL, 'a' },
    { "copy",   0, NULL, 'c' },
    { "help",   0, NULL, 'h' },
    { "file",   0, NULL, 'f' },
    { "prompt", 0, NULL, 'p' },
    { "view",   0, NULL, 'v' }
};

static const char helptext[] = {
    "Usage:\n"
    "gui-screenshot [OPTIONS]\n"
    "\n"
    "Options:\n"
    "  -a, --area       Area mode - use mouse to select area to be captured\n"
    "  -f, --file       Save captured image to file in ~/Pictures only\n"
    "  -v, --view       Open captured image file in default image viewer\n"
    "  -c, --copy       Copy captured image to clipboard as well as saving to file\n"
    "  -p, --prompt     Reset saved output option and display interactive prompt\n"
    "  -h, --help       Show this help text\n"
};

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void build_filepath (void);
static int run_cmd (char *const argv[], char **stdout_buf, size_t *stdout_len);
static void write_setting (const char *option);
static gboolean open (GtkButton *button, gpointer data);
static gboolean copy (GtkButton *button, gpointer data);
static gboolean quit (GtkButton *button, gpointer data);
static gboolean key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/* ── helpers ─────────────────────────────────────────────────────── */

static void build_filepath(void)
{
    const char *dir = getenv("XDG_PICTURES_DIR");
    if (!dir || dir[0] == '\0') {
        const char *home = getenv("HOME");
        if (!home)
            home = "/tmp";
        snprintf(g_filepath, sizeof g_filepath, "%s/Pictures", home);
    } else {
        snprintf(g_filepath, sizeof g_filepath, "%s", dir);
    }

    /* ensure directory exists */
    mkdir(g_filepath, 0755);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    size_t len = strlen(g_filepath);
    strftime(g_filepath + len, sizeof g_filepath - len,
             "/%Y%m%d_%H%M%S_grim.png", tm);
}

/* Run a command via fork+execvp; returns exit status or -1 on error.
 * If stdout_buf/stdout_len are non-NULL, captures stdout into them. */
static int run_cmd(char *const argv[], char **stdout_buf, size_t *stdout_len)
{
    int pipefd[2] = {-1, -1};
    if (stdout_buf) {
        *stdout_buf = NULL;
        *stdout_len = 0;
        if (pipe(pipefd) < 0)
            return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        if (stdout_buf) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }
        execvp(argv[0], argv);
        _exit(127);
    }

    if (stdout_buf) {
        close(pipefd[1]);
        /* read all stdout */
        size_t cap = 256, len = 0;
        char *buf = malloc(cap);
        ssize_t n;
        while ((n = read(pipefd[0], buf + len, cap - len)) > 0) {
            len += n;
            if (len == cap) {
                cap *= 2;
                buf = realloc(buf, cap);
            }
        }
        close(pipefd[0]);
        /* strip trailing newline */
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            len--;
        buf[len] = '\0';
        *stdout_buf = buf;
        *stdout_len = len;
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

/* Store user choice in ~/.config/gui-screenshot/config */

static void write_setting (const char *option)
{
    FILE *fp;
    char *fname, *dir;

    if (!check || !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check))) return;

    fname = g_build_filename (g_get_user_config_dir (), "gui-screenshot", "config", NULL);
    dir = g_path_get_dirname (fname);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);

    fp = fopen (fname, "wb");
    if (fp)
    {
        fprintf (fp, "%s", option);
        fclose (fp);
    }

    g_free (fname);
}

/* Button handlers */

static gboolean open (GtkButton *button, gpointer data)
{
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", g_filepath, (char *)NULL);
        _exit(127);
    }

    write_setting ("open");
    if (gtk_main_level ()) gtk_main_quit ();
    else exit (0);
    return FALSE;
}

static gboolean copy (GtkButton *button, gpointer data)
{
    pid_t pid = fork();
    if (pid == 0) {
        execlp("clipserve", "clipserve", g_filepath, (char *)NULL);
        _exit(127);
    }

    write_setting ("copy");
    if (gtk_main_level ()) gtk_main_quit ();
    else exit (0);
    return FALSE;
}

static gboolean quit (GtkButton *button, gpointer data)
{
    write_setting ("quit");
    if (gtk_main_level ()) gtk_main_quit ();
    else exit (0);
    return FALSE;
}

static gboolean key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        gtk_main_quit ();
        return TRUE;
    }
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Main window                                                                */
/*----------------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    char *caption, *fname, buf[16];
    FILE *fp;
    int opt;
    gboolean area = FALSE, clip = FALSE, file = FALSE, view = FALSE, prompt = FALSE;

    while ((opt = getopt_long (argc, argv, opts, long_opts, NULL)) != -1)
    {
        switch (opt)
        {
            case 'a' :  area = TRUE;
                        break;

            case 'c' :  clip = TRUE;
                        break;

            case 'f' :  file = TRUE;
                        break;

            case 'v' :  view = TRUE;
                        break;

            case 'p' :  prompt = TRUE;
                        break;

            case 'h' :
            default :   printf (helptext);
                        exit (0);
        }
    }

    if (clip + file + view > 1)
    {
        printf ("gui-screenshot: only one of -c, -f or -v can be specified\n");
        printf (helptext);
        exit (0);
    }

    build_filepath();

    /* ── capture ─────────────────────────────────────────────────── */
    if (area) {
        /* slurp → get region string */
        char *region = NULL;
        size_t region_len = 0;
        char *slurp_argv[] = {"slurp", NULL};
        int ret = run_cmd(slurp_argv, &region, &region_len);
        if (ret != 0) {
            free(region);
            return 0;          /* user pressed Escape — exit silently */
        }
        char *grim_argv[] = {"grim", "-g", region, g_filepath, NULL};
        ret = run_cmd(grim_argv, NULL, NULL);
        free(region);
        if (ret != 0) {
            fprintf(stderr, "gui-screenshot: grim failed\n");
            return 1;
        }
    } else {
        char *grim_argv[] = {"grim", g_filepath, NULL};
        if (run_cmd(grim_argv, NULL, NULL) != 0) {
            fprintf(stderr, "gui-screenshot: grim failed\n");
            return 1;
        }
    }

    /* ── verify file ─────────────────────────────────────────────── */
    struct stat st;
    if (stat(g_filepath, &st) < 0 || st.st_size == 0) {
        fprintf(stderr, "gui-screenshot: capture produced no output\n");
        return 1;
    }    
    
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    
    check = NULL;

    fname = g_build_filename (g_get_user_config_dir (), "gui-screenshot", "config", NULL);
    if (prompt) remove (fname);
    fp = fopen (fname, "rb");
    if (fp)
    {
        fgets (buf, sizeof (buf), fp);
        fclose (fp);
        
        if (!strncmp (buf, "open", 4)) open (NULL, NULL);
        if (!strncmp (buf, "copy", 4)) copy (NULL, NULL);
        if (!strncmp (buf, "quit", 4)) quit (NULL, NULL);
    }
    g_free (fname);

    if (view) open (NULL, NULL);
    if (clip) copy (NULL, NULL);
    if (file) quit (NULL, NULL);

    g_set_prgname ("wf-panel-pi");
    gtk_init (&argc, &argv);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/gui-screenshot.ui");

    msg_dlg = gtk_builder_get_object (builder, "modal");
    msg = gtk_builder_get_object (builder, "msg");
    copy_btn = gtk_builder_get_object (builder, "btn_copy");
    open_btn = gtk_builder_get_object (builder, "btn_open");
    quit_btn = gtk_builder_get_object (builder, "btn_cancel");
    check = gtk_builder_get_object (builder, "check_remember");

    g_object_unref (builder);

    g_signal_connect (copy_btn, "clicked", G_CALLBACK (copy), NULL);
    g_signal_connect (open_btn, "clicked", G_CALLBACK (open), NULL);
    g_signal_connect (quit_btn, "clicked", G_CALLBACK (quit), NULL);
    g_signal_connect (msg_dlg, "key-press-event", G_CALLBACK (key_press_event), NULL);

    gtk_window_set_title (GTK_WINDOW (msg_dlg), _("Screenshot"));
    fname = g_path_get_basename (g_filepath);
    caption = g_strdup_printf ("Screenshot captured as ~/Pictures/%s", fname);
    gtk_label_set_text (GTK_LABEL (msg), caption);
    g_free (caption);
    g_free (fname);

    gtk_widget_show (GTK_WIDGET (msg_dlg));

    gtk_main ();

    return 0;
}

/* End of file                                                                */
/*----------------------------------------------------------------------------*/
