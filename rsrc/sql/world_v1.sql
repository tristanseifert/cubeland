------------------------------------
--- World file format, v1 schema ---
------------------------------------
------
--- World info table
CREATE TABLE worldinfo_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    value BLOB,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX worldinfo_name ON worldinfo_v1(name);



------
--- Player data table
CREATE TABLE player_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    uuid BLOB NOT NULL,
    name TEXT,
    spawnX REAL DEFAULT 0,
    spawnY REAL DEFAULT 0,
    spawnZ REAL DEFAULT 0,
    currentX REAL DEFAULT 0,
    currentY REAL DEFAULT 0,
    currentZ REAL DEFAULT 0,
    created DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP,
    lastPlayed DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX player_name ON player_v1(name);
CREATE INDEX player_uuid ON player_v1(uuid);

------
--- Player info table
CREATE TABLE playerinfo_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    playerId INTEGER NOT NULL,
    name TEXT NOT NULL,
    value BLOB,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY(playerId) REFERENCES player_v1(id) ON DELETE CASCADE ON UPDATE CASCADE
);
CREATE INDEX playerinfo_playerid ON playerinfo_v1(playerId);
CREATE INDEX playerinfo_name ON playerinfo_v1(name);



------
--- Chunk table
CREATE TABLE chunk_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    worldX INTEGER NOT NULL,
    worldZ INTEGER NOT NULL,

    metadata BLOB DEFAULT NULL,

    created DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX chunk_pos ON chunk_v1(worldX, worldZ);
------
--- Chunk slice table
CREATE TABLE chunk_slice_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    chunkId INTEGER NOT NULL,
    chunkY INTEGER NOT NULL,
    
    blocks BLOB NOT NULL,
    blockMeta BLOB DEFAULT NULL,

    created DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified DATETIME DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY(chunkId) REFERENCES chunk_v1(id) ON DELETE CASCADE ON UPDATE CASCADE
);
CREATE INDEX chunkslice_chunkid ON chunk_slice_v1(chunkId);



------
--- Chunk block type map
CREATE TABLE type_map_v1 (
    id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT,
    blockId INTEGER UNIQUE NOT NULL,
    blockUuid BLOB NOT NULL,

    created DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX typemap_blockid ON type_map_v1(blockId);
CREATE INDEX typemap_blockuuid ON type_map_v1(blockUuid);
