-----------------------------------------------
--- User preferences file format, v1 schema ---
-----------------------------------------------
------
--- String prefs table
CREATE TABLE prefs_string_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    value TEXT,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX prefs_string_name ON prefs_string_v1(key);

------
--- number prefs table
CREATE TABLE prefs_number_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    value REAL NOT NULL DEFAULT 0,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX prefs_number_name ON prefs_number_v1(key);

