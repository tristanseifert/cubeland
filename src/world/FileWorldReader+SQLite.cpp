#include "FileWorldReader.h"

#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>

#include <sqlite3.h>

#include <stdexcept>

using namespace world;

/**
 * Prepares the given query for execution.
 */
void FileWorldReader::prepare(const std::string &query, sqlite3_stmt **outStmt) {
    int err = sqlite3_prepare_v2(this->db, query.c_str(), query.size(), outStmt, nullptr);
    if(err != SQLITE_OK) {
        throw DbError(f("Failed to prepare query '{}' ({}): {}", query, err,
                        sqlite3_errmsg(this->db)));
    }
}

/**
 * Binds the given string to the statement.
 *
 * @note In case of an error, an exception is thrown, _and_ the statement is finalized.
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, const std::string &str) {
    int err = sqlite3_bind_text64(stmt, index, str.c_str(), str.size(), nullptr, SQLITE_UTF8);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind string ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Binds a BLOB value to the statement,
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, const std::vector<unsigned char> &value) {
    int err = sqlite3_bind_blob64(stmt, index, value.data(), value.size(), nullptr);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind blob ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Binds an uuid value to the statement.
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, const uuids::uuid &value) {
    const auto bytes = value.as_bytes();

    int err = sqlite3_bind_blob(stmt, index, bytes.data(), bytes.size(), nullptr);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind uuid ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Binds a 32-bit integer value to the statement.
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, const int32_t value) {
    int err = sqlite3_bind_int(stmt, index, value);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind int32 ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Binds a 64-bit integer value to the statement.
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, const int64_t value) {
    int err = sqlite3_bind_int64(stmt, index, value);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind int64 ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Binds a double (REAL) value to the statement.
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, const double value) {
    int err = sqlite3_bind_double(stmt, index, value);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind double ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Binds a NULL value to the statement at the given index.
 */
void FileWorldReader::bindColumn(sqlite3_stmt *stmt, const size_t index, std::nullptr_t) {
    int err = sqlite3_bind_null(stmt, index);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to bind null ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}

/**
 * Reads a string from the given column in the current result row.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, std::string &out) {
    // get the text value
    const auto ptr = sqlite3_column_text(stmt, col);
    if(!ptr) {
        return false;
    }

    const auto length = sqlite3_column_bytes(stmt, col);
    XASSERT(length > 0, "Invalid TEXT length: {}", length);

    // create string
    out = std::string(ptr, ptr + length);
    return true;
}
/**
 * Extracts the blob value for the given column from the current result row.
 *
 * @note Indices are 0-based.
 *
 * @return Whether the column was non-NULL. A zero-length BLOB will return true, but the output
 * vector is trimmed to be zero bytes.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, std::vector<unsigned char> &out) {
    int err;

    // is the column NULL?
    err = sqlite3_column_type(stmt, col);
    if(err == SQLITE_NULL) {
        out.clear();
        return false;
    }

    // no, get the blob value
    auto valuePtr = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(stmt, col));
    if(valuePtr == nullptr) {
        // zero length blob
        out.clear();
        return true;
    }

    int valueLen = sqlite3_column_bytes(stmt, col);
    XASSERT(valueLen > 0, "Invalid BLOB length: {}", valueLen);

    // assign it into the vector
    out.resize(valueLen);
    out.assign(valuePtr, valuePtr + valueLen);

    return true;
}
/**
 * Attempts to read a 16-byte (exactly) blob from the given column, then creates an UUID from it.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, uuids::uuid &outUuid) {
    int err;

    // check that the blob is 16 bytes
    err = sqlite3_column_bytes(stmt, col);
    if(err != 16) {
        return false;
    }

    // get blob data, then create the uuid
    std::vector<unsigned char> bytes;
    bytes.reserve(16);

    if(!this->getColumn(stmt, col, bytes)) {
        return false;
    }

    outUuid = uuids::uuid(bytes);
    return true;
}
/**
 * Reads the double value of the given column in the current result row.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, double &out) {
    out = sqlite3_column_double(stmt, col);
    return true;
}
/**
 * Reads the 32-bit integer value of the given column in the current result row.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, int32_t &out) {
    out = sqlite3_column_int(stmt, col);
    return true;
}
/**
 * Reads the 64-bit integer value of the given column in the current result row.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, int64_t &out) {
    out = sqlite3_column_int64(stmt, col);
    return true;
}
/**
 * Reads the column value as a boolean; this is to say a 32-bit integer that may only have the
 * values 0 or 1.
 */
bool FileWorldReader::getColumn(sqlite3_stmt *stmt, const size_t col, bool &out) {
    int32_t temp;

    if(this->getColumn(stmt, col, temp)) {
        XASSERT(temp == 0 || temp == 1, "Invalid boolean column value: {}", temp);
        out = (temp == 0) ? false : true;

        return true;
    }
    return false;
}



/**
 * Begins a new transaction.
 */
void FileWorldReader::beginTransaction() {
    PROFILE_SCOPE(TxnBegin);
    int err;

    err = sqlite3_exec(this->db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
        throw DbError(f("failed to start transaction ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Commits the current transaction.
 */
void FileWorldReader::commitTransaction() {
    PROFILE_SCOPE(TxnCommit);
    int err;

    err = sqlite3_exec(this->db, "COMMIT TRANSACTION;", nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
        throw DbError(f("failed to commit transaction ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
/**
 * Rolls the current transaction back.
 */
void FileWorldReader::rollbackTransaction() {
    PROFILE_SCOPE(TxnRollback);
    int err;

    err = sqlite3_exec(this->db, "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
        throw DbError(f("failed to roll back transaction ({}): {}", err, sqlite3_errmsg(this->db)));
    }
}
