#ifndef __TABLE_H
#define __TABLE_H

#include "cursor.h"

enum IndexingStrategy {
    BTREE,
    HASH,
    NOTHING
};

/**
 * @brief The Table class holds all information related to a loaded table. It
 * also implements methods that interact with the parsers, executors, cursors
 * and the buffer manager. There are typically 2 ways a table object gets
 * created through the course of the workflow - the first is by using the LOAD
 * command and the second is to use assignment statements (SELECT, PROJECT,
 * JOIN, SORT, CROSS and DISTINCT). 
 *
 */
class Table {

   public:
    //TODO: Decide if unordered or ordered. If unordered, rewrite selection loops, change line 104 of table.cpp
    vector<set<int>> distinctValuesInColumns;
    string sourceFileName = "";
    string tableName = "";
    vector<string> columns;
    vector<uint> distinctValuesPerColumnCount;
    uint columnCount = 0;
    long long int rowCount = 0;
    uint blockCount = 0;
    uint maxRowsPerBlock = 0;
    vector<uint> rowsPerBlockCount;
    bool indexed = false;
    int indexedColumn = -1;
    IndexingStrategy indexingStrategy = NOTHING;

    bool extractColumnNames(string firstLine);
    bool blockify();
    void updateStatistics(vector<int> row);
    Table();
    Table(string tableName);
    Table(string tableName, vector<string> columns);
    bool load();
    bool isColumn(string columnName);
    void renameColumn(string fromColumnName, string toColumnName);
    void print();
    void makePermanent();
    bool isPermanent();
    void getNextPage(Cursor *cursor);
    void getNextPage(Cursor *cursor, int chainCount);
    void getNextPage(Cursor *cursor, int bucket, int chainCount);

    Cursor getCursor();
    Cursor getCursor(int bucket, int chainCount);
    int getColumnIndex(string columnName);
    string getIndexedColumn();
    void unload();
    bool insert(const vector<int>& row);

    // FOR LINEAR HASHING

    int M = 0; // different from blockCount, as blockCount includes overflow blocks as well
    int N = 0; // all blocks from 0 to N-1 have been split
    // TODO: Make these private

    // We don't use rowsPerBlockCount, because the blocks now have overflow chains and cannot be numbered sequentially. Instead use blocksInBuckets.
    vector<vector<int>> blocksInBuckets;
    
    int hash(int key);
    void linearHash(const string &columnName, int bucketCount);
    bool insertIntoHashBucket(const vector<int>& row, int bucket);
    void linearHashSplit();


    /**
 * @brief Static function that takes a vector of valued and prints them out in a
 * comma seperated format.
 *
 * @tparam T current usaages include int and string
 * @param row 
 */
    template <typename T>
    void writeRow(vector<T> row, ostream &fout) {
        logger.log("Table::printRow");
        for (int columnCounter = 0; columnCounter < row.size(); columnCounter++) {
            if (columnCounter != 0)
                fout << ", ";
            fout << row[columnCounter];
        }
        fout << endl;
    }

    /**
 * @brief Static function that takes a vector of valued and prints them out in a
 * comma seperated format.
 *
 * @tparam T current usaages include int and string
 * @param row 
 */
    template <typename T>
    void writeRow(vector<T> row) {
        logger.log("Table::printRow");
        ofstream fout(this->sourceFileName, ios::app);
        this->writeRow(row, fout);
        fout.close();
    }
};

#endif