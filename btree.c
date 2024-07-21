#include "define.h"

NodeType get_node_type(void* node) {
  uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
  return (NodeType)value;
}

void set_node_type(void* node, NodeType type) {
  uint8_t value = type;
  *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;
}

bool is_node_root(void* node) {
  uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
  return (bool)value;
}

void set_node_root(void* node, bool is_root) {
  uint8_t value = is_root;
  *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
}

uint32_t* node_next(void* node) {
  return node + NODE_NEXT_OFFSET;
}
 
uint32_t* node_prev(void* node) {
  return node + NODE_PREV_OFFSET;
}

uint32_t* node_parent(void* node) { return node + PARENT_POINTER_OFFSET; }


uint32_t get_node_max_key(Pager* pager, void* node) {
  if (get_node_type(node) == NODE_LEAF) {
    return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
  void* right_child = get_page(pager,*internal_node_right_child(node));
  return get_node_max_key(pager, right_child);
}

Cursor* table_find(Table* table, uint32_t key) {
  uint32_t root_page_num = table->root_page_num;
  void* root_node = get_page(table->pager, root_page_num);

  if (get_node_type(root_node) == NODE_LEAF) {
    return leaf_node_find(table, root_page_num, key);
  } else {
    return internal_node_find(table, root_page_num, key);
  }
}

void create_new_root(Table* table, uint32_t right_child_page_num) {

  void* root = get_page(table->pager, table->root_page_num);
  void* right_child = get_page(table->pager, right_child_page_num);
  uint32_t left_child_page_num = get_unused_page_num(table->pager);
  void* left_child = get_page(table->pager, left_child_page_num);

  if (get_node_type(root) == NODE_INTERNAL) {
    initialize_internal_node(right_child);
    initialize_internal_node(left_child);
  }
  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  if (get_node_type(left_child) == NODE_INTERNAL) {
    void* child;
    for (int i = 0; i < *internal_node_num_keys(left_child); i++) {
      child = get_page(table->pager, *internal_node_child(left_child,i));
      *node_parent(child) = left_child_page_num;
    }
    child = get_page(table->pager, *internal_node_right_child(left_child));
    *node_parent(child) = left_child_page_num;
  }

  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = left_child_page_num;
  uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);
  *internal_node_key(root, 0) = left_child_max_key;
  *internal_node_right_child(root) = right_child_page_num;
  *node_parent(left_child) = table->root_page_num;
  *node_parent(right_child) = table->root_page_num;
  *node_next(left_child) = right_child_page_num;
  *node_prev(right_child) = left_child_page_num;
}

void delete_from_root(Table* table, uint32_t key){
  void * root = get_page(table->pager, table->root_page_num);
  uint32_t num = *internal_node_num_keys(root);
  if(num==1){
    uint32_t new_root_no = *internal_node_right_child(root);
    void* new_root = get_page(table->pager,new_root_no);

    delete_page(table->pager,table->root_page_num);   
    table->root_page_num = new_root_no;
    *node_parent(new_root) = INVALID_PAGE_NUM;
    *table_root(table->pager) = new_root_no;
    set_node_root(new_root,true);
    *node_prev(new_root) = INVALID_PAGE_NUM;
    *node_next(new_root) = INVALID_PAGE_NUM;

    
    return;
  }
  uint32_t idx = internal_node_find_child(root,key);
  for(uint32_t i=idx+1;i<num;i++){
    memcpy(internal_node_cell(root,i-1),internal_node_cell(root,i),INTERNAL_NODE_CELL_SIZE);
  }
  *internal_node_num_keys(root)-=1;
}