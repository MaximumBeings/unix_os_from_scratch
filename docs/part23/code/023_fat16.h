#ifndef UNIX_OS_023_FAT16_H
#define UNIX_OS_023_FAT16_H

#include <stdint.h>

/* This chapter's own real filesystem, still the same genuine FAT16
 * volume Chapter 20 built on top of Chapter 19's own real
 * ata_read_sector()/ata_write_sector(), Chapter 21 extended with real
 * subdirectories, and Chapter 22 extended again with fat16_rmdir() --
 * cited field-for-field from OSDev Wiki ("FAT": https://wiki.osdev.org/
 * FAT) for every field this file already used, from the Microsoft FAT
 * specification for Chapter 21's own subdirectory conventions ("."/".."
 * entries, ATTR_DIRECTORY), and from IEEE Std 1003.1-2008 (POSIX.1-2008)'s
 * own rmdir() specification for Chapter 22's own empty-directory rule.
 * This chapter's own new work is lifting the ONE stated scope limit
 * every one of those three chapters shared: `name`/`path` could only
 * ever be a bare leaf or exactly "DIR/NAME.EXT" -- one real '/', never
 * more. Neither OSDev Wiki's own "FAT" page nor the Microsoft FAT
 * specification says anything about walking a MULTI-component path at
 * all -- both documents describe the real on-disk FORMAT, not how a
 * filesystem driver should parse the string an application hands it
 * (confirmed directly: OSDev's own page never mentions parsing "/"-
 * separated paths, only that a directory's own entries "should each be
 * read in the same way starting with their first cluster number" once
 * found). So this chapter's own new resolve_path() -- now a genuine,
 * arbitrary-depth walk instead of a single split at the first '/' --
 * is cited instead from a real, load-bearing convention every
 * POSIX-conformant filesystem driver already follows: IEEE Std
 * 1003.1-2008 (POSIX.1-2008), Base Definitions, Section 4.11,
 * "Pathname Resolution" (https://pubs.opengroup.org/onlinepubs/
 * 009696799/basedefs/xbd_chap04.html), quoted directly below
 * resolve_path()'s own implementation (023_fat16.c): "Each filename in
 * the pathname is located in the directory specified by its
 * predecessor ... Pathname resolution shall fail if this cannot be
 * accomplished." fat16_mkdir() and fat16_rmdir() -- Chapters 21 and 22
 * each refused outright the instant `name` held more than one '/' --
 * now resolve every intermediate component through that same real
 * walk, exactly like fat16_create_file()/fat16_read_file()/
 * fat16_delete_file() already did since Chapter 21. Nothing about the
 * real on-disk FORMAT changes this chapter at all -- every real
 * boot-sector field, FAT entry, and directory-entry layout is
 * unchanged from Chapter 22. */

/* Every real filename or path this API accepts or returns is a plain,
 * NUL-terminated string. A bare "NAME.EXT" (or "NAME" with no
 * extension) refers to the root directory, exactly as in Chapter 20.
 * "A/B/C/NAME.EXT" now refers to a real file inside a real subdirectory
 * nested arbitrarily deep -- each component from "A" through "C" must
 * already exist as a real, on-disk directory (via fat16_mkdir()) before
 * the next one can be resolved through it; this chapter's own new
 * resolve_path() (023_fat16.c) refuses outright -- never silently
 * creates an intermediate directory, never guesses -- the instant any
 * of them doesn't exist or isn't really a directory. `path` must never
 * begin or end with '/', and no component may be empty (an accidental
 * "//" mid-path) -- both refused outright too, an honest, stated limit
 * this chapter's own real demo never needs to exercise. Every real
 * component along the way -- directory names and the final leaf name
 * alike -- is an ordinary 8.3 name (at most 8 name characters, 3
 * extension characters), silently truncated by this chapter's own
 * to_83_name() (023_fat16.c) exactly as before. */

/* Builds a genuinely fresh FAT16 volume on the whole disk this
 * kernel's Chapter 19 driver talks to: a real boot sector/BIOS
 * Parameter Block, two mirrored File Allocation Tables, and an empty
 * root directory, all written via real ata_write_sector() calls.
 * Destroys any data on the disk already -- this chapter's own demo
 * always formats before using the volume. Returns 1 on success. */
int fat16_format(void);

/* Reads and validates the real on-disk boot sector, and computes
 * every geometry value this file's other functions need (first FAT
 * sector, first root-directory sector, first data sector, sectors per
 * FAT, total usable clusters) directly from it -- never hardcoded a
 * second time, so a genuinely differently-formatted volume would
 * still be read correctly. Must be called once, after fat16_format()
 * (or after finding an already-formatted volume), before any other
 * function below. Returns 1 if a valid FAT16 boot signature (0xAA55)
 * was found, 0 otherwise. */
int fat16_init(void);

/* Creates a new real subdirectory -- `name` may now be an arbitrarily
 * deep real path ("A/B/C"), resolved through this chapter's own new
 * resolve_path() exactly like fat16_create_file() already resolves
 * one: every component up to the last must already exist as a real
 * directory (Chapter 21's and 22's own one-level limit is gone;
 * Chapter 21's own bare "under the root" case, `name` with no '/' at
 * all, still works exactly as before). Allocates one real cluster for
 * the subdirectory's own contents and writes two real, cited on-disk
 * entries into it before anything else: a "." entry whose own cluster
 * fields self-reference this same new cluster, and a ".." entry whose
 * cluster fields are set to the REAL PARENT's own first cluster --
 * cited directly (Microsoft FAT specification, Section 6.5): "the
 * contents of the DIR_FstClusLO and DIR_FstClusHI fields must be the
 * same as that of the parent of the current directory. If the parent
 * of the current directory is the root directory ... the
 * DIR_FstClusLO and DIR_FstClusHI contents must be set to 0" -- this
 * chapter's own new resolve_path() supplies that real parent cluster
 * (0 for the root, exactly as Chapters 21 and 22 always assumed since
 * they only ever created directories directly under it; a genuine
 * non-zero cluster now, for any deeper parent). Then writes a real
 * directory entry for the final component into that resolved parent
 * directory, with the cited ATTR_DIRECTORY bit (0x10) set and
 * DIR_FileSize left at 0 even though a real cluster is allocated --
 * cited directly (Microsoft FAT specification, Section 6.5: "The
 * DIR_FileSize must be set to 0"). Refuses -- returns 0 -- if an
 * intermediate path component doesn't exist or isn't really a
 * directory, if an entry with the final name already exists in the
 * resolved parent, if that parent directory has no free entry left, or
 * if the volume has no free clusters left. If `out_first_cluster` is
 * non-null, the new subdirectory's real first cluster number is
 * written there on success -- mirroring fat16_create_file()'s own
 * existing convention, purely so this book's own real demos can prove
 * a removed directory's cluster gets reused, the same way Chapter 20's
 * own REUSE.TXT demo already proved it for a deleted file. */
int fat16_mkdir(const char *name, uint16_t *out_first_cluster);

/* Creates a new real file: allocates however many whole clusters
 * `size` real bytes need, chains them together in both real FAT
 * copies, writes `size` real bytes across them (the last, partial
 * cluster is zero-padded), and writes a real 32-byte directory entry
 * naming the file, its size, and its first cluster -- into the root
 * directory for a bare name, or into the real subdirectory an
 * arbitrarily deep "A/B/C/NAME.EXT" path resolves to (see the top of
 * this file). Refuses -- returns 0, writes nothing -- if a file with
 * this name already exists, if a
 * named directory component doesn't exist or isn't really a
 * directory, if the target directory has no free entry left (a real
 * subdirectory grows by one more cluster first, exactly like a file
 * would -- the root directory, by real on-disk construction, never
 * can), or if the volume has no free clusters left; never overwrites
 * or guesses. If `out_first_cluster` is non-null, the file's real
 * first cluster number is written there on success, purely so this
 * book's own real demos can prove a deleted file's cluster gets
 * reused. */
int fat16_create_file(const char *name, const uint8_t *data, uint32_t size,
                       uint16_t *out_first_cluster);

/* Reads an existing file's real contents into `buffer` -- a bare name
 * for the root directory, or a "DIR/NAME.EXT" path for a file inside a
 * real subdirectory. Refuses -- returns 0 -- if no file with this name
 * exists, if a named directory component doesn't exist or isn't
 * really a directory, if the name resolves to a real directory rather
 * than a file, or if `buffer_size` is smaller than the file's own real
 * size (this function never truncates). On success, copies exactly
 * the file's real size in bytes, walking its real cluster chain one
 * real sector at a time, and writes that size to `out_size` if
 * non-null. */
int fat16_read_file(const char *name, uint8_t *buffer, uint32_t buffer_size,
                     uint32_t *out_size);

/* Deletes an existing file -- a bare name for the root directory, or a
 * "DIR/NAME.EXT" path for a file inside a real subdirectory. Marks its
 * real directory entry as free (the cited 0xE5 "unused entry" marker)
 * and frees every real cluster in its chain back to both FAT copies
 * (each entry set back to the cited 0x0000 free value), making them
 * available to a future fat16_create_file() call again. Refuses --
 * returns 0 -- if no file with this name exists, if a named directory
 * component doesn't exist or isn't really a directory, or if the name
 * resolves to a real directory rather than a file -- a real directory
 * is only ever removed by this chapter's own new fat16_rmdir() below,
 * never by this function. Returns 1 if a matching file was found and
 * deleted. */
int fat16_delete_file(const char *name);

/* Deletes an existing real subdirectory -- `name` may now be an
 * arbitrarily deep real path ("A/B/C"), resolved through this
 * chapter's own new resolve_path() exactly like fat16_delete_file()
 * already resolves one (Chapters 21's and 22's own one-level limit is
 * gone). Refuses -- returns 0 -- if an intermediate path component
 * doesn't exist or isn't really a directory, if no such directory
 * exists, if the name resolves to a real file rather
 * than a directory (use fat16_delete_file() instead), or, cited
 * directly (IEEE Std 1003.1-2008, rmdir(): "If the directory is not an
 * empty directory, rmdir() shall fail"; "there are hard links to the
 * directory other than dot or a single entry in dot-dot" -- the same
 * "nothing but the real '.'/'..' entries" definition of empty this
 * chapter's own is_dir_empty() checks directly against real, on-disk
 * entries), if the directory holds any real entry besides its own "."
 * and ".." pair. On success, frees every real cluster in the
 * directory's own chain back to both FAT copies (its whole chain, not
 * merely its first cluster -- a real subdirectory can genuinely grow
 * past one cluster the same way a file does, even though this
 * function only ever runs once it has already confirmed the directory
 * is empty) and marks its own entry in the root as free (the cited
 * 0xE5 "unused entry" marker), making its first cluster available to a
 * future fat16_create_file()/fat16_mkdir() call again -- the same
 * "matches the freed cluster?" proof this book has run on every real
 * allocator since Chapter 7's own physical memory manager. Returns 1
 * if a matching, genuinely empty directory was found and removed. */
int fat16_rmdir(const char *name);

/* Prints every real, currently-occupied entry in the root directory --
 * name (converted back from the on-disk 8.3 form to an ordinary
 * "NAME.EXT" string), a real "<DIR>" tag for any entry whose own
 * cited ATTR_DIRECTORY bit is set, and real file size for anything
 * else -- skipping every entry this chapter's own format/deletion
 * convention marks as free (0x00 end-of-directory or 0xE5 deleted).
 * Returns the real count printed. */
uint32_t fat16_list_root(void);

/* The same real listing fat16_list_root() prints, but for the real
 * subdirectory an arbitrarily deep `name` path resolves to (a bare
 * name directly under the root still works exactly as before) instead
 * of the root itself -- including that subdirectory's own real "." and
 * ".." entries, printed exactly as fat16_mkdir() wrote them, genuine
 * proof they are really sitting on disk rather than merely implied.
 * Refuses -- returns 0 -- if an intermediate path component doesn't
 * exist or isn't really a directory, or if no such subdirectory
 * exists. */
uint32_t fat16_list_dir(const char *name);

#endif
