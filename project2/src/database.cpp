#include <database.hpp>

/* *************************
 * Record Class Definition *
 * *************************/
Record::Record(record_index_t index) {
    this->index = index;
    this->value = DEFAULT_RECORD_VALUE;
}

Record::Record(record_index_t index, record_value_t value) {
    this->index = index;
    this->value = value;
}

Record Record::clone() {
    return Record(index, value);
}

/* ***************************
 * Database Class Definition *
 * ***************************/

Database::Database(size_t numRecords) {
    this->numRecords = numRecords;

    for (size_t i = 1; i <= numRecords; i++) {
        this->records.push_back(Record(i));
    }
}

Database::~Database() {
}

Record Database::get(record_index_t index) {
    if (index > numRecords || index <= 0) throw -1;
    return this->records[index - 1].clone();
}

void Database::set(record_index_t index, record_value_t value) {
    if (index > numRecords || index <= 0) throw -1;
    this->records[index - 1].value = value;
}
