-- ---------------------------------------------------------------------------
-- osborne1-cpm : common queries used by the CP/M layer
-- ---------------------------------------------------------------------------

USE osborne_cpm;

-- Directory listing of drive A: (like `DIR` at the CCP prompt)
SELECT name, type, extent, size_bytes, read_only, system
FROM files f
JOIN drives d ON d.id = f.drive_id
WHERE d.letter = 'A' AND f.user_no = 0
ORDER BY name, type, extent;

-- Free space on each drive (in CP/M 1K blocks)
SELECT d.letter, d.label, d.tracks, d.sectors, d.sector_size, d.free_blocks
FROM drives d
ORDER BY d.letter;

-- Look up a single file across extents, total its size
SELECT f.name, f.type, COUNT(*) AS extents, SUM(f.size_bytes) AS total_bytes
FROM files f
JOIN drives d ON d.id = f.drive_id
WHERE d.letter = 'B' AND f.name = 'HELLO   ' AND f.type = 'COM'
GROUP BY f.name, f.type;

-- Files that are read-only or marked SYS
SELECT d.letter, f.name, f.type, f.read_only, f.system
FROM files f
JOIN drives d ON d.id = f.drive_id
WHERE f.read_only = 1 OR f.system = 1
ORDER BY d.letter, f.name;

-- Allocation map for one file (the FCB d[] vector)
SELECT e.slot, e.block_no
FROM extents e
JOIN files f ON f.id = e.file_id
WHERE f.name = 'HELLO   ' AND f.type = 'COM'
ORDER BY e.slot;

-- Reclaim freed blocks: drop content rows no longer referenced
SELECT f.id, f.name, f.type, COUNT(b.id) AS blocks, SUM(LENGTH(b.payload)) AS bytes
FROM files f
LEFT JOIN data_blocks b ON b.file_id = f.id
GROUP BY f.id, f.name, f.type;

-- Recent CCP command history
SELECT command, ran_at
FROM history
ORDER BY ran_at DESC
LIMIT 20;

-- Errors in the last day
SELECT severity, source, message, logged_at
FROM event_log
WHERE severity IN ('warn','error')
  AND logged_at >= NOW() - INTERVAL 1 DAY
ORDER BY logged_at DESC;

-- Drive usage report: per-drive file and byte counts
SELECT d.letter,
       COUNT(f.id)               AS file_count,
       COALESCE(SUM(f.size_bytes),0) AS total_bytes,
       d.free_blocks
FROM drives d
LEFT JOIN files f ON f.drive_id = d.id
GROUP BY d.letter, d.free_blocks
ORDER BY d.letter;
