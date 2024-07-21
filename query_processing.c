#include "define.h"


Table* db_open(const char* filename) {
  Pager* pager = pager_open(filename);

  Table* table = malloc(sizeof(Table));
  table->pager = pager;
  table->root_page_num = *(table_root(pager));

  if (pager->file_length == 0) {

    void* root_node = get_page(pager, 1);
    initialize_leaf_node(root_node);
    set_node_root(root_node, true);
  }

  return table;
}

InputBuffer* new_input_buffer() {
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;

  return input_buffer;
}

void print_prompt() { printf("db > "); }

void read_input(InputBuffer* input_buffer) {
  ssize_t bytes_read =
      getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }

  input_buffer->input_length = bytes_read - 1;
  input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer* input_buffer) {
  free(input_buffer->buffer);
  free(input_buffer);
}



void db_close(Table* table) {
  Pager* pager = table->pager;

  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    if (pager->pages[i] == NULL) {
      continue;
    }
    pager_flush(pager, i);
    free(pager->pages[i]);
    pager->pages[i] = NULL;
  }

  int result = close(pager->file_descriptor);
  if (result == -1) {
    printf("Error closing db file.\n");
    exit(EXIT_FAILURE);
  }
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    void* page = pager->pages[i];
    if (page) {
      free(page);
      pager->pages[i] = NULL;
    }
  }
  free(pager);
  free(table);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    close_input_buffer(input_buffer);
    db_close(table);
    exit(EXIT_SUCCESS);
  } else if (strcmp(input_buffer->buffer, ".btree") == 0) {
    printf("Tree:\n");
    print_tree(table->pager, table->root_page_num, 0);
    return META_COMMAND_SUCCESS;
  } else if (strcmp(input_buffer->buffer, ".constants") == 0) {
    printf("Constants:\n");
    print_constants();
    return META_COMMAND_SUCCESS;
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement) {
  statement->type = STATEMENT_INSERT;

  char* keyword = strtok(input_buffer->buffer, " ");
  char* id_string = strtok(NULL, " ");
  char* username = strtok(NULL, " ");
  char* email = strtok(NULL, " ");

  if (id_string == NULL || username == NULL || email == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  int id = atoi(id_string);
  if (id < 0) {
    return PREPARE_NEGATIVE_ID;
  }
  if (strlen(username) > COLUMN_USERNAME_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }
  if (strlen(email) > COLUMN_EMAIL_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }

  statement->row.id = id;
  strcpy(statement->row.username, username);
  strcpy(statement->row.email, email);

  return PREPARE_SUCCESS;
}

PrepareResult prepare_delete(InputBuffer* input_buffer, Statement* statement) {
  statement->type = STATEMENT_DELETE;

  char* keyword = strtok(input_buffer->buffer, " ");
  char* id_string = strtok(NULL, " ");

  if (id_string == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  int id = atoi(id_string);
  if (id < 0) {
    return PREPARE_NEGATIVE_ID;
  }
  

  statement->row.id = id;
  strcpy(statement->row.username,"");
  strcpy(statement->row.email,"");

  return PREPARE_SUCCESS;
}

PrepareResult prepare_select(InputBuffer* input_buffer, Statement* statement) {
  statement->type = STATEMENT_SELECT_ONE;

  char* keyword = strtok(input_buffer->buffer, " ");
  char* id_string = strtok(NULL, " ");

  if (id_string == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  int id = atoi(id_string);
  if (id < 0) {
    return PREPARE_NEGATIVE_ID;
  }

  statement->row.id = id;
  strcpy(statement->row.username,"");
  strcpy(statement->row.email,"");
  return PREPARE_SUCCESS;
}

PrepareResult prepare_update(InputBuffer* input_buffer, Statement* statement) {
  statement->type = STATEMENT_UPDATE;
  
  char* keyword = strtok(input_buffer->buffer, " ");
  char* id_string_old = strtok(NULL, " ");
  char* id_string_new = strtok(NULL, " ");
  char* username = strtok(NULL, " ");
  char* email = strtok(NULL, " ");

  if (id_string_old == NULL || id_string_old == NULL || username == NULL || email == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  int id_old = atoi(id_string_old);
  if (id_old < 0) {
    return PREPARE_NEGATIVE_ID;
  }
  int id_new = atoi(id_string_new);
  if (id_new < 0) {
    return PREPARE_NEGATIVE_ID;
  }
  if (strlen(username) > COLUMN_USERNAME_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }
  if (strlen(email) > COLUMN_EMAIL_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }

  statement->row.id = id_new;
  strcpy(statement->row.username, username);
  strcpy(statement->row.email, email);
  statement->old_id = id_old;

  return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer* input_buffer,
                                Statement* statement) {
  if (strncmp(input_buffer->buffer,"insert", 6) == 0) {
    return prepare_insert(input_buffer, statement);
  }
  if (strcmp(input_buffer->buffer, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }
  else if (strncmp(input_buffer->buffer,"select", 6) == 0){
    return prepare_select(input_buffer,statement);
  }
  if (strncmp(input_buffer->buffer,"delete", 6) == 0){
    return prepare_delete(input_buffer, statement);
  }

  if (strncmp(input_buffer->buffer,"update", 6) == 0){
    return prepare_update(input_buffer, statement);
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}


ExecuteResult execute_insert(Statement* statement, Table* table) {
  Row* row = &(statement->row);
  uint32_t key_to_insert = row->id;
  Cursor* cursor = table_find(table, key_to_insert);
  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  if (cursor->cell_num < num_cells) {
    uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
    if (key_at_index == key_to_insert) {
      free(cursor);
      return EXECUTE_DUPLICATE_KEY;
    }
  }
  printf("Cursor pg_no: %d cell_no: %d", cursor->page_num,cursor->cell_num);
  leaf_node_insert(cursor, row->id, row);

  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
  Cursor* cursor = table_start(table);
  Row row;
  while (!(cursor->end_of_table)) {
    deserialize_row(cursor_value(cursor), &row);
    print_row(&row);
    cursor_advance(cursor);
  }

  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_delete(Statement* statement, Table* table) {
  Row* row = &(statement->row);
  uint32_t key_to_delete = row->id;
  Cursor* cursor = table_find(table, key_to_delete);

  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  if (cursor->cell_num < num_cells) {
    uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
    if (key_at_index == key_to_delete) {
      delete_from_leaf(cursor);
      free(cursor);
      return EXECUTE_SUCCESS;
    }
  }
  free(cursor);
  return EXECUTE_KEY_NOT_FOUND;
}

ExecuteResult execute_select_one(Statement* statement, Table* table) {
  Cursor* cursor = table_find(table, statement->row.id);
  Row row;
  
  deserialize_row(cursor_value(cursor), &row);
  if(row.id!=statement->row.id){
    return EXECUTE_KEY_NOT_FOUND;
  }
  print_row(&row);
  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(Statement* statement, Table* table) {
  Statement s1;
  s1.row.id = statement->old_id;
  strcpy(s1.row.username, "");
  strcpy(s1.row.email, "");
  s1.type = STATEMENT_DELETE;
  int q = execute_delete(&s1,table);
  if(q!=EXECUTE_SUCCESS){
    return q;
  }
  Statement s2;
  s2.type = STATEMENT_INSERT;
  s2.row = statement->row;
  return execute_insert(&s2,table);

}

ExecuteResult execute_statement(Statement* statement, Table* table) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      return execute_insert(statement, table);
    case (STATEMENT_SELECT):
      return execute_select(statement, table);
    case (STATEMENT_DELETE):
      return execute_delete(statement,table);
    case (STATEMENT_SELECT_ONE):
      return execute_select_one(statement,table);
    case (STATEMENT_UPDATE):
      return execute_update(statement,table);
  }
}