#ifndef OSB_DB_H
#define OSB_DB_H

#include "types.h"

/* MySQL-backed persistence for the CP/M filesystem layer.
 * Optional: only compiled in when OSB_WITH_MYSQL is defined. */

#define DB_OK          0
#define DB_ERR        -1
#define DB_ENOCONN    -2
#define DB_EQUERY     -3
#define DB_ENOTFOUND  -4

struct db;

/* Connection settings, loaded from env or config. */
struct db_config {
    const char *host;
    const char *port;
    const char *user;
    const char *password;
    const char *database;
};

struct db *db_open(const struct db_config *cfg);
void       db_close(struct db *db);
const char *db_last_error(struct db *db);

int db_exec(struct db *db, const char *sql);
int db_ping(struct db *db);

/* --- CP/M drive table ------------------------------------------------- */
int db_drive_upsert(struct db *db, char letter, const char *label, int ro);
int db_drive_get(struct db *db, char letter, char *label, size_t labellen, int *ro);
int db_drive_list(struct db *db);

/* --- CP/M file records ------------------------------------------------ */
struct db_file_row {
    long id;
    char drive;
    int  user;
    char name[9];   /* 8.3 padded with spaces */
    char type[4];
    int  extent;
    long size;
    int  read_only;
};

int db_file_insert(struct db *db, char drive, int user,
                   const char *name, const char *type,
                   int extent, long size);
int db_file_find(struct db *db, char drive, int user,
                 const char *name, const char *type,
                 struct db_file_row *out);
int db_file_delete(struct db *db, char drive, int user,
                   const char *name, const char *type);
int db_file_list(struct db *db, char drive, int user);

/* --- File contents ---------------------------------------------------- */
int db_block_write(struct db *db, long file_id, int block, const u8 *data, size_t len);
int db_block_read(struct db *db, long file_id, int block, u8 *data, size_t len);

/* --- CCP command history --------------------------------------------- */
int db_history_add(struct db *db, const char *cmd);
int db_history_list(struct db *db, int limit);

#endif /* OSB_DB_H */
