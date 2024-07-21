# DBMS in C: A Simple B+ Tree Database

## Introduction

This project implements a basic Database Management System (DBMS) in C, utilizing a B+ Tree for indexing and providing functionalities such as insert, delete, update, and select. The system is designed to be lightweight and efficient, an simulate the SQL-Lite database

## Features

- **B+ Tree Indexing**: Efficiently stores and retrieves data using a B+ Tree structure.
- **CRUD Operations**: Supports Create (Insert), Read (Select), Update, and Delete operations.
- **Persistence**: Data is stored in a file, ensuring persistence across sessions.
  
## File Structure

- **module1.h**: Header file containing all the type definitions, constants, and function prototypes.
- **query_processing.c**: Implements functions for opening the database, handling user input, and executing statements.
- **pager.c**: Manages pages in memory, reading from and writing to the database file.
- **cursor.c**: Defines the cursor used to navigate through the table.
- **internal_node.c**: Functions for handling internal nodes of the B+ Tree.
- **leaf_node.c**: Functions for handling leaf nodes of the B+ Tree.
- **btree.c**: Core B+ Tree operations and utility functions.
- **test.c**: Functions for printing and testing the B+ Tree structure.

## Getting Started

### Prerequisites

- GCC (GNU Compiler Collection)

### Building the Project

1. Clone the repository:
    ```sh
    git clone [https://github.com/yourusername/dbms-in-c.git](https://github.com/Tanmay7404/DataBase-With-C.git)
    cd dbms-in-c
    ```

2. Compile the project:
    ```sh
    gcc dbms.c
    ```

3. Run the program and provide any filename:
   ./a.exe helloworld

### Usage

1. Select:
    ```c
    >db select
    ```

2. Select one:
    ```c
    >db select {id}
    ```

3. Insert:
    ```c
    >db insert {id} {name} {email}
    ```

4. Insert:
    ```c
    >db insert {id} {name} {email}
    ```
5. Update:
    ```c
    >db update {old_id} {new_id} {name} {email}
    ```

6. Delete:
    ```c
    >db delete {id}
    ```

7. Exit
   ```c
   >db .exit
  ```
