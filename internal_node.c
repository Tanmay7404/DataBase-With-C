#include "define.h"



uint32_t* internal_node_num_keys(void* node) {
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t* internal_node_right_child(void* node) {
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

uint32_t* internal_node_cell(void* node, uint32_t cell_num) {
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t* internal_node_child(void* node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys) {
    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  } else if (child_num == num_keys) {
    uint32_t* right_child = internal_node_right_child(node);
    if (*right_child == INVALID_PAGE_NUM) {
      printf("Tried to access right child of node, but was invalid page\n");
      exit(EXIT_FAILURE);
    }
    return right_child;
  } else {
    uint32_t* child = internal_node_cell(node, child_num);
    if (*child == INVALID_PAGE_NUM) {
      printf("Tried to access child %d of node, but was invalid page\n", child_num);
      exit(EXIT_FAILURE);
    }
    return child;
  }
}

uint32_t* internal_node_key(void* node, uint32_t key_num) {
  return (void*)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

void initialize_internal_node(void* node) {
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
  *(node_next(node)) = INVALID_PAGE_NUM;
  *(node_prev(node)) = INVALID_PAGE_NUM;


  *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

uint32_t internal_node_find_child(void* node, uint32_t key) {
 

  uint32_t num_keys = *internal_node_num_keys(node);

  uint32_t min_index = 0;
  uint32_t max_index = num_keys; 

  while (min_index != max_index) {
    uint32_t index = (min_index + max_index) / 2;
    uint32_t key_to_right = *internal_node_key(node, index);
    if (key_to_right >= key) {
      max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  return min_index;
}

Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);

  uint32_t child_index = internal_node_find_child(node, key);
  uint32_t child_num = *internal_node_child(node, child_index);
  void* child = get_page(table->pager, child_num);
  switch (get_node_type(child)) {
    case NODE_LEAF:
      return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
      return internal_node_find(table, child_num, key);
  }
}


void internal_node_insert(Table* table, uint32_t parent_page_num,
                          uint32_t child_page_num) {
  

  void* parent = get_page(table->pager, parent_page_num);
  void* child = get_page(table->pager, child_page_num);
  uint32_t child_max_key = get_node_max_key(table->pager, child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);

  if (original_num_keys >= INTERNAL_NODE_MAX_KEYS) {
    internal_node_split_and_insert(table, parent_page_num, child_page_num);
    return;
  }

  uint32_t right_child_page_num = *internal_node_right_child(parent);
  
  if (right_child_page_num == INVALID_PAGE_NUM) {
    *internal_node_right_child(parent) = child_page_num;
    return;
  }

  void* right_child = get_page(table->pager, right_child_page_num);
 
  *internal_node_num_keys(parent) = original_num_keys + 1;

  if (child_max_key > get_node_max_key(table->pager, right_child)) {
   
    *internal_node_child(parent, original_num_keys) = right_child_page_num;
    *internal_node_key(parent, original_num_keys) =
        get_node_max_key(table->pager, right_child);
    *internal_node_right_child(parent) = child_page_num;
  } else {
    
    for (uint32_t i = original_num_keys; i > index; i--) {
      void* destination = internal_node_cell(parent, i);
      void* source = internal_node_cell(parent, i - 1);
      memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_child(parent, index) = child_page_num;
    *internal_node_key(parent, index) = child_max_key;
  }
}

void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key) {
  uint32_t old_child_index = internal_node_find_child(node, old_key);
  *internal_node_key(node, old_child_index) = new_key;
}

void internal_node_split_and_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num) {
  uint32_t old_page_num = parent_page_num;
  void* old_node = get_page(table->pager,parent_page_num);
  uint32_t old_max = get_node_max_key(table->pager, old_node);

  void* child = get_page(table->pager, child_page_num); 
  uint32_t child_max = get_node_max_key(table->pager, child);

  uint32_t new_page_num = get_unused_page_num(table->pager);

  uint32_t splitting_root = is_node_root(old_node);

  void* parent;
  void* new_node;
  if (splitting_root) {
    create_new_root(table, new_page_num);
    parent = get_page(table->pager,table->root_page_num);
   
    old_page_num = *internal_node_child(parent,0);
    old_node = get_page(table->pager, old_page_num);
  } else {
    parent = get_page(table->pager,*node_parent(old_node));
    new_node = get_page(table->pager, new_page_num);
    initialize_internal_node(new_node);
  }
  
  uint32_t* old_num_keys = internal_node_num_keys(old_node);

  uint32_t cur_page_num = *internal_node_right_child(old_node);
  void* cur = get_page(table->pager, cur_page_num);

  internal_node_insert(table, new_page_num, cur_page_num);
  *node_parent(cur) = new_page_num;
  *internal_node_right_child(old_node) = INVALID_PAGE_NUM;
  
  for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--) {
    cur_page_num = *internal_node_child(old_node, i);
    cur = get_page(table->pager, cur_page_num);

    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;

    (*old_num_keys)--;
  }

  *internal_node_right_child(old_node) = *internal_node_child(old_node,*old_num_keys - 1);
  (*old_num_keys)--;

  uint32_t max_after_split = get_node_max_key(table->pager, old_node);

  uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

  internal_node_insert(table, destination_page_num, child_page_num);
  *node_parent(child) = destination_page_num;

  

  update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));

  if (!splitting_root) {
    internal_node_insert(table,*node_parent(old_node),new_page_num);
    *node_parent(new_node) = *node_parent(old_node);
    *(node_next(new_node)) = *(node_next(old_node));
    *(node_next(old_node)) = new_page_num;
    *node_prev(new_node) = old_page_num;
  }
  
}

void borrow_from_left_internal(Pager* pager, void* node, void * left, void* par){
  uint32_t num = *internal_node_num_keys(node);
  *(internal_node_num_keys(node))+=1;
  uint32_t old_max;
  for(uint32_t i= 0; i<*internal_node_num_keys(par);i++){
    if(*internal_node_child(par,i) == *node_prev(node)){
      old_max = *internal_node_key(par,i);
      break;
    }
  }
  for(uint32_t i=num;i>0;i--){
    memcpy(internal_node_cell(node,i),internal_node_cell(node,i-1),INTERNAL_NODE_CELL_SIZE);
  }
  uint32_t node_num = *node_next(left);
  
  *internal_node_child(node,0) = *internal_node_right_child(left);
  uint32_t num2 = *internal_node_num_keys(left);
  *internal_node_key(node,0) = old_max;
  *internal_node_right_child(left) = * internal_node_child(left,num2-1);
  uint32_t new_max = *internal_node_key(left,num2-1);
  *(internal_node_num_keys(left))-=1;

  int32_t child_num = *internal_node_child(node,0);
  void* child = get_page(pager, child_num);
  *node_parent(child) = node_num;

  if(par!=NULL){
    update_internal_node_key(par,old_max,new_max);
  }
}

void borrow_from_right_internal(Pager * pager, void* node, void * right, void * par){

  

  uint32_t num2 = *internal_node_num_keys(node);
  uint32_t old_max;
  for(uint32_t i= 0; i<*internal_node_num_keys(par);i++){
    if(*internal_node_child(par,i) == *node_prev(right)){
      old_max = *internal_node_key(par,i);
      break;
    }
  }

  *(internal_node_num_keys(node))+=1;
  uint32_t node_num = *node_prev(right);
  int32_t child_num = *internal_node_child(right,0);
  void* child = get_page(pager, child_num);
  *node_parent(child) = node_num;

  *internal_node_child(node,num2) = *internal_node_right_child(node);
  *internal_node_key(node,num2) = old_max;
  *internal_node_right_child(node) = *internal_node_child(right,0);
  uint32_t new_max = *internal_node_key(right,0);

  uint32_t num = *internal_node_num_keys(right);
  for(uint32_t i=num-1;i>=1;i--){
    memcpy(internal_node_cell(right,i-1),internal_node_cell(right,i),INTERNAL_NODE_CELL_SIZE);
  }
  *(internal_node_num_keys(right))-=1;

  if(par!=NULL){
    update_internal_node_key(par,old_max,new_max);
  }
}

void merge_internal(void* node, void* left, void* par, Table* table ) {
  uint32_t old_max;
  for(uint32_t i= 0; i<*internal_node_num_keys(par);i++){
    if(*internal_node_child(par,i) == *node_prev(left)){
      old_max = *internal_node_key(par,i);
      break;
    }
  }
  uint32_t node_pg_num = *node_next(left);
  uint32_t num = *internal_node_num_keys(left);
  uint32_t num2 = *internal_node_num_keys(node);
  *internal_node_num_keys(node)+=num+1;
  for(uint32_t i=0;i<num2;i++){
    memcpy(internal_node_cell(node,i+num+1),internal_node_cell(node,i),INTERNAL_NODE_CELL_SIZE);
  }
  for(uint32_t i=0;i<num;i++){
    uint32_t child_pg_num = *internal_node_child(left,i);
    void* child = get_page(table->pager,child_pg_num);
    *node_parent(child) = node_pg_num;
    memcpy(internal_node_cell(node,i),internal_node_cell(left,i),INTERNAL_NODE_CELL_SIZE);
  }
  uint32_t child_pg_num = *internal_node_child(left,num);
 
  void* child = get_page(table->pager,child_pg_num);


  uint32_t new_key = get_node_max_key(table->pager,child);


  *node_parent(child) = node_pg_num;
  *internal_node_key(node,num) = new_key;
  *internal_node_child(node,num) = child_pg_num;


  delete_page(table->pager,*node_prev(node));


  if(is_node_root(par)){
    delete_from_root(table,old_max);
  }
  else{
    delete_from_internal(table,*node_parent(node),old_max);
  }

}

void delete_from_internal(Table* table,uint32_t node_page_num, uint32_t key){
  void* node = get_page(table->pager,node_page_num);
  uint32_t num_cell = *internal_node_num_keys(node);
  uint32_t idx  = internal_node_find_child(node,key); 
  for(uint32_t i=idx+1;i<num_cell;i++){
    memcpy(internal_node_cell(node,i-1),internal_node_cell(node,i),INTERNAL_NODE_CELL_SIZE);
  }
  *internal_node_num_keys(node)-=1;
  if(*internal_node_num_keys(node)<INTERNAL_NODE_MIN_KEYS){
    uint32_t left_pg_num = *node_prev(node);
    uint32_t right_pg_num = *node_next(node);
    uint32_t par_pg_num = *node_parent(node);
    void * left = NULL;
    void * right = NULL;  
    void * par = NULL;
    if(left_pg_num!=INVALID_PAGE_NUM){
      left = get_page(table->pager,left_pg_num);
    }
    if(right_pg_num!=INVALID_PAGE_NUM){
      right = get_page(table->pager,right_pg_num);
    }
    if(par_pg_num!=INVALID_PAGE_NUM){
      par = get_page(table->pager,par_pg_num);
    }
    if((left!=NULL)&&(*internal_node_num_keys(left)>INTERNAL_NODE_MIN_KEYS)&&(*node_parent(left)==*node_parent(node))){
      borrow_from_left_internal(table->pager,node,left,par);
    }
    else if((right!=NULL)&&(*internal_node_num_keys(right)>INTERNAL_NODE_MIN_KEYS)&&(*node_parent(right)==*node_parent(node))){
      borrow_from_right_internal(table->pager,node,right,par);
    }
    else if((left!=NULL)&&(*node_parent(left)==*node_parent(node))){
      merge_internal(node,left,par,table);
    }
    else if((right!=NULL)&&(*node_parent(right)==*node_parent(node))){
      merge_internal(right,node,par,table);
      
    }
    else{

      printf("Error Deleting.\n");
      exit(EXIT_FAILURE);
    }
  }
}