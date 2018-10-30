#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
  //bzero　功能：置字节字符串s的前n个字节为零且包括‘\0’。
}

void
disk::read_block(blockid_t id, char *buf)
{
  memcpy(buf, blocks[id], BLOCK_SIZE);
  //printf("finish read_block\n");
}

void
disk::write_block(blockid_t id, const char *buf)
{
  memcpy(blocks[id], buf, BLOCK_SIZE);
  //printf("finish  write_file\n");
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
   //printf("begin alloc_block\n");
   char buf[BLOCK_SIZE];  //bblock

   blockid_t MinDataBlock = IBLOCK(INODE_NUM, sb.nblocks)+1;//最大的inode之后一个block就是datablock了

   blockid_t blockId = MinDataBlock;

   while(blockId < BLOCK_NUM){
      d->read_block(BBLOCK(blockId),buf);// Block containing bit for block blockId

      int idx = (blockId%BPB)/8;

      char trybyte = buf[idx];

      int trybit = idx%8;

      int mark = (trybyte >> trybit)& 0x1 ;

      if(mark == 0){
        char alloc_byte = trybyte | (0x1 << trybit);
        buf[idx] = alloc_byte;
        d->write_block(BBLOCK(blockId),buf);
        return blockId;
      }

      blockId++;
   }
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding
   * bit in the block bitmap when free.
   */
   //printf("begin free_block\n");
   char buf[BLOCK_SIZE];//bblock

   d->read_block(BBLOCK(id),buf);//buf is bblock

   int idx = (id%BPB)/8;

   buf[idx] &= ~(1<<((id%BPB)%8));
   d->write_block(BBLOCK(id),buf);

   //free(buf);//
   printf("finish free_block\n");
   return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;


}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
  //  printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should
   * begin from the 2nd inode block.
   * the 1st is used for root_dir, 
   * see inode_manager::inode_manager().
   */
   //printf("begin alloc_inode\n");
   int inum = 1;
   inode_t *inode = get_inode(inum);
   inode_t *ino_disk;
  
   char buf[BLOCK_SIZE];

   while(inode != NULL){
     inum++;
     inode = get_inode(inum);
   }
   //找到能够用的inode
  
   bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);//拿到那个inode对应的block
   ino_disk = (struct inode*)buf + inum%IPB;//inode
   ino_disk->type = type; //alloc
   ino_disk->size = 0;
   ino_disk->atime = (unsigned int)time(NULL);
   ino_disk->mtime = (unsigned int)time(NULL);
   ino_disk->ctime = (unsigned int)time(NULL);
   bm->write_block(IBLOCK(inum, bm->sb.nblocks),buf);
  // printf("finish alloc_inode\n");
   return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
 //  printf("begin free_inode\n");

   inode_t *inode = get_inode(inum);
   if(inode==NULL)return;

   inode->type = 0;
   put_inode(inum,inode);
   free(inode);

 //  printf("finish free_inode\n");
   return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

 // printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
 //   printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

 // printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  
   //printf("begin read_file\n");

   inode_t *inode = get_inode(inum);//get inode 
   *size = inode->size;
   *buf_out = (char*)malloc(inode->size);

   int complete_block_num = inode->size/BLOCK_SIZE; //blockNum;
   int last_block_bytes = inode->size%BLOCK_SIZE;
  // int last_block_num;

   if(last_block_bytes!=0){
      complete_block_num ++;
   }
   else{
     last_block_bytes=BLOCK_SIZE;
   }

   
  // char init_buf_out[(bNum*BLOCK_SIZE)];

   char buf[BLOCK_SIZE];

   if(complete_block_num > MAXFILE){
     return ;//too large file
   }

   blockid_t in_blocks[NINDIRECT];
  
   if(complete_block_num <= NDIRECT){//only use NDIRECT
     for(int i = 0;i < complete_block_num;i++){
        if(i!=complete_block_num-1){
          blockid_t blockId = inode->blocks[i];
         // printf("blockId :%d\n", blockId);
          bm->read_block(blockId, *buf_out+i*BLOCK_SIZE);
         // if(i==0) printf("test read[0] blockId :%s\n", init_buf_out);
        }
        else{
          bm->read_block(inode->blocks[i], buf);
          memcpy(*buf_out + i * BLOCK_SIZE, buf, last_block_bytes);
        }
     }
   }
   else{ //in_blocks
     bm->read_block(inode->blocks[NDIRECT],buf);
     memcpy(in_blocks,buf,sizeof(in_blocks));//copy blockid
     for(int i = 0;i<complete_block_num;i++){
       if(i < NDIRECT){
          blockid_t blockId = inode->blocks[i];
          bm->read_block(blockId,*buf_out+i*BLOCK_SIZE);
         //  if(i==0) printf("test read[0] blockId :%s\n", init_buf_out);
        }
        else if(i<complete_block_num-1){
          //blockid_t blockId = in_blocks[i-NDIRECT];
          bm->read_block(in_blocks[i-NDIRECT],*buf_out+i*BLOCK_SIZE);
        }
        else{
          bm->read_block(in_blocks[i-NDIRECT], buf);
          memcpy(*buf_out + i*BLOCK_SIZE, buf, last_block_bytes);
        }
      }
     //  printf("test read_file ---- 8 ---- \n");
   }
   //printf("\tinit_buf_out %s\n", init_buf_out);
   //memcpy(*buf_out,init_buf_out,inode->size);
   printf("\twhatbuf_out %s\n", *buf_out);
   // *size = inode->size;
   free(inode);
   printf("finish read_file\n");

   return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  // printf("begin write_file\n");
   char blockbuf[BLOCK_SIZE];
   char last_block_buf[BLOCK_SIZE];

   //if((size < 0 )|| size > (MAXFILE*BLOCK_SIZE)){printf("finish write_file--too big or too small \n");return;}//too big or too small
   
   inode_t *inode = get_inode(inum);//
   
   if(inode==NULL){printf("bad inode\n");return;}//bad inode
   

   blockid_t old_in_blocks[NINDIRECT];
   blockid_t new_in_blocks[NINDIRECT];


   int old_block_num = (inode->size+BLOCK_SIZE-1)/BLOCK_SIZE;
   int new_block_num = (size+BLOCK_SIZE-1)/BLOCK_SIZE;
   int last_bytes = size%BLOCK_SIZE;
   //  printf("\tnew_block_num  %d\n",   new_block_num);
   // memcpy(old_blocks,inode->blocks,NDIRECT*sizeof(blockid_t));
   if(last_bytes==0)last_bytes=BLOCK_SIZE;
 
    if(old_block_num > NDIRECT){ 
      bm->read_block(inode->blocks[NDIRECT],blockbuf);//indirect block
      memcpy(old_in_blocks,blockbuf,sizeof(old_in_blocks));
      printf("-blocksize :%d\n",sizeof(old_in_blocks));
    }
    
    if(new_block_num <= old_block_num){//no change to blocks, just write and free block
        if(old_block_num <= NDIRECT){
          for(int i = 0;i<old_block_num;i++){//new<old<ndirect
            if(i < new_block_num-1){
              bm->write_block(inode->blocks[i],buf+BLOCK_SIZE*i);
            }
            else if(i==new_block_num-1){
              memcpy(last_block_buf, buf + i * BLOCK_SIZE, last_bytes);
              bm->write_block(inode->blocks[i],last_block_buf);
            }
            else{
            // bm->free_block(old_in_blocks[i-NDIRECT]);
              bm->free_block(inode->blocks[i]);
            }
          }
        }
        else if((new_block_num<=NDIRECT) && (NDIRECT<old_block_num)){
          for(int i = 0;i<old_block_num;i++){//new<old<ndirect
            if(i < new_block_num-1){
              bm->write_block(inode->blocks[i],buf+BLOCK_SIZE*i);
            }
            else if(i==new_block_num-1){
              memcpy(last_block_buf, buf + i * BLOCK_SIZE, last_bytes);
              bm->write_block(inode->blocks[i],last_block_buf);
            }
            else if(i>=new_block_num && i<NDIRECT){
            // bm->free_block(old_in_blocks[i-NDIRECT]);
              bm->free_block(inode->blocks[i]);
            }
            else{//i>=NDIRECT && i<old_block_num
              bm->free_block(old_in_blocks[i-NDIRECT]);
            }
          }
        }
        else {//NDIRECT<new<old
          for(int i = 0;i<old_block_num;i++){
            if(i < NDIRECT){
              bm->write_block(inode->blocks[i],buf+BLOCK_SIZE*i);
            }
            else if(i>=NDIRECT&&i<new_block_num-1){
              new_in_blocks[i-NDIRECT]=old_in_blocks[i-NDIRECT];
              bm->write_block(old_in_blocks[i-NDIRECT],buf+BLOCK_SIZE*i);
            }
            else if(i==new_block_num-1){
              memcpy(last_block_buf, buf + i * BLOCK_SIZE, last_bytes);
              new_in_blocks[i-NDIRECT]=old_in_blocks[i-NDIRECT];
              bm->write_block(old_in_blocks[i-NDIRECT],last_block_buf);
            }
            else{
              bm->free_block(old_in_blocks[i-NDIRECT]);
            }
          }
        }
    }

    else {//new_block num >= old_block_num;write, alloc and change the blocks
        if(new_block_num <= NDIRECT){// write and alloc change blocks  
          for(int i = 0;i<new_block_num;i++){//old write, new alloc 
            if(i<old_block_num){
              bm->write_block(inode->blocks[i],buf+i*BLOCK_SIZE);
            }
            else if(i<new_block_num-1){
              blockid_t new_block_id = bm->alloc_block();
              inode->blocks[i] = new_block_id;
              bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
            }
            else{
              blockid_t new_block_id = bm->alloc_block();
              inode->blocks[i] = new_block_id;
              memcpy(last_block_buf, buf + i * BLOCK_SIZE, last_bytes);
              bm->write_block(new_block_id,last_block_buf);
            }
          }
          
        }
        else if(old_block_num<=NDIRECT && NDIRECT<new_block_num){//new_block_num>NDIRECT,
          for(int i = 0;i<new_block_num;i++){//old write, new alloc 
            if(i<old_block_num){
              bm->write_block(inode->blocks[i],buf+i*BLOCK_SIZE);
            }
            else if(old_block_num<=i && i<NDIRECT){
              blockid_t new_block_id = bm->alloc_block();
              inode->blocks[i] = new_block_id;
              bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
            }
            else if(i<new_block_num-1){
              blockid_t new_block_id = bm->alloc_block();
              new_in_blocks[i-NDIRECT] = new_block_id;
              bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
            }
            else{
              blockid_t new_block_id = bm->alloc_block();
              new_in_blocks[i-NDIRECT] = new_block_id;
              memcpy(last_block_buf, buf + i * BLOCK_SIZE, last_bytes);
              bm->write_block(new_block_id,last_block_buf);
            }
          }
        }
        else{//NDIRECT<=old_block_num<new_blocknum
          for(int i=0;i<new_block_num; i++){
            if(i < NDIRECT){
              bm->write_block(inode->blocks[i],buf+i*BLOCK_SIZE);
            }
            else if(i<old_block_num){
              new_in_blocks[i-NDIRECT] = old_in_blocks[i-NDIRECT];
              bm->write_block(old_in_blocks[i-NDIRECT],buf+i*BLOCK_SIZE);
            }
            else if(i<new_block_num-1){// NDIRECT<old_block_num<=i <new_block_num
              blockid_t new_block_id = bm->alloc_block();
              new_in_blocks[i-NDIRECT] = new_block_id;
              bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
            }
            else{
              memcpy(last_block_buf, buf + i * BLOCK_SIZE, last_bytes);
              blockid_t new_block_id = bm->alloc_block();
              new_in_blocks[i-NDIRECT] = new_block_id;
              bm->write_block(new_block_id,last_block_buf);
            }
          }
        }  
    }
    if(new_block_num > NDIRECT){
        blockid_t in_blockId = bm->alloc_block();
        inode->blocks[NDIRECT] = in_blockId;
        memcpy(blockbuf,new_in_blocks,BLOCK_SIZE);
        bm->write_block(in_blockId,blockbuf); 
    }
 

   inode->size = size;
   put_inode(inum,inode);
   
   char *buf2 = NULL;
   
   //int size1 = 0;
   //read_file(inum, &buf2, &size1);
   int block_number = inode->blocks[0];
    

   printf("what your read after write_file11 :%s\n",buf2);
 
   printf("should write_file :%s\n",buf);
   
   return;

}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  /// printf("begin getattr\n");
  inode_t* inode = get_inode(inum);

  if(inode ==NULL)return;
  
  a.type = inode->type;
  a.atime = inode->atime;
  a.mtime = inode->mtime;
  a.ctime = inode->ctime;
  a.size = inode->size;

  free(inode);
 // printf("finish getattr\n");
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
 //  printf("begin remove_file\n");
  inode_t *inode = get_inode(inum);

  int block_num = (inode->size+BLOCK_SIZE-1)/BLOCK_SIZE;
  char buf[BLOCK_SIZE];

  if(block_num<=NDIRECT){
    for(int i = 0;i<block_num;i++){ 
      bm->free_block(inode->blocks[i]);
    }
  }

  else{
    for(int i = 0;i<NDIRECT;i++){ 
      bm->free_block(inode->blocks[i]);
    }

    blockid_t in_blocks[NINDIRECT];
    bm->read_block(inode->blocks[NDIRECT],buf);//indirect block
    memcpy(in_blocks,buf,BLOCK_SIZE);

    for(int i = NDIRECT;i<block_num;i++){

      bm->free_block(in_blocks[i-NDIRECT]);
    
    }
  
  }

  free_inode(inum);
//  printf("finish remove_file\n");
  return;
}
