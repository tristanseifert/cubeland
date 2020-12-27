#!/usr/bin/env python3
"""
Given a directory, archive all contents of it (including subdirectories) into a resource bundle for
the game to use.

Resource bundles are simply sqlite3 databases with a very simple schema.
"""
import sys
import os
from pathlib import Path
import glob
import sqlite3
from time import strftime

##### validate arguments and open database
if len(sys.argv) != 3:
    raise ValueError(f'usage: {sys.argv[0]} [path to resource directory] [resource bundle name]')

conn = sqlite3.connect(sys.argv[2])
cursor = conn.cursor()

##### create tables and drop existing indices
cursor.execute('CREATE TABLE IF NOT EXISTS resources (name TEXT NOT NULL UNIQUE PRIMARY KEY, content BLOB)')
cursor.execute('DROP INDEX IF EXISTS resources_name')

cursor.execute('CREATE TABLE IF NOT EXISTS metadata (name TEXT NOT NULL UNIQUE PRIMARY KEY, value TEXT)')

##### iterate over all files
root = Path(sys.argv[1])
for file in glob.iglob(sys.argv[1] + '/**', recursive=True):
    if os.path.isdir(file): # skip directories
        continue

    with open(file, 'rb') as input:
        ablob = input.read()
        relPath = Path(file).relative_to(root)
        print(relPath)

        cursor.execute('INSERT INTO resources (name, content) VALUES (?, ?) ON CONFLICT (name) DO UPDATE SET content=excluded.content', (str(relPath), sqlite3.Binary(ablob)))

##### clean up
dateStr = strftime("%Y-%m-%d %H:%M:%S")
cursor.execute('INSERT INTO metadata (name, value) VALUES (\'created_at\', ?) ON CONFLICT (name) DO UPDATE SET value=excluded.value', (dateStr, ))

cursor.execute('CREATE INDEX resources_name ON resources (name)')

conn.commit()
cursor.execute('VACUUM')
