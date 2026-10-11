-- osborne1-cpm : MySQL schema — desktop GUI API
--
-- Persists the surface described by include/gui.h:
--   apps            the launcher / app grid
--   gui_themes      the colour+glyph sets every surface renders through
--   gui_windows     the open window model (position, size, z-order)
--   gui_widgets     the controls inside each window, in stack order
--   gui_panels      the edge panels (top panel / statusbar)
--   gui_routes      the API route table (what "gui:<route>" resolves to)
--   gui_keybinds    key -> action mappings for a session
--
-- The menu tree itself lives in sql/os/schema_os.sql (menu_items); this file
-- is what a menu action *launches into*, and the route table is what
-- os/server/rest exposes over HTTP.
--
-- Depends on: sql/schema.sql (creates the osborne_cpm database + machines),
--             sql/os/schema_os.sql (runlevels, users, sessions).
-- ---------------------------------------------------------------------------

USE osborne_cpm;

-- --- Themes ----------------------------------------------------------------
-- Mirrors struct gui_theme. `fg` / `bg` / `accent` / `dim` / `alert` are the
-- ANSI SGR colour codes screen.h defines (A_GREEN = 32, ...).
CREATE TABLE IF NOT EXISTS gui_themes (
    id          VARCHAR(32)  NOT NULL PRIMARY KEY,
    fg          TINYINT      NOT NULL DEFAULT 32,
    bg          TINYINT      NOT NULL DEFAULT 30,
    accent      TINYINT      NOT NULL DEFAULT 33,   -- 32 | ATTR_BOLD
    dim         TINYINT      NOT NULL DEFAULT 34,   -- 32 | ATTR_DIM
    alert       TINYINT      NOT NULL DEFAULT 31,   -- 31 | ATTR_BOLD
    border      VARCHAR(8)   NOT NULL DEFAULT '+-|',
    bullet      VARCHAR(4)   NOT NULL DEFAULT '*',
    description VARCHAR(160) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

INSERT INTO gui_themes (id, fg, bg, accent, dim, alert, border, bullet, description) VALUES
    ('nostromo-green', 32, 30, 33, 34, 31, '+-|', '*',
     'The Osborne phosphor green CRT'),
    ('nostromo-amber', 33, 30, 34, 35, 31, '+-|', '>',
     'Amber monochrome console'),
    ('weyland',        37, 34, 36, 35, 31, '+-|', '#',
     'Weyland-Yutani corporate blue')
ON DUPLICATE KEY UPDATE fg = VALUES(fg), accent = VALUES(accent);

-- --- App registry ----------------------------------------------------------
-- Mirrors struct gui_app; the app grid / launcher.
CREATE TABLE IF NOT EXISTS gui_apps (
    id          VARCHAR(32)  NOT NULL PRIMARY KEY,
    label       VARCHAR(64)  NOT NULL,
    icon        VARCHAR(32)  NOT NULL DEFAULT '',
    exec        VARCHAR(128) NOT NULL,              -- "launch:..." / a command
    category    VARCHAR(32)  NOT NULL DEFAULT 'System',
    pinnable    TINYINT(1)   NOT NULL DEFAULT 0,    -- may sit on the panel
    running     TINYINT(1)   NOT NULL DEFAULT 0,
    sort_order  INT          NOT NULL DEFAULT 0
) ENGINE=InnoDB;

INSERT INTO gui_apps (id, label, icon, exec, category, pinnable, sort_order) VALUES
    ('cpm',      'CP/M 2.2',      'terminal', 'osborne1 --disk A:boot.cpm', 'System',      1, 10),
    ('terminal', 'Terminal',      'terminal', 'xterm',                      'System',      1, 20),
    ('muthur',   'MU/TH/UR 6000', 'terminal', 'osborne1 --muthur',          'Ship',        1, 30),
    ('files',    'File Manager',  'folder',   'pcmanfm',                    'System',      0, 40),
    ('menu',     'Menu Tree',     'apps',     'osborne1 --menu',            'System',      0, 50),
    ('sound',    'Sound',         'media',    'osborne1 --sound-demo',      'Media',       0, 60),
    ('printer',  'Printer',       'printer',  'osborne1 --printer-demo',    'Media',       0, 70),
    ('dbview',   'Database',      'db',       'mysql-client',               'Development', 0, 80)
ON DUPLICATE KEY UPDATE label = VALUES(label), exec = VALUES(exec);

-- --- Panels ----------------------------------------------------------------
-- Mirrors struct gui_panel; one edge bar per desktop session.
CREATE TABLE IF NOT EXISTS gui_panels (
    id       VARCHAR(32) NOT NULL PRIMARY KEY,
    edge     ENUM('top','bottom','left','right') NOT NULL DEFAULT 'top',
    size     INT         NOT NULL DEFAULT 1,
    left_txt VARCHAR(128) NOT NULL DEFAULT '',
    right_txt VARCHAR(128) NOT NULL DEFAULT '',
    visible  TINYINT(1)  NOT NULL DEFAULT 1
) ENGINE=InnoDB;

INSERT INTO gui_panels (id, edge, size, left_txt, right_txt, visible) VALUES
    ('top', 'top', 1, 'USCSS NOSTROMO', 'tty1', 1)
ON DUPLICATE KEY UPDATE left_txt = VALUES(left_txt);

-- --- Windows ---------------------------------------------------------------
-- Mirrors struct gui_window. A window belongs to a session and optionally to
-- an app; `z` is the stacking order (higher = nearer the viewer).
CREATE TABLE IF NOT EXISTS gui_windows (
    id           INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    session_id   INT          NULL,
    window_id    VARCHAR(32)  NOT NULL,
    title        VARCHAR(64)  NOT NULL,
    app_id       VARCHAR(32)  NULL,
    x            INT          NOT NULL DEFAULT 0,
    y            INT          NOT NULL DEFAULT 0,
    width        INT          NOT NULL DEFAULT 40,
    height       INT          NOT NULL DEFAULT 10,
    z            INT          NOT NULL DEFAULT 0,
    visible      TINYINT(1)   NOT NULL DEFAULT 1,
    focused      TINYINT(1)   NOT NULL DEFAULT 0,
    modal        TINYINT(1)   NOT NULL DEFAULT 0,
    UNIQUE KEY uq_gui_window (session_id, window_id),
    KEY idx_gui_window_z (session_id, z),
    CONSTRAINT fk_gw_session FOREIGN KEY (session_id)
        REFERENCES sessions(id) ON DELETE CASCADE,
    CONSTRAINT fk_gw_app FOREIGN KEY (app_id)
        REFERENCES gui_apps(id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- --- Widgets ---------------------------------------------------------------
-- Mirrors struct gui_widget, one row per control inside a window. `text` is
-- the label/field value; list rows live in gui_widget_items.
CREATE TABLE IF NOT EXISTS gui_widgets (
    id          INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    window_id   INT          NOT NULL,
    widget_id   VARCHAR(32)  NOT NULL,
    label       VARCHAR(64)  NOT NULL DEFAULT '',
    kind        ENUM('label','button','field','list','meter','gauge','separator')
                             NOT NULL DEFAULT 'label',
    row         INT          NOT NULL DEFAULT 0,
    col         INT          NOT NULL DEFAULT 0,
    width       INT          NOT NULL DEFAULT 0,
    height      INT          NOT NULL DEFAULT 0,
    value       INT          NOT NULL DEFAULT 0,     -- meter / gauge 0..100
    text        VARCHAR(128) NOT NULL DEFAULT '',
    selected    INT          NOT NULL DEFAULT 0,
    enabled     TINYINT(1)   NOT NULL DEFAULT 1,
    sort_order  INT          NOT NULL DEFAULT 0,
    UNIQUE KEY uq_gui_widget (window_id, widget_id),
    CONSTRAINT fk_gwd_window FOREIGN KEY (window_id)
        REFERENCES gui_windows(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- A list widget's rows, in order.
CREATE TABLE IF NOT EXISTS gui_widget_items (
    id         INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    widget_id  INT          NOT NULL,
    item_index INT          NOT NULL DEFAULT 0,
    label      VARCHAR(64)  NOT NULL,
    UNIQUE KEY uq_gui_item (widget_id, item_index),
    CONSTRAINT fk_git_widget FOREIGN KEY (widget_id)
        REFERENCES gui_widgets(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- --- API routes ------------------------------------------------------------
-- Mirrors struct gui_route: the route table "gui:<name>" resolves against.
-- `handler` names the C entry point; `rest_path` is the HTTP endpoint the
-- os/server/rest service exposes for the same surface.
CREATE TABLE IF NOT EXISTS gui_routes (
    id          SMALLINT     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(48)  NOT NULL UNIQUE,        -- route name (before ':')
    handler     VARCHAR(64)  NOT NULL,               -- C handler symbol
    rest_path   VARCHAR(96)  NOT NULL DEFAULT '',    -- /api/...
    args        VARCHAR(128) NOT NULL DEFAULT '',    -- <id> | list | close-all
    help        VARCHAR(160) NOT NULL DEFAULT '',
    enabled     TINYINT(1)   NOT NULL DEFAULT 1
) ENGINE=InnoDB;

INSERT INTO gui_routes (name, handler, rest_path, args, help) VALUES
    ('app',     'route_app',      '/api/apps',     '<id>|list',  'Open or close an app in the grid'),
    ('window',  'route_window',   '/api/windows',  'list|focus|close-all', 'Manage the window stack'),
    ('theme',   'route_theme',    '/api/theme',    '<preset|file>', 'Switch the desktop theme'),
    ('gui',     'route_menu',     '/api/menu',     '<node-id>|list', 'Run a desktop menu entry'),
    ('key',     'route_keybind',  '/api/keys',     '<key>|list', 'Dispatch a keybind'),
    ('modules', 'route_modules',  '/api/modules',  'list|load|unload-all', 'Module registry'),
    ('help',    'route_help',     '/api/help',     '',           'List the GUI API routes')
ON DUPLICATE KEY UPDATE handler = VALUES(handler), rest_path = VALUES(rest_path);

-- --- Keybinds --------------------------------------------------------------
-- Mirrors struct gui_keybind.
CREATE TABLE IF NOT EXISTS gui_keybinds (
    id         INT          NOT NULL AUTO_INCREMENT PRIMARY KEY,
    session_id INT          NULL,                    -- NULL = all sessions
    key_name   VARCHAR(16)  NOT NULL,                -- "F1", "ctrl+q"
    action     VARCHAR(48)  NOT NULL,                -- a route name
    label      VARCHAR(64)  NOT NULL DEFAULT '',
    UNIQUE KEY uq_gui_key (session_id, key_name),
    CONSTRAINT fk_gk_session FOREIGN KEY (session_id)
        REFERENCES sessions(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- The default bindings gui_keybinds_builtin() installs (session NULL = global).
INSERT INTO gui_keybinds (session_id, key_name, action, label) VALUES
    (NULL, 'F1',     'help',            'Help'),
    (NULL, 'F2',     'gui',             'MU/TH/UR console'),
    (NULL, 'F3',     'gui',             'Ship status'),
    (NULL, 'F4',     'window',          'Window list'),
    (NULL, 'F5',     'theme',           'Cycle theme'),
    (NULL, 'F10',    'window',          'Close all windows'),
    (NULL, 'ctrl+q', 'exit',            'Log out')
ON DUPLICATE KEY UPDATE action = VALUES(action), label = VALUES(label);

-- --- Convenience views -----------------------------------------------------

-- The live window stack for a session, front-most first.
CREATE OR REPLACE VIEW v_gui_window_stack AS
SELECT w.session_id, w.window_id, w.title, w.app_id,
       w.x, w.y, w.width, w.height, w.z, w.focused,
       (SELECT COUNT(*) FROM gui_widgets d WHERE d.window_id = w.id) AS widgets
FROM gui_windows w
WHERE w.visible = 1
ORDER BY w.session_id, w.z DESC;

-- The full API surface, as `gui:help` would print it.
CREATE OR REPLACE VIEW v_gui_api AS
SELECT name, rest_path, args, help
FROM gui_routes
WHERE enabled = 1
ORDER BY name;
