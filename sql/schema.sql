and systems
-- ---------------------------------------------------------------------------
-- osborne1-cpm : MySQL schema
-- Persistence layer for the CP/M filesystem (drives, directory entries, blocks)
-- ---------------------------------------------------------------------------

CREATE DATABASE IF NOT EXISTS osborne_cpm
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_general_ci;

USE osborne_cpm;

-- Machine instances: one row per emulated Osborne 1.
CREATE TABLE IF NOT EXISTS machines (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(64)  NOT NULL UNIQUE,
    rom_version VARCHAR(16)  NOT NULL DEFAULT 'CP/M 2.2',
    ram_kb      SMALLINT     NOT NULL DEFAULT 64,
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP
                             ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- Drives: A: and B: floppies attached to a machine.
CREATE TABLE IF NOT EXISTS drives (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    machine_id  INT          NOT NULL,
    letter      CHAR(1)      NOT NULL,          -- 'A' .. 'P'
    label       VARCHAR(16)  NOT NULL DEFAULT 'BLANK',
    read_only   TINYINT(1)   NOT NULL DEFAULT 0,
    tracks      SMALLINT     NOT NULL DEFAULT 40,
    sectors     SMALLINT     NOT NULL DEFAULT 9,
    sector_size SMALLINT     NOT NULL DEFAULT 512,
    free_blocks INT          NOT NULL DEFAULT 0,
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_drive (machine_id, letter),
    CONSTRAINT fk_drive_machine FOREIGN KEY (machine_id)
        REFERENCES machines(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Directory entries: the CP/M 8.3 filename records, one per extent.
CREATE TABLE IF NOT EXISTS files (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    drive_id    INT          NOT NULL,
    user_no     TINYINT      NOT NULL DEFAULT 0,  -- CP/M user area 0..15
    name        CHAR(8)      NOT NULL,            -- padded with spaces
    type        CHAR(3)      NOT NULL,            -- padded with spaces
    extent      TINYINT      NOT NULL DEFAULT 0,
    rec_count   TINYINT      NOT NULL DEFAULT 0,
    size_bytes  BIGINT       NOT NULL DEFAULT 0,
    read_only   TINYINT(1)   NOT NULL DEFAULT 0,
    system      TINYINT(1)   NOT NULL DEFAULT 0,  -- SYS attribute
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP
                             ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uq_dir (drive_id, user_no, name, type, extent),
    KEY idx_lookup (drive_id, user_no, name, type),
    CONSTRAINT fk_file_drive FOREIGN KEY (drive_id)
        REFERENCES drives(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Extent allocation map: the 16 x 8-bit block pointers from each FCB.
CREATE TABLE IF NOT EXISTS extents (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    file_id     BIGINT       NOT NULL,
    slot        TINYINT      NOT NULL,   -- 0..15 position in d[]
    block_no    SMALLINT     NOT NULL,   -- 0 = hole
    UNIQUE KEY uq_slot (file_id, slot),
    CONSTRAINT fk_extent_file FOREIGN KEY (file_id)
        REFERENCES files(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Raw 128-byte CP/M records, stored per 1 KB block.
CREATE TABLE IF NOT EXISTS data_blocks (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    file_id     BIGINT       NOT NULL,
    block_index INT          NOT NULL,
    payload     VARBINARY(1024) NOT NULL,   -- one CP/M block
    checksum    BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uq_block (file_id, block_index),
    CONSTRAINT fk_block_file FOREIGN KEY (file_id)
        REFERENCES files(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- CCP command history for the terminal front end.
CREATE TABLE IF NOT EXISTS history (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    machine_id  INT          NULL,
    command     VARCHAR(128) NOT NULL,
    ran_at      TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_machine (machine_id),
    KEY idx_time (ran_at)
) ENGINE=InnoDB;

-- Emulator event log (cold boots, disk errors, BDOS faults).
CREATE TABLE IF NOT EXISTS event_log (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    machine_id  INT          NULL,
    severity    ENUM('debug','info','warn','error') NOT NULL DEFAULT 'info',
    source      VARCHAR(32)  NOT NULL,  -- 'bios','bdos','fdc','cpu'
    message     VARCHAR(255) NOT NULL,
    logged_at   TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    KEY idx_sev_time (severity, logged_at)
) ENGINE=InnoDB;

-- Seed one machine and its two drives.
INSERT INTO machines (name, rom_version, ram_kb)
VALUES ('osborne1', 'CP/M 2.2', 64)
ON DUPLICATE KEY UPDATE name = name;

INSERT INTO drives (machine_id, letter, label, read_only, free_blocks)
SELECT m.id, 'A', 'SYSTEM', 0, 0 FROM machines m WHERE m.name = 'osborne1'
ON DUPLICATE KEY UPDATE label = VALUES(label);

INSERT INTO drives (machine_id, letter, label, read_only, free_blocks)
SELECT m.id, 'B', 'BLANK', 0, 0 FROM machines m WHERE m.name = 'osborne1'
ON DUPLICATE KEY UPDATE label = VALUES(label);
