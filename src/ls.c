#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <strings.h>
/* src/ls.c
   v1.5.0 - ls with -a, -l, -x, -R, alphabetical sort (qsort) and colorized output.
   Color rules:
     Directory -> Blue
     Executable -> Green
     Tarballs (.tar, .gz, .zip, .tgz, .bz2, .xz) -> Red
     Symbolic Link -> Magenta (pink)
     Special files (device, socket, fifo) -> Reverse video
   Colors disabled automatically if stdout is not a TTY.
*/
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   // for strcasecmp
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ANSI color macros */
#define CLR_RESET    "\x1b[0m"
#define CLR_BLUE     "\x1b[0;34m"
#define CLR_GREEN    "\x1b[0;32m"
#define CLR_RED      "\x1b[0;31m"
#define CLR_MAGENTA  "\x1b[0;35m"
#define CLR_REVERSE  "\x1b[7m"

/* flags */
static int flag_all = 0;       // -a
static int flag_long = 0;      // -l
static int flag_across = 0;    // -x
static int flag_recursive = 0; // -R
static int use_color = 1;

/* comparator for qsort */
static int cmpstr(const void *a, const void *b) {
    const char *A = *(const char * const *)a;
    const char *B = *(const char * const *)b;
    return strcmp(A, B);
}

/* get terminal width */
static int term_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) return 80;
    if (w.ws_col == 0) return 80;
    return w.ws_col;
}

/* format permission string */
static void format_mode(mode_t m, char *out) {
    out[0] = S_ISDIR(m) ? 'd' :
             S_ISLNK(m) ? 'l' :
             S_ISCHR(m) ? 'c' :
             S_ISBLK(m) ? 'b' :
             S_ISFIFO(m) ? 'p' :
             S_ISSOCK(m) ? 's' : '-';
    out[1] = (m & S_IRUSR) ? 'r' : '-';
    out[2] = (m & S_IWUSR) ? 'w' : '-';
    out[3] = (m & S_IXUSR) ? 'x' : '-';
    out[4] = (m & S_IRGRP) ? 'r' : '-';
    out[5] = (m & S_IWGRP) ? 'w' : '-';
    out[6] = (m & S_IXGRP) ? 'x' : '-';
    out[7] = (m & S_IROTH) ? 'r' : '-';
    out[8] = (m & S_IWOTH) ? 'w' : '-';
    out[9] = (m & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
    if (m & S_ISUID) out[3] = (out[3]=='x') ? 's' : 'S';
    if (m & S_ISGID) out[6] = (out[6]=='x') ? 's' : 'S';
    if (m & S_ISVTX) out[9] = (out[9]=='x') ? 't' : 'T';
}

/* helper: check archive extensions */
static int is_tarball(const char *name) {
    if (!name) return 0;
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    if (strcasecmp(ext, ".tar") == 0) return 1;
    if (strcasecmp(ext, ".gz") == 0) return 1;
    if (strcasecmp(ext, ".tgz") == 0) return 1;
    if (strcasecmp(ext, ".zip") == 0) return 1;
    if (strcasecmp(ext, ".bz2") == 0) return 1;
    if (strcasecmp(ext, ".xz") == 0) return 1;
    return 0;
}

/* pick color */
static const char* pick_color(const char *name, mode_t mode) {
    if (!use_color) return NULL;
    if (S_ISLNK(mode)) return CLR_MAGENTA;
    if (S_ISDIR(mode)) return CLR_BLUE;
    if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISSOCK(mode) || S_ISFIFO(mode)) return CLR_REVERSE;
    if ((mode & S_IXUSR) || (mode & S_IXGRP) || (mode & S_IXOTH)) return CLR_GREEN;
    if (is_tarball(name)) return CLR_RED;
    return NULL;
}

/* print long listing entry */
static void print_long_item(const char *dir, const char *name) {
    char full[PATH_MAX];
    struct stat st;
    snprintf(full, sizeof(full), "%s/%s", dir, name);
    if (lstat(full, &st) == -1) { perror(full); return; }
    char perms[11]; format_mode(st.st_mode, perms);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    char timebuf[64];
    struct tm *mt = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", mt);
    printf("%s %2lu %s %s %6lld %s ",
           perms, (unsigned long)st.st_nlink,
           pw ? pw->pw_name : "?", gr ? gr->gr_name : "?",
           (long long)st.st_size, timebuf);
    const char *color = pick_color(name, st.st_mode);
    if (color) printf("%s", color);
    printf("%s", name);
    if (color) printf("%s", CLR_RESET);
    if (S_ISLNK(st.st_mode)) {
        char buf[PATH_MAX];
        ssize_t r = readlink(full, buf, sizeof(buf)-1);
        if (r >= 0) { buf[r] = '\0'; printf(" -> %s", buf); }
    }
    putchar('\n');
}

/* print name padded with color */
static void print_name_padded(const char *dir, const char *name, int width) {
    char full[PATH_MAX];
    struct stat st;
    snprintf(full, sizeof(full), "%s/%s", dir, name);
    if (lstat(full, &st) == -1) { printf("%-*s", width, name); return; }
    const char *color = pick_color(name, st.st_mode);
    if (color) printf("%s", color);
    printf("%-*s", width, name);
    if (color) printf("%s", CLR_RESET);
}

/* down-then-across */
static void print_columns_down(const char *dir, char **names, int count, int maxlen) {
    int width = term_width();
    int colw = maxlen + 2; if (colw < 1) colw = 1;
    int ncols = width / colw; if (ncols < 1) ncols = 1;
    int nrows = (count + ncols - 1) / ncols;
    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            int idx = c * nrows + r;
            if (idx < count) print_name_padded(dir, names[idx], colw);
        }
        putchar('\n');
    }
}

/* across -x */
static void print_columns_across(const char *dir, char **names, int count, int maxlen) {
    int width = term_width();
    int colw = maxlen + 2; if (colw < 1) colw = 1;
    int cur = 0;
    for (int i = 0; i < count; ++i) {
        if (cur + colw > width && cur > 0) { putchar('\n'); cur = 0; }
        print_name_padded(dir, names[i], colw);
        cur += colw;
    }
    if (cur > 0) putchar('\n');
}

/* read directory, apply -a */
static int read_dir(const char *dir, char ***names_out, int *maxlen_out) {
    DIR *d = opendir(dir);
    if (!d) { perror(dir); return -1; }
    struct dirent *entry;
    char **names = NULL;
    int cap = 0, n = 0, maxlen = 0;
    while ((entry = readdir(d)) != NULL) {
        if (!flag_all && entry->d_name[0] == '.') continue; /* -a check */
        if (n + 1 > cap) {
            cap = cap ? cap * 2 : 128;
            names = realloc(names, cap * sizeof(char*));
            if (!names) { perror("realloc"); closedir(d); return -1; }
        }
        names[n] = strdup(entry->d_name);
        if (!names[n]) { perror("strdup"); closedir(d); return -1; }
        int L = strlen(entry->d_name);
        if (L > maxlen) maxlen = L;
        n++;
    }
    closedir(d);
    *names_out = names;
    *maxlen_out = maxlen;
    return n;
}

static void free_names(char **names, int n) {
    for (int i = 0; i < n; ++i) free(names[i]);
    free(names);
}

/* recursive listing */
static void do_ls(const char *dir) {
    char **names = NULL;
    int maxlen = 0;
    int n = read_dir(dir, &names, &maxlen);
    if (n < 0) return;
    qsort(names, n, sizeof(char*), cmpstr);

    if (flag_long) {
        for (int i = 0; i < n; ++i) print_long_item(dir, names[i]);
    } else {
        if (flag_across) print_columns_across(dir, names, n, maxlen);
        else print_columns_down(dir, names, n, maxlen);
    }

    if (flag_recursive && n > 0) {
        for (int i = 0; i < n; ++i) {
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", dir, names[i]);
            struct stat st;
            if (lstat(full, &st) == -1) continue;
            if (S_ISDIR(st.st_mode)) {
                if (strcmp(names[i], ".") == 0 || strcmp(names[i], "..") == 0) continue;
                printf("\n%s:\n", full);
                do_ls(full);
            }
        }
    }
    free_names(names, n);
}

int main(int argc, char **argv) {
    int opt;
    use_color = isatty(STDOUT_FILENO);
    while ((opt = getopt(argc, argv, "alRx")) != -1) {
        switch (opt) {
            case 'a': flag_all = 1; break;
            case 'l': flag_long = 1; break;
            case 'R': flag_recursive = 1; break;
            case 'x': flag_across = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-a] [-l] [-R] [-x] [path]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    const char *path = (optind < argc) ? argv[optind] : ".";
    struct stat st;
    if (lstat(path, &st) == 0 && !S_ISDIR(st.st_mode)) {
        if (flag_long) print_long_item(".", path);
        else {
            const char *color = pick_color(path, st.st_mode);
            if (color) printf("%s", color);
            printf("%s\n", path);
            if (color) printf("%s", CLR_RESET);
        }
        return 0;
    }
    do_ls(path);
    return 0;
}
