#include "define.h"

uint32_t* leaf_node_num_cells(void* node) {
  return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void* leaf_node_cell(void* node, uint32_t cell_num) {
  return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
  return leaf_node_cell(node, cell_num);
}

void* leaf_node_value(void* node, uint32_t cell_num) {
  return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void* node) {
  set_node_type(node, NODE_LEAF);
  set_node_root(node, false);
  *leaf_node_num_cells(node) = 0;
  *(node_next(node)) = INVALID_PAGE_NUM;
  *(node_prev(node)) = INVALID_PAGE_NUM;

}
Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  Cursor* cursor = malloc(sizeof(Cursor));
  cursor->table = table;
  cursor->page_num = page_num;
  cursor->end_of_table = false;

  uint32_t min_index = 0;
  uint32_t one_past_max_index = num_cells;
  while (one_past_max_index != min_index) {
    uint32_t index = (min_index + one_past_max_index) / 2;
    uint32_t key_at_index = *leaf_node_key(node, index);
    if (key == key_at_index) {
      cursor->cell_num = index;
      return cursor;
    }
    if (key < key_at_index) {
      one_past_max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  cursor->cell_num = min_index;
  return cursor;
}

void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {

  void* old_node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t old_max = get_node_max_key(cursor->table->pager, old_node);
  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
  void* new_node = get_page(cursor->table->pager, new_page_num);
  initialize_leaf_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);
  *node_next(new_node) = *node_next(old_node);
  *node_next(old_node) = new_page_num;
  *node_prev  (new_node) = cursor->page_num;

  for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
    void* destination_node;
    if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
      destination_node = new_node;
    } else {
      destination_node = old_node;
    }
    uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
    void* destination = leaf_node_cell(destination_node, index_within_node);

    if (i == cursor->cell_num) {
      serialize_row(value,
                    leaf_node_value(destination_node, index_within_node));
      *leaf_node_key(destination_node, index_within_node) = key;
    } else if (i > cursor->cell_num) {
      memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
    } else {
      memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
    }
  }
  *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
  *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;
  if (is_node_root(old_node)) {

    return create_new_root(cursor->table, new_page_num);
  } else {
    uint32_t parent_page_num = *node_parent(old_node);
    uint32_t new_max = get_node_max_key(cursor->table->pager, old_node);
    void* parent = get_page(cursor->table->pager, parent_page_num);

    update_internal_node_key(parent, old_max, new_max);
    internal_node_insert(cursor->table, parent_page_num, new_page_num);
    return;
  }
}

void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {
  void* node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  if (num_cells >= LEAF_NODE_MAX_CELLS) {

    leaf_node_split_and_insert(cursor, key, value);
    return;
  }

  if (cursor->cell_num < num_cells) {

    for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
      memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),LEAF_NODE_CELL_SIZE);
    }
  }

  *(leaf_node_num_cells(node)) += 1;
  *(leaf_node_key(node, cursor->cell_num)) = key;
  serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

void borrow_from_right_leaf(void * node, void* right, void* par){
  
  uint32_t ind = *leaf_node_num_cells(node);
  uint32_t old_max;
  for(uint32_t i= 0; i<*internal_node_num_keys(par);i++){
    if(*internal_node_child(par,i) == *node_prev(right)){
      old_max = *internal_node_key(par,i);
      break;
    }
  }
  *leaf_node_num_cells(node) +=1;
  memcpy(leaf_node_cell(node,ind),leaf_node_cell(right,0),LEAF_NODE_CELL_SIZE);
  uint32_t num_cell_r = *leaf_node_num_cells(right);
  for(uint32_t i=1;i<num_cell_r;i++){
    memcpy(leaf_node_cell(right,i-1),leaf_node_cell(right,i),LEAF_NODE_CELL_SIZE);
  }
  *leaf_node_num_cells(right) -=1;
  uint32_t new_max = *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  update_internal_node_key(par,old_max,new_max);  
}

void borrow_from_left_leaf(void * node, void* left,void* par){
  
  uint32_t ind = *leaf_node_num_cells(left);
  uint32_t old_max;
  for(uint32_t i= 0; i<*internal_node_num_keys(par);i++){
    if(*internal_node_child(par,i) == *node_prev(node)){
      old_max = *internal_node_key(par,i);
      break;
    }
  }
  uint32_t num_cell_r = *leaf_node_num_cells(node);
  *leaf_node_num_cells(node) +=1;
  for(uint32_t i=0;i<num_cell_r;i++){
    memcpy(leaf_node_cell(node,i+1),leaf_node_cell(node,i),LEAF_NODE_CELL_SIZE);
  }
  memcpy(leaf_node_cell(node,0),leaf_node_cell(left,ind-1),LEAF_NODE_CELL_SIZE);
  *leaf_node_num_cells(left) -=1;
  uint32_t new_max = *leaf_node_key(node, *leaf_node_num_cells(left) - 1);
  update_internal_node_key(par,old_max,new_max);  
}

void merge_leaf(void* node, void* left,void* par, Table* table){
  uint32_t num = *leaf_node_num_cells(left);
  uint32_t num2 = *leaf_node_num_cells(node);
  *leaf_node_num_cells(node)+=num;

  uint32_t old_max = get_node_max_key(table->pager,left);
  for(uint32_t i=num2;i>0;i--){
    memcpy(leaf_node_cell(node,i+num-1),leaf_node_cell(node,i-1),LEAF_NODE_CELL_SIZE);
  }

  for(uint32_t i=0;i<num;i++){
    memcpy(leaf_node_cell(node,i),leaf_node_cell(left,i),LEAF_NODE_CELL_SIZE);
  }
  delete_page(table->pager,*node_prev(node));
  if(is_node_root(par)){
    delete_from_root(table,old_max);
  }
  else{
    delete_from_internal(table,*node_parent(node),old_max);
  }
}

void delete_from_leaf(Cursor* cursor){
  void* node = get_page(cursor->table->pager,cursor->page_num);
  uint32_t num_cell = *leaf_node_num_cells(node);
  for(uint32_t i=cursor->cell_num+1;i<num_cell;i++){
    memcpy(leaf_node_cell(node,i-1),leaf_node_cell(node,i),LEAF_NODE_CELL_SIZE);
  }
  *leaf_node_num_cells(node)-=1;
  if(is_node_root(node)){
    return;
  }
  if(*leaf_node_num_cells(node)<LEAF_NODE_MIN_CELLS){
    uint32_t left_pg_num = *node_prev(node);
    uint32_t right_pg_num = *node_next(node);
    uint32_t par_pg_num = *node_parent(node);
    
    void * left = NULL;
    void * right = NULL;
    void * par = NULL;
    if(left_pg_num!=INVALID_PAGE_NUM){
      left = get_page(cursor->table->pager,left_pg_num);
    }
    if(right_pg_num!=INVALID_PAGE_NUM){
      right = get_page(cursor->table->pager,right_pg_num);
    }
    if(par_pg_num!=INVALID_PAGE_NUM){
      par = get_page(cursor->table->pager,par_pg_num);
    }
    if((left!=NULL)&&(*leaf_node_num_cells(left)>LEAF_NODE_MIN_CELLS)&&(*node_parent(left)==*node_parent(node))){
      borrow_from_left_leaf(node,left,par);
    }
    else if((right!=NULL)&&(*leaf_node_num_cells(right)>LEAF_NODE_MIN_CELLS)&&(*node_parent(right)==*node_parent(node))){
      borrow_from_right_leaf(node,right,par);
    }
    else if((left!=NULL)&&(*node_parent(left)==*node_parent(node))){
      merge_leaf(node,left,par,cursor->table);
    }
    else if((right!=NULL)&&(*node_parent(right)==*node_parent(node))){
      merge_leaf(right,node,par,cursor->table);
    }
    else{
      printf("Error Deleting.\n");
      exit(EXIT_FAILURE);
    }
  }
}