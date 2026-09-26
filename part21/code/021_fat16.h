#ifndef UNIX_OS_021_FAT16_H
#define UNIX_OS_021_FAT16_H

#include <stdint.h>

/* This chapter's own real filesystem, still the same genuine FAT16
 * volume Chapter 20 built on top of Chapter 19's own real
 * ata_read_sector()/ata_write_sector() -- cited field-for-field from
 * OSDev Wiki ("FAT": https://wiki.osdev.org/FAT) for every field this
 * file already used, and from the Microsoft FAT specification (the
 * primary source OSDev's own "FAT" page cites in its External Links)
 * for this chapter's own new subdirectory conventions. This chapter's
 * own new work is real subdirectories: fat16_mkdir() creates a real
 * on-disk subdirectory, and fat16_create_file()/fat16_read_file()/
 * fat16_delete_file() now accept a real one-level "DIR/NAME.EXT" path,
 * not just a bare root-level name. Deliberately scoped to ONE level of
 * nesting -- "DIR/NAME.EXT" is supported, "DIR/SUB/NAME.EXT" is not,
 * and fat16_mkdir() itself refuses any name containing '/' -- a
 * genuine recursive multi-level path walker is a natural further
 * extension this chapter explicitly does not attempt, the same kind
 * of stated scope limit this book has drawn before (Chapter 19's own
 * raw-sector-only scope, Chapter 20's own flat-root-only scope, one
 * chapter each before this one). */

/* Every real filename or path this API accepts or returns is a plain,
 * NUL-terminated string. A bare "NAME.EXT" (or "NAME" with no
 * extension) refers to the root directory, exactly as in Chapter 20.
 * "DIR/NAME.EXT" refers to a real file inside the real subdirectory
 * "DIR", which must already exist (via fat16_mkdir()) directly under
 * the root -- never more than one real '/' is ever resolved. Both the
 * directory name and the leaf name are ordinary 8.3 names (at most 8
 * name characters, 3 extension characters), silently truncated by
 * this chapter's own to_83_name() (021_fat16.c) exactly as before; an
 * honest, stated limit this chapter's own real demo never needs to
 * exercise. */

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

/* Creates a new real subdirectory directly under the root -- `name`
 * must not contain '/' (refused outright if it does, this chapter's
 * own stated one-level-of-nesting scope). Allocates one real cluster
 * for the subdirectory's own contents and writes two real, cited
 * on-disk entries into it before anything else: a "." entry whose own
 * cluster fields self-reference this same new cluster, and a ".."
 * entry whose cluster fields are set to 0 -- cited directly (Microsoft
 * FAT specification, Section 6.5: "If the parent of the current
 * directory is the root directory ... the DIR_FstClusLO and
 * DIR_FstClusHI contents must be set to 0"), true here every time
 * since this function only ever creates a subdirectory of the root.
 * Then writes a real directory entry for `name` into the root itself,
 * with the cited ATTR_DIRECTORY bit (0x10) set and DIR_FileSize left
 * at 0 even though a real cluster is allocated -- cited directly
 * (Microsoft FAT specification, Section 6.5: "The DIR_FileSize must be
 * set to 0"). Refuses -- returns 0 -- if `name` contains '/', if a
 * root entry with this name already exists, if the root has no free
 * entry left, or if the volume has no free clusters left. */
int fat16_mkdir(const char *name);

/* Creates a new real file: allocates however many whole clusters
 * `size` real bytes need, chains them together in both real FAT
 * copies, writes `size` real bytes across them (the last, partial
 * cluster is zero-padded), and writes a real 32-byte directory entry
 * naming the file, its size, and its first cluster -- into the root
 * directory for a bare name, or into the named real subdirectory for
 * a "DIR/NAME.EXT" path (this chapter's own new work; see the top of
 * this file for the exact one-level scope). Refuses -- returns 0,
 * writes nothing -- if a file with this name already exists, if a
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
 * resolves to a real directory rather than a file -- there is no
 * fat16_rmdir() in this chapter; deleting a directory is explicitly
 * out of this chapter's own stated scope. Returns 1 if a matching file
 * was found and deleted. */
int fat16_delete_file(const char *name);

/* Prints every real, currently-occupied entry in the root directory --
 * name (converted back from the on-disk 8.3 form to an ordinary
 * "NAME.EXT" string), a real "<DIR>" tag for any entry whose own
 * cited ATTR_DIRECTORY bit is set, and real file size for anything
 * else -- skipping every entry this chapter's own format/deletion
 * convention marks as free (0x00 end-of-directory or 0xE5 deleted).
 * Returns the real count printed. */
uint32_t fat16_list_root(void);

/* The same real listing fat16_list_root() prints, but for the real
 * subdirectory named `name` (which must exist directly under the
 * root) instead of the root itself -- including that subdirectory's
 * own real "." and ".." entries, printed exactly as this chapter's
 * own fat16_mkdir() wrote them, genuine proof they are really sitting
 * on disk rather than merely implied. Refuses -- returns 0 -- if no
 * such subdirectory exists. */
uint32_t fat16_list_dir(const char *name);

#endif
