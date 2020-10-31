#include "global.h"

/**
 * @brief Construct a new Table:: Table object
 *
 */
Table::Table() {
    logger.log("Table::Table");
}

/**
 * @brief Construct a new Table:: Table object used in the case where the data
 * file is available and LOAD command has been called. This command should be
 * followed by calling the load function;
 *
 * @param tableName 
 */
Table::Table(string tableName) {
    logger.log("Table::Table");
    this->sourceFileName = "../data/" + tableName + ".csv";
    this->tableName = tableName;
}

/**
 * @brief Construct a new Table:: Table object used when an assignment command
 * is encountered. To create the table object both the table name and the
 * columns the table holds should be specified.
 *
 * @param tableName 
 * @param columns 
 */
Table::Table(string tableName, vector<string> columns) {
    logger.log("Table::Table");
    this->sourceFileName = "../data/temp/" + tableName + ".csv";
    this->tableName = tableName;
    this->columns = columns;
    this->columnCount = columns.size();
    this->maxRowsPerBlock = (uint)((BLOCK_SIZE * 1000) / (sizeof(int) * columnCount));
    this->writeRow<string>(columns);
}

/**
 * @brief The load function is used when the LOAD command is encountered. It
 * reads data from the source file, splits it into blocks and updates table
 * statistics.
 *
 * @return true if the table has been successfully loaded 
 * @return false if an error occurred 
 */
bool Table::load() {
    logger.log("Table::load");
    fstream fin(this->sourceFileName, ios::in);
    string line;
    if (getline(fin, line)) {
        fin.close();
        if (this->extractColumnNames(line))
            if (this->blockify())
                return true;
    }
    fin.close();
    return false;
}

/**
 * @brief Function extracts column names from the header line of the .csv data
 * file. 
 *
 * @param line 
 * @return true if column names successfully extracted (i.e. no column name
 * repeats)
 * @return false otherwise
 */
bool Table::extractColumnNames(string firstLine) {
    logger.log("Table::extractColumnNames");
    unordered_set<string> columnNames;
    string word;
    stringstream s(firstLine);
    while (getline(s, word, ',')) {
        word.erase(std::remove_if(word.begin(), word.end(), ::isspace), word.end());
        if (columnNames.count(word))
            return false;
        columnNames.insert(word);
        this->columns.emplace_back(word);
    }
    this->columnCount = this->columns.size();
    this->maxRowsPerBlock = (uint)((BLOCK_SIZE * 1000) / (32 * this->columnCount));
    return true;
}

/**
 * @brief This function splits all the rows and stores them in multiple files of
 * one block size. 
 *
 * @return true if successfully blockified
 * @return false otherwise
 */
bool Table::blockify() {
    logger.log("Table::blockify");
    ifstream fin(this->sourceFileName, ios::in);
    string line, word;
    vector<int> row(this->columnCount, 0);
    vector<vector<int>> rowsInPage(this->maxRowsPerBlock, row);
    int pageCounter = 0;
    this->valuesInColumns = vector<map<int, long long>> (this->columnCount);
    // set<int> dummy;
    // dummy.clear();
    // this->distinctValuesInColumns.assign(this->columnCount, dummy);
    // this->distinctValuesPerColumnCount.assign(this->columnCount, 0);
    getline(fin, line);
    while (getline(fin, line)) {
        stringstream s(line);
        for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++) {
            if (!getline(s, word, ','))
                return false;
            row[columnCounter] = stoi(word);
            rowsInPage[pageCounter][columnCounter] = row[columnCounter];
        }
        pageCounter++;
        this->updateStatistics(row);
        if (pageCounter == this->maxRowsPerBlock) {
            bufferManager.writeTablePage(this->tableName, this->blockCount, rowsInPage, pageCounter);
            this->blockCount++;
            this->rowsPerBlockCount.emplace_back(pageCounter);
            pageCounter = 0;
        }
    }
    if (pageCounter) {
        bufferManager.writeTablePage(this->tableName, this->blockCount, rowsInPage, pageCounter);
        this->blockCount++;
        this->rowsPerBlockCount.emplace_back(pageCounter);
        pageCounter = 0;
    }

    if (this->rowCount == 0)
        return false;
    // this->distinctValuesInColumns.clear();
    return true;
}

/**
 * @brief Given a row of values, this function will update the statistics it
 * stores i.e. it updates the number of rows that are present in the column and
 * the number of distinct values present in each column. These statistics are to
 * be used during optimisation.
 *
 * @param row 
 */
void Table::updateStatistics(vector<int> row) {
    this->rowCount++;
    for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++) {
        this->valuesInColumns[columnCounter][row[columnCounter]]++;
        // if (!this->distinctValuesInColumns[columnCounter].count(row[columnCounter])) {
        //     this->distinctValuesInColumns[columnCounter].insert(row[columnCounter]);
        //     this->distinctValuesPerColumnCount[columnCounter]++;
        // }
    }
}

/**
 * @brief Checks if the given column is present in this table.
 *
 * @param columnName 
 * @return true 
 * @return false 
 */
bool Table::isColumn(string columnName) {
    logger.log("Table::isColumn");
    for (auto col : this->columns) {
        if (col == columnName) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Renames the column indicated by fromColumnName to toColumnName. It is
 * assumed that checks such as the existence of fromColumnName and the non prior
 * existence of toColumnName are done.
 *
 * @param fromColumnName 
 * @param toColumnName 
 */
void Table::renameColumn(string fromColumnName, string toColumnName) {
    logger.log("Table::renameColumn");
    for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++) {
        if (columns[columnCounter] == fromColumnName) {
            columns[columnCounter] = toColumnName;
            break;
        }
    }
    return;
}

/**
 * @brief Returns the name of the indexed column. For external use, member methods are
 * advised to directly access the internal data.
 *
 * @return indexed column name
 */
string Table::getIndexedColumn() {
    if (this->indexingStrategy == NOTHING) {
        return "";
    }
    return this->columns[this->indexedColumn];
}

/**
 * @brief Function prints the first few rows of the table. If the table contains
 * more rows than PRINT_COUNT, exactly PRINT_COUNT rows are printed, else all
 * the rows are printed.
 *
 */
void Table::print() {
    logger.log("Table::print");
    uint count = min((long long)PRINT_COUNT, this->rowCount);

    //print headings
    this->writeRow(this->columns, cout);

    if (this->indexingStrategy == NOTHING) {
        Cursor cursor(this->tableName, 0);
        vector<int> row;
        for (int rowCounter = 0; rowCounter < count; rowCounter++) {
            row = cursor.getNext();
            this->writeRow(row, cout);
        }
    } else if (this->indexingStrategy == HASH) {
        Cursor cursor =this->getCursor(0, 0);
        vector<int> row;
        for (int i = 0; i < count; i++) {
            row = cursor.getNextInAllBuckets();
            this->writeRow(row, cout);
        }
        // TODO: rewrite previous block like this
    }

    printRowCount(this->rowCount);
}

/**
 * @brief This function checks if cursor pageIndex is less than the table pageIndex, 
 * and calls the nextPage function of the cursor with incremented pageIndex.
 *
 * @param cursor 
 * @return vector<int> 
 */
void Table::getNextPage(Cursor *cursor) {
    logger.log("Table::getNextPage");

    if (cursor->pageIndex < this->blockCount - 1) {
        cursor->nextPage(cursor->pageIndex + 1);
    }
}

/**
 * @brief This function checks if cursor chainCount is less than the bucket chainCount, 
 * and calls the nextPage function of the cursor with incremented chainCount.
 *
 * @param cursor 
 * @return void 
 */
void Table::getNextPage(Cursor *cursor, int chainCount) {
    logger.log("Table::getNextPage");
    if (cursor->chainCount < this->blocksInBuckets[cursor->bucket].size() - 1) {
        cursor->nextPage(cursor->bucket, cursor->chainCount + 1);
    }
}

/**
 * @brief This function checks if cursor chainCount is less than the bucket chainCount, 
 * and calls the nextPage function of the cursor with incremented chainCount. If the
 * bucket has been fully read, it calls the nextPage function of the cursor with
 * incremented bucketCount.
 *
 * @param cursor 
 * @return void 
 */
void Table::getNextPage(Cursor *cursor, int bucket, int chainCount) {
    logger.log("Table::getNextPage");

    if (cursor->chainCount < this->blocksInBuckets[cursor->bucket].size() - 1) {
        cursor->nextPage(cursor->bucket, cursor->chainCount + 1);
    }
    else if (cursor->bucket < M - 1) {
        cursor->nextPage(cursor->bucket + 1, 0);
    }
}


/**
 * @brief called when EXPORT command is invoked to move source file to "data"
 * folder.
 *
 */
void Table::makePermanent() {
    logger.log("Table::makePermanent");
    if (!this->isPermanent())
        bufferManager.deleteFile(this->sourceFileName);
    string newSourceFile = "../data/" + this->tableName + ".csv";
    ofstream fout(newSourceFile, ios::out);

    //print headings
    this->writeRow(this->columns, fout);

    Cursor cursor(this->tableName, 0);
    vector<int> row;
    for (int rowCounter = 0; rowCounter < this->rowCount; rowCounter++) {
        row = cursor.getNext();
        this->writeRow(row, fout);
    }
    fout.close();
}

/**
 * @brief Function to check if table is already exported
 *
 * @return true if exported
 * @return false otherwise
 */
bool Table::isPermanent() {
    logger.log("Table::isPermanent");
    if (this->sourceFileName == "../data/" + this->tableName + ".csv")
        return true;
    return false;
}

/**
 * @brief The unload function removes the table from the database by deleting
 * all temporary files created as part of this table
 *
 */
void Table::unload() {
    logger.log("Table::~unload");
    if (this->indexingStrategy == NOTHING) {
        for (int pageCounter = 0; pageCounter < this->blockCount; pageCounter++)
            bufferManager.deleteTableFile(this->tableName, pageCounter);
        if (!isPermanent())
            bufferManager.deleteFile(this->sourceFileName);
    } else if (this->indexingStrategy == HASH) {
        for (int i = 0; i < this->blocksInBuckets.size(); i++) { // not necessarily this->M
            for (int j = 0; j < this->blocksInBuckets[i].size(); j++) {
                bufferManager.deleteHashFile(this->tableName, i, j);
            }
        }
    }
}

/**
 * @brief Function that returns a cursor that reads rows from this table
 * 
 * @return Cursor 
 */
Cursor Table::getCursor() {
    logger.log("Table::getCursor");
    Cursor cursor(this->tableName, 0);
    return cursor;
}

/**
 * @brief Function that returns a cursor that reads rows from the blocks of a bucket 
 * @param bucket
 * @param chainCount 
 * @return Cursor 
 */
Cursor Table::getCursor(int bucket, int chainCount) {
    logger.log("Table::getCursor");
    Cursor cursor(this->tableName, bucket, chainCount);
    return cursor;
}

/**
 * @brief Function that returns the index of column indicated by columnName
 * 
 * @param columnName 
 * @return int 
 */
int Table::getColumnIndex(string columnName) {
    logger.log("Table::getColumnIndex");
    for (int columnCounter = 0; columnCounter < this->columnCount; columnCounter++) {
        if (this->columns[columnCounter] == columnName)
            return columnCounter;
    }
}

// LINEAR HASHING

/**
 * @brief The hash function for linear hashing. Takes care of this table's M and N.
 * 
 * @param key
 * 
 * @return int
 */
int Table::hash(int key) {
    logger.log("Table::hash");

    int first = MOD(key, this->M);
    return (first < this->N ? MOD(key, 2 * M) : first);
}


/**
 * @brief Function that creates a linear hash index on the table
 * 
 * @param columnName
 * @param bucketCount
 * 
 * @return void
 */
void Table::linearHash(const string& columnName, int bucketCount) {
    logger.log("Table::linearHash");
    
    if (this->rowCount == 0) {
        return;
    }
    // cout << "DEBUG" << this->rowCount; // TODO: Check for correctness

    // set metadata
    this->indexed = true;
    this->indexedColumn = this->getColumnIndex(columnName);
    this->indexingStrategy = HASH;

    this->M = bucketCount;
    this->initialBucketCount = bucketCount;
    this->blocksInBuckets = vector<vector<int>> (this->M);

    int col = this->indexedColumn;
    Cursor cursor (this->tableName, 0);

    vector<int> row;
    for (int i = 0; i < this->rowCount; i++) {
        row = cursor.getNext();
        int bucket = this->hash(row[col]);
        this->insertIntoHashBucket(row, bucket);
        // This would return if overflow or not,
        // but we don't need to split right now.
    }

    // Delete existing pages
    for (int i = 0; i < this->blockCount; i++) {
        bufferManager.deleteTableFile(this->tableName, i);
    }

    // Update block count
    this->blockCount = 0;
    for (auto &b : this->blocksInBuckets) {
        this->blockCount += b.size();
    }
    this->rowsPerBlockCount.clear(); // this doesn't hold any meaning anymore
}

/**
 * @brief Inserts a row into the last block of the overflow chain of a bucket. Allocates a new block if needed.
 * 
 * @param row
 * @param bucket
 * 
 * @return bool indicating an overflow
 */

bool Table::insertIntoHashBucket(const vector<int>& row, int bucket) {
    if (bucket >= this->blocksInBuckets.size()) {
        return false;  // TODO: raise an error somehow
    }

    bool found = false;
    
    vector<vector<int>> rows (0);
    int chainCount;

    for (chainCount = 0; chainCount < this->blocksInBuckets[bucket].size(); chainCount++) {
        if (blocksInBuckets[bucket][chainCount] < this->maxRowsPerBlock) {
            rows = bufferManager.getHashPage(this->tableName, bucket, chainCount).data;
            this->blocksInBuckets[bucket][chainCount]++;

            found = true;
            break;
        }
    }

    if (!found) {
        this->blocksInBuckets[bucket].push_back(1);
        chainCount = blocksInBuckets[bucket].size() - 1;
    }

    rows.push_back(row);
    bufferManager.writeHashPage(this->tableName, bucket, chainCount, rows);

    return !found;
}

// bool Table::insertIntoHashBucket(const vector<int>& row, int bucket) {
//     if (bucket >= this->blocksInBuckets.size()) {
//         return false; // TODO: raise an error somehow
//}

//     bool newPage = false;
//     vector<vector<int>> rows (0);

//     if (!this->blocksInBuckets[bucket].empty() && this->blocksInBuckets[bucket].back() < this->maxRowsPerBlock) {
//         // Fit in the last block
        
//         rows = bufferManager.getHashPage(this->tableName, bucket, this->blocksInBuckets[bucket].size() - 1).data;
//         // TODO: Exception handling, page not found

//         this->blocksInBuckets[bucket].back() = this->blocksInBuckets[bucket].back() + 1;

//     } else {
//         // allocate a new block

//         this->blocksInBuckets[bucket].push_back(1);
//         newPage = true;
//     }

//     rows.push_back(row);
//     bufferManager.writeHashPage(this->tableName, bucket, this->blocksInBuckets[bucket].size() - 1, rows);
//     return newPage;
// }

/**
 * @brief Inserts a row into the table
 * 
 * @param row
 * @param
 * 
 * @return void
 */
bool Table::insert(const vector<int>& row) {
    if (row.size() != this->columnCount) {
        return false;
    }

    if (this->indexingStrategy == NOTHING) {
        ; // TODO: insert into a normal table
    } else if (this->indexingStrategy == HASH) {
        int bucket = this->hash(row[this->indexedColumn]);
        // because row is large enough, this is always in bounds
        bool overflow = this->insertIntoHashBucket(row, bucket);

        if (overflow) {
            // split
            this->linearHashSplit();
        }

    } else if (this->indexingStrategy == BTREE) {
        ;
    }

    // Update metadata
    this->updateStatistics(row);

    return true;
}

void Table::linearHashSplit() {
    this->blocksInBuckets.push_back(vector<int> (0)); // M + N
    this->blocksInBuckets.push_back(vector<int>(0)); // M + N + 1 (temporary)

    // rehash
    Cursor cursor = this->getCursor(this->N, 0);
    auto row = cursor.getNextInBucket();
    while (!row.empty()) {
        int hashkey = row[this->indexedColumn];
        int hashval = MOD(hashkey, 2 * this->M);

        if (hashval == this->N) {
            this->insertIntoHashBucket(row, this->M + this->N + 1);
        } else {
            this->insertIntoHashBucket(row, this->M + this->N);
        }

        row = cursor.getNextInBucket();
    }

    // delete original buckets
    for (int i = 0; i < this->blocksInBuckets[this->N].size(); i++) {
        bufferManager.deleteHashFile(this->tableName, this->N, i);
    }

    // rename buckets
    for (int i = 0; i < this->blocksInBuckets.back().size(); i++) {
        auto data = bufferManager.getHashPage(this->tableName, this->M + this->N + 1, i).data;
        bufferManager.deleteHashFile(this->tableName, this->M + this->N + 1, i);

        bufferManager.writeHashPage(this->tableName, this->N, i, data);
    }

    // fix metadata
    this->blocksInBuckets[this->N] = this->blocksInBuckets.back();
    this->blocksInBuckets.pop_back();
    
    this->blockCount = 0;
    for (auto &b : this->blocksInBuckets) {
        this->blockCount += b.size();
    }

    this->N++;
    if (this->N == this->M) {
        this->M++;
        this->N = 0;
    }

}

/**
 * @brief Deletes a row, if exists, from the table
 * 
 * @param row
 * 
 * @return bool true if the row was deleted
 */
bool Table::remove(const vector<int>& row) {
    // invalid row
    if (row.size() != this->columnCount) {
        return false;
    }

    // heuristic for row not existing
    for (int i = 0; i < this->columnCount; i++) {
        if (this->valuesInColumns[i]. find(row[i]) == this->valuesInColumns[i].end() || this->valuesInColumns[i][row[i]] <= 0) {
            return false;
        }
    }

    bool foundAtleastOnce = false;
    
    if (this->indexingStrategy == NOTHING) {
        ;
        // TODO
    } else if (this->indexingStrategy == HASH) {

        // check corresponding bucket
        // does not use cursor because have to modify page if row was actually found
        int bucket = this->hash(row[this->indexedColumn]);
        
        for (int i = 0; i < this->blocksInBuckets[bucket].size(); i++) {
            
            bool foundInPage = false;
            auto data = bufferManager.getHashPage(this->tableName, bucket, i).data;
            
            auto it = data.begin();
            while (it != data.end()) {
                if (*it == row) {
                    it = data.erase(it);
                    foundInPage = true;
                } else {
                    it++;
                }
            }

            if (foundInPage) {
                bufferManager.writeHashPage(this->tableName, bucket, i, data);
            }

            foundAtleastOnce |= foundInPage;
        }
    }

    // TODO: combine on underflow


    // TODO: Update statistics
    for (int i = 0; i < this->columnCount; i++) {
        this->valuesInColumns[i][row[i]]--;
        if (this->valuesInColumns[i][row[i]] == 0) {
            this->valuesInColumns[i].erase(row[i]);
        }
    }
    this->rowCount--;

    return foundAtleastOnce;
}

void Table::linearHashCombine() {
    
    // can't combine further, M could be odd
    if (this->M == this->initialBucketCount) {
        return;
    }

    // handle the special case of undoing doubling of M
    if (this->N == 0) {
        this->M /= 2;
        this->N = this->M;
    }

    // copy rows back
    int to = this->N - 1;
    int from = to + this->M;

    Cursor cursor = this->getCursor(from, 0);
    auto row = cursor.getNextInBucket();
    while (!row.empty()) {
        this->insertIntoHashBucket(row, to);
    }

    // delete the bucket
    for (int i = 0; i < this->blocksInBuckets[from].size(); i++) {
        bufferManager.deleteHashFile(this->tableName, from, i);
    }

    // fix metadata
    this->N--;
    this->blocksInBuckets.pop_back(); // has to be the last entry

    this->blockCount = 0;
    for (auto &b : this->blocksInBuckets) {
        this->blockCount += b.size();
    }
}