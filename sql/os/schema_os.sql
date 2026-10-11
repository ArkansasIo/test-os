-- ---------------------------------------------------------------------------
-- osborne1-cpm : MySQL schema — OS layer
--
-- The emulator's operating-system structure, as reported by
--   osborne1 --list      (the init service table + runlevels)
--   osborne1 --menu      (the desktop menu tree)
--   osborne1 --user NAME (the account database)
--
-- This is deliberately separate from sql/schema.sql, which persists the CP/M
-- *filesystem* (drives, directory entries, extents, data blocks). Here we
-- persist how the machine and its desktop/server sessions are *organised*.
--
-- Depends on: sql/schema.sql (creates the osborne_cpm database + machines).
-- ---------------------------------------------------------------------------

USE osborne_cpm;

-- --- Runlevels -------------------------------------------------------------
-- INIT_RUNLEVEL_* from include/boot.h.
CREATE TABLE IF NOT EXISTS runlevels (
    id           TINYINT      NOT NULL PRIMARY KEY,   -- the runlevel number
    name         VARCHAR(32)  NOT NULL UNIQUE,
    description  VARCHAR(128) NOT NULL DEFAULT '',
    graphical    TINYINT(1)   NOT NULL DEFAULT 0,
    network      TINYINT(1)   NOT NULL DEFAULT 0
) ENGINE=InnoDB;

INSERT INTO runlevels (id, name, description, graphical, network) VALUES
    (0, 'halt',      'System halted',                 0, 0),
    (1, 'rescue',    'Single user, recovery shell',   0, 0),
    (3, 'multi',     'Text mode, no desktop',         0, 0),
    (5, 'graphical', 'Desktop session',               1, 0),
    (6, 'server',    'Headless services',             0, 1)
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- --- Boot targets ----------------------------------------------------------
-- enum boot_target from include/boot.h (desktop, server, shell, rescue).
CREATE TABLE IF NOT EXISTS boot_targets (
    id           TINYINT      NOT NULL PRIMARY KEY,
    name         VARCHAR(16)  NOT NULL UNIQUE,
    description  VARCHAR(128) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

INSERT INTO boot_targets (id, name, description) VALUES
    (0, 'desktop', 'Interactive desktop / CP/M terminal'),
    (1, 'server',  'Headless server, VNC + REST API'),
    (2, 'headless','Text shell, no display manager'),
    (3, 'rescue',  'Recovery session')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- --- Services --------------------------------------------------------------
-- The init service table (init.c service_add()).
--   binary       - the program the service starts (we own no real pid here)
--   runlevel_min - first runlevel at which the service may start
--   runlevel_max - last runlevel at which it may run (9 = "always")
--   depends_on   - the service that must be up first (NULL for boot-early)
--   essential    - 1 if the boot fails when this service will not start
CREATE TABLE IF NOT EXISTS os_services (
    id            SMALLINT     NOT NULL PRIMARY KEY,
    name          VARCHAR(32)  NOT NULL UNIQUE,
    binary        VARCHAR(64)  NOT NULL,
    runlevel_min  TINYINT      NOT NULL DEFAULT 3,
    runlevel_max  TINYINT      NOT NULL DEFAULT 9,
    depends_on    VARCHAR(32)  NULL,
    runlevel      TINYINT      NOT NULL DEFAULT 5,
    target        TINYINT      NOT NULL DEFAULT 0,
    essential     TINYINT(1)   NOT NULL DEFAULT 0,
    description   VARCHAR(160) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

-- --- Service <-> runlevel (which runlevels bring a service up) --------------
CREATE TABLE IF NOT EXISTS service_runlevels (
    service_id  SMALLINT NOT NULL,
    runlevel_id TINYINT  NOT NULL,
    PRIMARY KEY (service_id, runlevel_id),
    CONSTRAINT fk_sr_service  FOREIGN KEY (service_id)
        REFERENCES os_services(id) ON DELETE CASCADE,
    CONSTRAINT fk_sr_runlevel FOREIGN KEY (runlevel_id)
        REFERENCES runlevels(id)   ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- CPU / machine profile -------------------------------------------------
-- The hardware the emulator claims for itself (no longer hard-coded strings).
CREATE TABLE IF NOT EXISTS cpu_profiles (
    id           SMALLINT     NOT NULL PRIMARY KEY,
    name         VARCHAR(32)  NOT NULL UNIQUE,
    word_bits    TINYINT      NOT NULL DEFAULT 8,      -- Z80: 8-bit data path
    addr_bits    TINYINT      NOT NULL DEFAULT 16,     -- 16-bit address bus
    clock_hz     INT          NOT NULL DEFAULT 4000000,
    data_bus_bits TINYINT     NOT NULL DEFAULT 8,
    notes        VARCHAR(160) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

INSERT INTO cpu_profiles (id, name, word_bits, addr_bits, clock_hz, data_bus_bits, notes)
VALUES
    (1, 'Z80A', 8, 16, 4000000, 8,
     'Osborne 1 CPU: 8-bit ALU, 16-bit address bus, 4 MHz'),
    (2, '8086', 16, 20, 4772727, 16,
     'Not modelled; reference row for 16-bit comparison'),
    (3, '80386', 32, 32, 16000000, 32,
     'Not modelled; reference row for 32-bit comparison'),
    (4, 'x86_64', 64, 64, 3000000000, 64,
     'Not modelled; reference row for 64-bit comparison')
ON DUPLICATE KEY UPDATE name = VALUES(name), notes = VALUES(notes);

-- --- Seed the 22-service init table (mirrors `osborne1 --list`) -------------
INSERT INTO os_services
    (id, name, binary, runlevel_min, runlevel_max, depends_on, runlevel, target, essential, description)
VALUES
    ( 1, 'boot-early', 'os/boot/init',       0, 9, NULL,         5, 0, 1, 'Firmware POST + loader handoff'),
    ( 2, 'machine',    'src/machine/bus',    1, 9, 'boot-early', 5, 0, 1, 'Osborne-1 board: RAM, ROM, FDC, CRT'),
    ( 3, 'cpu',        'src/cpu/z80',        1, 9, 'machine',    5, 0, 1, 'Z80 instruction set'),
    ( 4, 'devices',    'os/core/dev',        1, 9, 'cpu',        5, 0, 1, 'Device nodes for the emulated board'),
    ( 5, 'vfs',        'os/core/vfs',        1, 9, 'devices',    5, 0, 1, 'Virtual filesystem'),
    ( 6, 'bus',        'os/core/bus',        1, 9, 'devices',    5, 0, 0, 'System message bus'),
    ( 7, 'net',        'os/core/net',        3, 9, 'bus',        5, 0, 0, 'Networking stack'),
    ( 8, 'cpm-disk',   'src/cpm/disk',       3, 9, 'vfs',        5, 0, 1, 'CP/M disk format + block layer'),
    ( 9, 'cpm-fcb',    'src/cpm/fcb',        3, 9, 'cpm-disk',   5, 0, 1, 'FCB / directory entry layer'),
    (10, 'cpm-bios',   'src/cpm/bios',       3, 9, 'cpm-disk',   5, 0, 1, 'CP/M 2.2 BIOS'),
    (11, 'cpm-bdos',   'src/cpm/bdos',       3, 9, 'cpm-fcb',    5, 0, 1, 'CP/M 2.2 BDOS'),
    (12, 'cpm-ccp',    'src/cpm/ccp',        3, 9, 'cpm-bdos',   5, 0, 1, 'Console Command Processor'),
    (13, 'hostname',   'os/boot/hostname',   3, 9, 'vfs',        5, 0, 0, 'Set the machine hostname'),
    (14, 'seed-users', 'os/seed/users',      3, 9, 'vfs',        5, 0, 1, 'Seed the account database'),
    (15, 'login',      'os/login/login',     3, 9, 'seed-users', 5, 0, 1, 'Login manager / autologin'),
    (16, 'db',         'os/server/mysqld',   3, 9, 'cpm-ccp',    5, 1, 0, 'MySQL persistence layer'),
    (17, 'modules',    'os/modules/modload', 3, 9, 'cpm-disk',   5, 0, 0, 'Module registry + loader'),
    (18, 'os-desktop', 'os/desktop/os-desk', 3, 9, 'login',      5, 0, 0, 'Desktop panel + app grid'),
    (19, 'os-menus',   'os/desktop/menus',   3, 9, 'os-desktop', 5, 0, 0, 'Desktop menu tree'),
    (20, 'os-served',  'os/server/os-served',6, 9, 'net',        6, 1, 0, 'Headless server session'),
    (21, 'vnc',        'os/server/vnc',      6, 9, 'os-served',  6, 1, 0, 'VNC server for the CRT'),
    (22, 'api',        'os/server/rest',     6, 9, 'os-served',  6, 1, 0, 'REST API')
ON DUPLICATE KEY UPDATE
    binary = VALUES(binary), depends_on = VALUES(depends_on),
    essential = VALUES(essential), description = VALUES(description);

-- runlevel 5 (graphical) brings up 1..19; runlevel 6 (server) brings up 1..22.
INSERT INTO service_runlevels (service_id, runlevel_id)
SELECT id, 5 FROM os_services WHERE runlevel_min <= 5
ON DUPLICATE KEY UPDATE runlevel_id = runlevel_id;

INSERT INTO service_runlevels (service_id, runlevel_id)
SELECT id, 6 FROM os_services
ON DUPLICATE KEY UPDATE runlevel_id = runlevel_id;

INSERT INTO service_runlevels (service_id, runlevel_id)
SELECT id, 3 FROM os_services WHERE runlevel_min <= 3 AND target = 0
ON DUPLICATE KEY UPDATE runlevel_id = runlevel_id;

-- ---------------------------------------------------------------------------
-- The rest of the OS layer: accounts, groups, modules, menus, sessions,
-- boot events, and the on-disk "save files" registry.
-- ---------------------------------------------------------------------------

-- --- Groups ----------------------------------------------------------------
CREATE TABLE IF NOT EXISTS groups (
    id          SMALLINT     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(32)  NOT NULL UNIQUE,
    gid         SMALLINT     NOT NULL UNIQUE,   -- numeric gid used in /etc/passwd terms
    description VARCHAR(128) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

INSERT INTO groups (name, gid, description) VALUES
    ('root',   0,  'Superuser group'),
    ('users',  100,'Ordinary interactive users'),
    ('wheel',  10, 'May use su/sudo to become root'),
    ('tty',    5,  'Owns the console / CP/M terminal'),
    ('floppy', 6,  'Raw access to the CP/M drive images'),
    ('db',     20, 'Owns the MySQL persistence layer')
ON DUPLICATE KEY UPDATE gid = VALUES(gid);

-- --- Accounts --------------------------------------------------------------
-- The account database that `login_user_find()` reads. The password column
-- stores a crypt(3)-style hash; 'x' means "shadowed elsewhere".
CREATE TABLE IF NOT EXISTS users (
    id            SMALLINT     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    username      VARCHAR(32)  NOT NULL UNIQUE,
    uid           SMALLINT     NOT NULL UNIQUE,
    gid           SMALLINT     NOT NULL,
    password_hash VARCHAR(128) NOT NULL DEFAULT 'x',
    full_name     VARCHAR(64)  NOT NULL DEFAULT '',
    home          VARCHAR(64)  NOT NULL DEFAULT 'A:',
    shell         VARCHAR(32)  NOT NULL DEFAULT 'ccp',
    login_shell   VARCHAR(32)  NOT NULL DEFAULT 'desktop',
    enabled       TINYINT(1)   NOT NULL DEFAULT 1,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_user_group FOREIGN KEY (gid)
        REFERENCES groups(gid) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB;

-- Seeded with the accounts the emulator ships with.
INSERT INTO users (username, uid, gid, password_hash, full_name, home, login_shell) VALUES
    ('root',    0,  0,   '*', 'System Administrator', 'A:',  'desktop'),
    ('osborne', 1000,100, '*', 'Osborne Operator',     'A:',  'desktop'),
    ('muthur',  1001,100, '*', 'MU/TH/UR 6000',        'A:',  'muthur'),
    ('guest',   1002,100, '',  'Guest account',        'B:',  'menu')
ON DUPLICATE KEY UPDATE uid = VALUES(uid), full_name = VALUES(full_name);

-- --- User <-> group membership (many-to-many, beyond the primary gid) ------
CREATE TABLE IF NOT EXISTS user_groups (
    user_id  SMALLINT NOT NULL,
    group_id SMALLINT NOT NULL,
    PRIMARY KEY (user_id, group_id),
    CONSTRAINT fk_ug_user  FOREIGN KEY (user_id)
        REFERENCES users(id)  ON DELETE CASCADE,
    CONSTRAINT fk_ug_group FOREIGN KEY (group_id)
        REFERENCES groups(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- Modules ---------------------------------------------------------------
-- The loadable-module registry. `modules/modload` reads this at runlevel 5.
CREATE TABLE IF NOT EXISTS os_modules (
    id           SMALLINT     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name         VARCHAR(32)  NOT NULL UNIQUE,
    path         VARCHAR(96)  NOT NULL,
    version      VARCHAR(16)  NOT NULL DEFAULT '1.0',
    autoload     TINYINT(1)   NOT NULL DEFAULT 0,
    loaded       TINYINT(1)   NOT NULL DEFAULT 0,
    refcount     INT          NOT NULL DEFAULT 0,
    description  VARCHAR(160) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

INSERT INTO os_modules (name, path, version, autoload, loaded, description) VALUES
    ('z80',      'src/cpu/z80.o',        '2.2', 1, 1, 'Z80 CPU core'),
    ('bus',      'src/machine/bus.o',    '2.2', 1, 1, 'Memory bus + I/O ports'),
    ('video',    'src/machine/video.o',  '2.2', 1, 1, '80x24 CRT emulation'),
    ('keyboard', 'src/machine/kbd.o',    '2.2', 1, 1, 'Osborne-1 keyboard matrix'),
    ('fdc',      'src/machine/fdc.o',    '2.2', 1, 1, 'Floppy disk controller'),
    ('bdos',     'src/cpm/bdos.o',       '2.2', 1, 1, 'CP/M 2.2 BDOS'),
    ('bios',     'src/cpm/bios.o',       '2.2', 1, 1, 'CP/M 2.2 BIOS'),
    ('ccp',      'src/cpm/ccp.o',        '2.2', 1, 1, 'Console Command Processor'),
    ('desktop',  'os/desktop/desk.o',    '0.1', 1, 0, 'Desktop panel'),
    ('vnc',      'os/server/vnc.o',      '0.1', 0, 0, 'VNC server (server target only)'),
    ('db',       'os/server/mysqld.o',   '0.1', 0, 0, 'MySQL persistence layer'),
    ('vfs',      'os/core/vfs.o',        '0.1', 1, 1, 'Virtual filesystem')
ON DUPLICATE KEY UPDATE path = VALUES(path), version = VALUES(version);

-- --- Desktop menus ---------------------------------------------------------
-- Flattens os/desktop/menus/*.json. parent_id NULL = a top-level entry.
CREATE TABLE IF NOT EXISTS menu_items (
    id          INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    menu_file   VARCHAR(64)  NOT NULL DEFAULT 'main-menu.json',
    node_id     VARCHAR(32)  NOT NULL,
    parent_id   INT          NULL,
    label       VARCHAR(64)  NOT NULL,
    icon        VARCHAR(32)  NOT NULL DEFAULT '',
    action      VARCHAR(128) NULL,           -- 'launch:...' for a leaf
    sort_order  INT          NOT NULL DEFAULT 0,
    UNIQUE KEY uq_menu_node (menu_file, node_id),
    CONSTRAINT fk_menu_parent FOREIGN KEY (parent_id)
        REFERENCES menu_items(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- Sessions --------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sessions (
    id          INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    user_id     SMALLINT     NOT NULL,
    tty         VARCHAR(16)  NOT NULL DEFAULT 'tty1',
    kind        ENUM('desktop','server','shell','rescue','muthur') NOT NULL DEFAULT 'desktop',
    runlevel    TINYINT      NOT NULL DEFAULT 5,
    target      TINYINT      NOT NULL DEFAULT 0,
    started_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    ended_at    DATETIME     NULL,
    end_reason  VARCHAR(64)  NULL,
    CONSTRAINT fk_session_user FOREIGN KEY (user_id)
        REFERENCES users(id) ON DELETE CASCADE,
    KEY idx_session_open (ended_at)
) ENGINE=InnoDB;

-- --- Boot events -----------------------------------------------------------
-- One row per boot stage transition (see boot.h BOOT_STAGE_*).
CREATE TABLE IF NOT EXISTS boot_events (
    id          BIGINT       NOT NULL AUTO_INCREMENT PRIMARY KEY,
    machine_id  INT          NULL,
    stage       TINYINT      NOT NULL,          -- BOOT_STAGE_POST..READY
    stage_name  VARCHAR(32)  NOT NULL,
    runlevel    TINYINT      NOT NULL,
    target      TINYINT      NOT NULL DEFAULT 0,
    ok          TINYINT(1)   NOT NULL DEFAULT 1,
    detail      VARCHAR(200) NOT NULL DEFAULT '',
    logged_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_boot_time (logged_at),
    KEY idx_boot_stage (stage),
    CONSTRAINT fk_boot_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------------
-- SAVE FILES : "save and load everything"
--
-- A save file is a complete, named snapshot of the machine: its hardware
-- inventory, the running service table, the open sessions and the CP/M
-- drive images. `save_slots` is the registry; the payload tables below hold
-- the snapshot body. Loading a save re-materialises all of it.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS save_slots (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name         VARCHAR(64)  NOT NULL UNIQUE,   -- user-facing slot name
    machine_id   INT          NULL,
    description  VARCHAR(200) NOT NULL DEFAULT '',
    format_ver   SMALLINT     NOT NULL DEFAULT 1,
    bytes        BIGINT       NOT NULL DEFAULT 0,
    created_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    loaded_at    DATETIME     NULL,
    load_count   INT          NOT NULL DEFAULT 0,
    checksum     BIGINT       NOT NULL DEFAULT 0,
    CONSTRAINT fk_save_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- CPU / RAM / GPU / storage / other components captured in a save.
CREATE TABLE IF NOT EXISTS save_components (
    id          BIGINT       NOT NULL AUTO_INCREMENT PRIMARY KEY,
    save_id     INT          NOT NULL,
    kind        ENUM('cpu','ram','gpu','hdd','ssd','floppy','rom','nic','sound','serial','other')
                             NOT NULL,
    name        VARCHAR(64)  NOT NULL,
    slot_index  INT          NOT NULL DEFAULT 0,   -- 0,1,2... for multi-instance
    model       VARCHAR(64)  NOT NULL DEFAULT '',
    capacity    BIGINT       NOT NULL DEFAULT 0,   -- bytes, or 0 if n/a
    clock_hz    INT          NOT NULL DEFAULT 0,
    word_bits   TINYINT      NOT NULL DEFAULT 0,
    attrs       JSON         NULL,
    UNIQUE KEY uq_save_component (save_id, kind, slot_index),
    CONSTRAINT fk_scomp_save FOREIGN KEY (save_id)
        REFERENCES save_slots(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Software inventory captured in a save (the "other computer software
-- component" list): services, modules, packages, versions.
CREATE TABLE IF NOT EXISTS save_software (
    id          BIGINT       NOT NULL AUTO_INCREMENT PRIMARY KEY,
    save_id     INT          NOT NULL,
    component   VARCHAR(64)  NOT NULL,     -- service / module / package name
    kind        ENUM('service','module','package','firmware','driver') NOT NULL,
    version     VARCHAR(24)  NOT NULL DEFAULT '',
    state       VARCHAR(24)  NOT NULL DEFAULT 'stopped',
    UNIQUE KEY uq_save_sw (save_id, component, kind),
    CONSTRAINT fk_ssw_save FOREIGN KEY (save_id)
        REFERENCES save_slots(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- The running service table and open sessions, snapshotted verbatim.
CREATE TABLE IF NOT EXISTS save_services (
    save_id     INT          NOT NULL,
    service_id  SMALLINT     NOT NULL,
    name        VARCHAR(32)  NOT NULL,
    state       ENUM('running','stopped') NOT NULL DEFAULT 'stopped',
    started_at  DATETIME     NULL,
    PRIMARY KEY (save_id, service_id),
    CONSTRAINT fk_ssvc_save FOREIGN KEY (save_id)
        REFERENCES save_slots(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS save_sessions (
    save_id     INT          NOT NULL,
    session_id  INT          NOT NULL,
    user_name   VARCHAR(32)  NOT NULL,
    tty         VARCHAR(16)  NOT NULL,
    kind        VARCHAR(16)  NOT NULL,
    PRIMARY KEY (save_id, session_id),
    CONSTRAINT fk_sses_save FOREIGN KEY (save_id)
        REFERENCES save_slots(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------------
-- HARDWARE INVENTORY : multi-HDD, RAM, CPU, GPU and other components.
--
-- One `machines` row can own many CPUs, RAM modules, GPUs, storage devices
-- and peripherals — the emulator models a single Z80, but the inventory is
-- general so a multi-device machine can be described and saved.
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS hw_component_kinds (
    id        TINYINT     NOT NULL PRIMARY KEY,
    name      VARCHAR(16) NOT NULL UNIQUE,
    removable TINYINT(1)  NOT NULL DEFAULT 0
) ENGINE=InnoDB;

INSERT INTO hw_component_kinds (id, name, removable) VALUES
    (1,'cpu',0),(2,'ram',1),(3,'gpu',0),(4,'hdd',1),(5,'ssd',1),
    (6,'floppy',1),(7,'rom',0),(8,'nic',0),(9,'sound',0),(10,'serial',0),(11,'other',1)
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- --- CPUs: a machine may hold more than one -------------------------------
CREATE TABLE IF NOT EXISTS machine_cpus (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    machine_id   INT          NOT NULL,
    slot_index   INT          NOT NULL DEFAULT 0,     -- socket 0, 1, 2...
    profile_id   SMALLINT     NULL,                   -- -> cpu_profiles
    model        VARCHAR(48)  NOT NULL DEFAULT 'Z80A',
    arch_bits    TINYINT      NOT NULL DEFAULT 8,
    addr_bits    TINYINT      NOT NULL DEFAULT 16,
    cores        TINYINT      NOT NULL DEFAULT 1,
    clock_hz     INT          NOT NULL DEFAULT 4000000,
    enabled      TINYINT(1)   NOT NULL DEFAULT 1,
    UNIQUE KEY uq_machine_cpu (machine_id, slot_index),
    CONSTRAINT fk_mcpu_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE CASCADE,
    CONSTRAINT fk_mcpu_profile FOREIGN KEY (profile_id)
        REFERENCES cpu_profiles(id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- --- RAM: multiple modules, with their size and speed ---------------------
CREATE TABLE IF NOT EXISTS machine_ram (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    machine_id   INT          NOT NULL,
    slot_index   INT          NOT NULL DEFAULT 0,
    size_kb      INT          NOT NULL DEFAULT 64,
    width_bits   TINYINT      NOT NULL DEFAULT 8,     -- Z80 data path
    access_ns    INT          NOT NULL DEFAULT 0,
    installed    TINYINT(1)   NOT NULL DEFAULT 1,
    UNIQUE KEY uq_machine_ram (machine_id, slot_index),
    CONSTRAINT fk_mram_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- GPUs / display adapters ----------------------------------------------
CREATE TABLE IF NOT EXISTS machine_gpus (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    machine_id   INT          NOT NULL,
    slot_index   INT          NOT NULL DEFAULT 0,
    model        VARCHAR(48)  NOT NULL DEFAULT 'Osborne CRT',
    vram_kb      INT          NOT NULL DEFAULT 4,
    cols         SMALLINT     NOT NULL DEFAULT 80,
    rows         SMALLINT     NOT NULL DEFAULT 24,
    colour       TINYINT(1)   NOT NULL DEFAULT 0,     -- 0 = monochrome green
    enabled      TINYINT(1)   NOT NULL DEFAULT 1,
    UNIQUE KEY uq_machine_gpu (machine_id, slot_index),
    CONSTRAINT fk_mgpu_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- Storage: multi-HDD / SSD / floppy ------------------------------------
CREATE TABLE IF NOT EXISTS machine_storage (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    machine_id   INT          NOT NULL,
    kind         ENUM('hdd','ssd','floppy','rom') NOT NULL DEFAULT 'floppy',
    slot_index   INT          NOT NULL DEFAULT 0,
    letter       CHAR(1)      NULL,                   -- 'A'..'P' or NULL
    model        VARCHAR(48)  NOT NULL DEFAULT '',
    capacity_kb  INT          NOT NULL DEFAULT 0,
    image_path   VARCHAR(128) NOT NULL DEFAULT '',    -- disks/A.dsk
    read_only    TINYINT(1)   NOT NULL DEFAULT 0,
    UNIQUE KEY uq_machine_store (machine_id, kind, slot_index),
    CONSTRAINT fk_mstore_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- Other peripherals (NIC, sound, serial, anything else) ----------------
CREATE TABLE IF NOT EXISTS machine_peripherals (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    machine_id   INT          NOT NULL,
    kind_id      TINYINT      NOT NULL,
    slot_index   INT          NOT NULL DEFAULT 0,
    model        VARCHAR(48)  NOT NULL DEFAULT '',
    attrs        JSON         NULL,
    UNIQUE KEY uq_machine_periph (machine_id, kind_id, slot_index),
    CONSTRAINT fk_mper_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE CASCADE,
    CONSTRAINT fk_mper_kind FOREIGN KEY (kind_id)
        REFERENCES hw_component_kinds(id) ON DELETE RESTRICT
) ENGINE=InnoDB;

-- --- Seed the inventory for the default `osborne1` machine ----------------
INSERT INTO machine_cpus (machine_id, slot_index, profile_id, model, arch_bits, addr_bits, clock_hz)
SELECT m.id, 0, 1, 'Z80A', 8, 16, 4000000 FROM machines m WHERE m.name='osborne1'
ON DUPLICATE KEY UPDATE model = VALUES(model);

INSERT INTO machine_ram (machine_id, slot_index, size_kb, width_bits, access_ns)
SELECT m.id, 0, 64, 8, 250 FROM machines m WHERE m.name='osborne1'
ON DUPLICATE KEY UPDATE size_kb = VALUES(size_kb);

INSERT INTO machine_gpus (machine_id, slot_index, model, vram_kb, cols, rows, colour)
SELECT m.id, 0, 'Osborne CRT', 4, 80, 24, 0 FROM machines m WHERE m.name='osborne1'
ON DUPLICATE KEY UPDATE model = VALUES(model);

INSERT INTO machine_storage (machine_id, kind, slot_index, letter, model, capacity_kb, image_path)
SELECT m.id, 'floppy', 0, 'A', 'SSDD 5.25"', 100, 'disks/boot.cpm'
FROM machines m WHERE m.name='osborne1'
ON DUPLICATE KEY UPDATE image_path = VALUES(image_path);

INSERT INTO machine_storage (machine_id, kind, slot_index, letter, model, capacity_kb, image_path)
SELECT m.id, 'floppy', 1, 'B', 'SSDD 5.25"', 100, 'disks/blank.dsk'
FROM machines m WHERE m.name='osborne1'
ON DUPLICATE KEY UPDATE image_path = VALUES(image_path);
