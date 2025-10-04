# REPORT.md

---

## Feature-2: File Metadata

**Q: stat() vs lstat()**  
- `stat()` follows symbolic links → returns metadata of the target.  
- `lstat()` does not follow symlinks → returns metadata of the symlink itself.  
👉 For `ls`, `lstat()` is appropriate to display symlink entries and their targets.

**Q: st_mode bit extraction**  
- `st_mode` contains file type + permission bits.  
- Check permissions with bitwise AND:  
  ```c
  if (st.st_mode & S_IRUSR) { /* owner can read */ }

    Check file type with macros:

    if (S_ISDIR(st.st_mode)) { /* directory */ }
    if (S_ISREG(st.st_mode)) { /* regular file */ }
    if (S_ISLNK(st.st_mode)) { /* symlink */ }

Feature-3: Display Layout

Q: "Down then across" logic

    Columns: cols = terminal_width / (max_filename_length + spacing)

    Rows: rows = ceil(nfiles / cols)

    Index formula: idx = row + col * rows

    Iterating rows first → arranges files down each column before moving across.

Q: ioctl purpose

    ioctl with TIOCGWINSZ → gets terminal width.

    Allows adaptive column layout.

    Without it, fallback is fixed width (80 chars), which may wrap/clutter output.

Feature-4: Implementation Strategy

Q: complexity comparison

    Down then across: requires filename length + count upfront → more pre-calculation.

    Across: simpler, just print and wrap when reaching max width.

Q: display-mode strategy

    Use a flag/enum set by getopt().

    After reading & sorting, decide mode:

    if (flag_long) { print_long(); }
    else if (flag_across) { print_across(); }
    else { print_down_across(); }

Feature-5: Sorting

Q: why read all entries then sort

    Sorting requires full list to compare.

    Streaming cannot produce globally sorted output.

    Drawback: O(n) memory.

    For huge directories, real ls may use external sorting.

Q: qsort comparison function

int cmp(const void *a, const void *b) {
    const char *const *pa = a;
    const char *const *pb = b;
    return strcmp(*pa, *pb);
}

Feature-6: Colors & Permissions

Q: ANSI color codes

    Escape sequence: \033[ (or \x1b[) … m

    Example:

    printf("\033[0;32mGreen Text\033[0m\n");

    Reset with \033[0m.

Q: which permission bits for executable

    S_IXUSR → owner execute

    S_IXGRP → group execute

    S_IXOTH → others execute
    👉 If any set, file is executable.

Feature-7: Recursion

Q: recursive base case

    Stop if:

        Path is not a directory, OR

        Path is "." or "..".

    Also skip symlinked directories unless explicitly allowed (detect with lstat).

Q: why build full path before recursion

    Needed for correctness:

    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", parent, child);
    do_ls(fullpath);

    Avoids ambiguity; otherwise relative paths break unless you chdir.

Testing Checklist

    Run with no args → prints error.

    Run on empty directory.

    Run with symlinks.

    Run with mixed file lengths.

    Run on nested directories (recursive).

    Test different terminal widths.

    Test with and without colors.

    Compare output with real ls.
