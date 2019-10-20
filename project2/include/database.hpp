#ifndef _DATABASE_HPP
#define _DATABASE_HPP

#include <sys/types.h>
#include <stdint.h>
#include <vector>

#define DEFAULT_RECORD_VALUE (100)

class Record;
class Database;

typedef int32_t record_index_t;
typedef int64_t record_value_t;
typedef std::vector<Record> RecordCollection;

class Record {
public:
    record_index_t index;
    record_value_t value;

    Record(record_index_t index);
    Record(record_index_t index, record_value_t value);
    Record clone();
};

class Database {
private:
    size_t numRecords;
    RecordCollection records;
public:
    Database(size_t numRecords);
    ~Database();

    size_t size();
    Record get(record_index_t index);
    void set(record_index_t index, record_value_t value);
};

#endif
