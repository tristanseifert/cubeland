-----------------------------------------------
--- Server key cache file format, v1 schema ---
-----------------------------------------------
------
--- keys table
CREATE TABLE keys_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    uuid BLOB UNIQUE NOT NULL,
    pubkey TEXT NOT NULL,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP,
    expires DATETIME DEFAULT 0
);
CREATE UNIQUE INDEX keys_player_id ON keys_v1(uuid);
