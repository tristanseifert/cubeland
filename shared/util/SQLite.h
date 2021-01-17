/**
 * Provides various helpers for working with an SQLite 3 database.
 */
#ifndef UTIL_SQLITE_H
#define UTIL_SQLITE_H

#include "io/Format.h"

#include <string>

#include <uuid.h>
#include <sqlite3.h>

namespace util {
class SQLite {
    public:
        /**
         * Begins a new transaction.
         */
        static void beginTransaction(sqlite3 *db) {
            int err = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
            if(err != SQLITE_OK) {
                throw std::runtime_error(f("SQLite error {}: {}", err, sqlite3_errmsg(db)));
            }
        }
        /**
         * Commits the current transaction.
         */
        static void commitTransaction(sqlite3 *db) {
            int err = sqlite3_exec(db, "COMMIT TRANSACTION;", nullptr, nullptr, nullptr);
            if(err != SQLITE_OK) {
                throw std::runtime_error(f("SQLite error {}: {}", err, sqlite3_errmsg(db)));
            }
        }
        /**
         * Rolls the current transaction back.
         */
        static void rollbackTransaction(sqlite3 *db) {
            int err = sqlite3_exec(db, "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr);
            if(err != SQLITE_OK) {
                throw std::runtime_error(f("SQLite error {}: {}", err, sqlite3_errmsg(db)));
            }
        }



        /**
         * Prepares the given SQL statement for binding and later use.
         */
        static void prepare(sqlite3 *db, const std::string &query, sqlite3_stmt **outStmt) {
            int err = sqlite3_prepare_v2(db, query.c_str(), query.size(), outStmt, nullptr);
            if(err != SQLITE_OK) {
                throw std::runtime_error(f("SQLite error {}: {}", err, sqlite3_errmsg(db)));
            }
        }



        /**
         * Binds the given string to the statement.
         *
         * @note In case of an error, an exception is thrown, _and_ the statement is finalized.
         */
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, const std::string &str) {
            int err = sqlite3_bind_text64(stmt, index, str.c_str(), str.size(), nullptr, SQLITE_UTF8);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }
        /**
         * Binds a BLOB value to the statement,
         */
        template<class T>
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, const std::vector<T> &value) {
            int err = sqlite3_bind_blob64(stmt, index, value.data(), value.size(), nullptr);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }
        /**
         * Binds an uuid value to the statement.
         */
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, const uuids::uuid &value) {
            const auto bytes = value.as_bytes();

            int err = sqlite3_bind_blob(stmt, index, bytes.data(), bytes.size(), nullptr);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }
        /**
         * Binds a 32-bit integer value to the statement.
         */
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, const int32_t value) {
            int err = sqlite3_bind_int(stmt, index, value);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }
        /**
         * Binds a 64-bit integer value to the statement.
         */
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, const int64_t value) {
            int err = sqlite3_bind_int64(stmt, index, value);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }
        /**
         * Binds a double (REAL) value to the statement.
         */
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, const double value) {
            int err = sqlite3_bind_double(stmt, index, value);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }
        /**
         * Binds a NULL value to the statement at the given index.
         */
        static void bindColumn(sqlite3_stmt *stmt, const size_t index, std::nullptr_t) {
            int err = sqlite3_bind_null(stmt, index);
            if(err != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to bind column: {}", err));
            }
        }

        /**
         * Reads a string from the given column in the current result row.
         */
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, std::string &out) {
            // get the text value
            const auto ptr = sqlite3_column_text(stmt, col);
            if(!ptr) {
                return false;
            }

            const auto length = sqlite3_column_bytes(stmt, col);
            if(!length) {
                throw std::runtime_error("Invalid TEXT column length");
            }

            // create string
            out = std::string(ptr, ptr + length);
            return true;
        }
        /**
         * Extracts the blob value for the given column from the current result row.
         *
         * @note Indices are 0-based.
         *
         * @return Whether the column was non-NULL. A zero-length BLOB will return true, but the
         * output vector is trimmed to be zero bytes.
         */
        template<class T>
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, std::vector<T> &out) {
            int err;

            // is the column NULL?
            err = sqlite3_column_type(stmt, col);
            if(err == SQLITE_NULL) {
                out.clear();
                return false;
            }

            // no, get the blob value
            auto valuePtr = reinterpret_cast<const T *>(sqlite3_column_blob(stmt, col));
            if(valuePtr == nullptr) {
                // zero length blob
                out.clear();
                return true;
            }

            int valueLen = sqlite3_column_bytes(stmt, col);
            if(!valueLen) {
                throw std::runtime_error("Invalid BLOB column length");
            }

            // assign it into the vector
            out.resize(valueLen);
            out.assign(valuePtr, valuePtr + valueLen);

            return true;
        }
        /**
         * Attempts to read a 16-byte (exactly) blob from the given column, then creates an UUID from it.
         */
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, uuids::uuid &outUuid) {
            int err;

            // check that the blob is 16 bytes
            err = sqlite3_column_bytes(stmt, col);
            if(err != 16) {
                return false;
            }

            // get blob data, then create the uuid
            std::vector<char> bytes;
            bytes.reserve(16);

            if(!getColumn(stmt, col, bytes)) {
                return false;
            }

            outUuid = uuids::uuid(bytes.begin(), bytes.end());
            return true;
        }
        /**
         * Reads the double value of the given column in the current result row.
         */
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, double &out) {
            out = sqlite3_column_double(stmt, col);
            return true;
        }
        /**
         * Reads the 32-bit integer value of the given column in the current result row.
         */
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, int32_t &out) {
            out = sqlite3_column_int(stmt, col);
            return true;
        }
        /**
         * Reads the 64-bit integer value of the given column in the current result row.
         */
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, int64_t &out) {
            out = sqlite3_column_int64(stmt, col);
            return true;
        }
        /**
         * Reads the column value as a boolean; this is to say a 32-bit integer that may only have 
         * the values 0 or 1.
         */
        static bool getColumn(sqlite3_stmt *stmt, const size_t col, bool &out) {
            int32_t temp;

            if(getColumn(stmt, col, temp)) {
                if(temp != 0 && temp != 1) {
                    throw std::runtime_error(f("Invalid boolean value: {}", temp));
                }

                out = (temp == 0) ? false : true;
                return true;
            }
            return false;
        }



    public:
        /**
         * Checks whether the given table exists.
         */
        static bool tableExists(sqlite3 *db, const std::string &name) {
           int err;
            bool found = false;
            sqlite3_stmt *stmt = nullptr;

            // prepare query and bind the table name
            prepare(db, "SELECT name FROM sqlite_master WHERE type='table' AND name=?", &stmt);
            bindColumn(stmt, 1, name);

            // execute it
            err = sqlite3_step(stmt);
            if(err != SQLITE_ROW && err != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("SQLite error {}: {}", err, sqlite3_errmsg(db)));
            }

            found = (err == SQLITE_ROW);

            // clean up
            sqlite3_finalize(stmt);
            return found; 
        }
};
}

#endif
