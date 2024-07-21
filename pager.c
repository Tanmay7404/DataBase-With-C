#include "define.h"

Pager* pager_open(const char* filename) {
  int fd = open(filename,
                O_RDWR |     
                    O_CREAT, 
                S_IWUSR |    
                    S_IRUSR  
                );

  if (fd == -1) {
    printf("Unable to open file\n");
    exit(EXIT_FAILURE);
  }
  off_t file_length = lseek(fd, 0, SEEK_END);
  
  Pager* pager = malloc(sizeof(Pager));
  pager->file_descriptor = fd;

  pager->file_length = file_length;

  if (file_length % PAGE_SIZE != 0) {
    printf("Db file is not a whole number of pages. Corrupt file.\n");
    exit(EXIT_FAILURE);
  }

  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    pager->pages[i] = NULL;
  
  }

  void* page0 =  malloc(PAGE_SIZE);
  if(file_length!=0){
    lseek(pager->file_descriptor, 0, SEEK_SET);
    ssize_t bytes_read = read(pager->file_descriptor, page0, PAGE_SIZE);
    if (bytes_read == -1) {
      printf("Error reading file: %d\n", errno);
      exit(EXIT_FAILURE);
    } 
  }
  pager->pages[0] = page0;
  pager->page_used = page0;
  *is_page_used(pager,0) = true;

  if(file_length==0){
    *(table_root(pager)) =1;
    for(int i=1;i<TABLE_MAX_PAGES;i++){
      *(is_page_used(pager,i)) = false;
    }
  }
  

  return pager;
}

void* get_page(Pager* pager, uint32_t page_num) {
  if (page_num > TABLE_MAX_PAGES) {
    printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
           TABLE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }

  if (pager->pages[page_num] == NULL) {
    void* page = malloc(PAGE_SIZE);

    if (*(is_page_used(pager,page_num))) {
      lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
      ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
      if (bytes_read == -1) {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
      }
    }
    *(is_page_used(pager,page_num)) = true;
    pager->pages[page_num] = page;
  }

  return pager->pages[page_num];
}


void pager_flush(Pager* pager, uint32_t page_num) {
  if (pager->pages[page_num] == NULL) {
    printf("Tried to flush null page\n");
    exit(EXIT_FAILURE);
  }

  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

  if (offset == -1) {
    printf("Error seeking: %d\n", errno);
    exit(EXIT_FAILURE);
  }

  ssize_t bytes_written =
      write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);

  if (bytes_written == -1) {
    printf("Error writing: %d\n", errno);
    exit(EXIT_FAILURE);
  }
}

bool* is_page_used(Pager* pager, uint32_t page_num){
  return ((pager->page_used)+ PAGE_USED_OFFSET +page_num*PAGE_USED_SIZE);
} 

uint32_t * table_root(Pager * pager){
  return (pager->page_used); 
}

uint32_t get_unused_page_num(Pager* pager) { 
  for(uint32_t i=0;i<TABLE_MAX_PAGES;i++){
    if(!(*is_page_used(pager,i))){
      *is_page_used(pager,i) = true;
      return i;
    }
  }  
  printf("Memory Full\n");
  exit(EXIT_FAILURE);

}

void delete_page(Pager* pager, uint32_t page_num){
  void * page = get_page(pager,page_num);
  if(*node_next(page)!=INVALID_PAGE_NUM){
    void* node = get_page(pager,*node_next(page));
    *node_prev(node) = *node_prev(page);
  }
  if(*node_prev(page)!=INVALID_PAGE_NUM){
    void* node = get_page(pager,*node_prev(page));
    *node_next(node) = *node_next(page);
  }

  *(is_page_used(pager,page_num)) = false;
}

void serialize_row(Row* source, void* destination) {
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
  memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination) {
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

