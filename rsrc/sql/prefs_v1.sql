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

--- UUID prefs table
-- This is basically the same as the blob table but differentiated for type safety
CREATE TABLE prefs_uuid_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    value BLOB NOT NULL,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX prefs_uuid_name ON prefs_uuid_v1(key);

--- Blob prefs table
CREATE TABLE prefs_blob_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    value BLOB NOT NULL,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX prefs_blob_name ON prefs_blob_v1(key);
