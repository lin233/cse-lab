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
      d->read_block(BBLOCK(blockId),buf);//buf is a block
      int idx = (blockId%BPB)/8 ;// buf[idx]  
      int x = buf[idx]; 
      
      int mark = !((~x)&(1<<((blockId%BPB)%8)));//if blockId can alloc, mark is 0 
      if(mark == 0){
        buf[idx] = (~x)|(1<<((blockId%BPB)%8));
        d->write_block(BBLOCK(blockId),buf);
        //printf("finish alloc_block\n");
      //  printf("----- blockID ---:%d\n", blockId);
        return blockId;
      }
      blockId++;
    }
   // printf("finish alloc_block11\n");
    return blockId;
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
   
   int bNum = (inode->size-1)/BLOCK_SIZE + 1; //blockNum;

 
   char init_buf_out[bNum*BLOCK_SIZE];

   *buf_out = (char*)malloc(inode->size);

   char buf[BLOCK_SIZE];

   if(bNum > MAXFILE){
     return ;//too large file
   }

   blockid_t in_blocks[NINDIRECT];
  
   if(bNum <= NDIRECT){//only use NDIRECT
     for(int i = 0;i < bNum;i++){
        blockid_t blockId = inode->blocks[i];
       // printf("blockId :%d\n", blockId);
        bm->read_block(blockId, init_buf_out+i*BLOCK_SIZE);
     }

   }
   else{ //in_blocks
     for(int i = 0;i<NDIRECT;i++){
        blockid_t blockId =  inode->blocks[i];
        bm->read_block(blockId,init_buf_out+i*BLOCK_SIZE);
     }
   //  printf("test read_file ---- 8 ---- \n");
     
     bm->read_block(inode->blocks[NDIRECT],buf);
     
     memcpy(in_blocks,buf,(sizeof(blockid_t))*(bNum-NDIRECT));//copy blockid
     
     for(int j = NDIRECT;j<bNum;j++){
   //   printf("test read_file ---- 9 ---- \n");
        blockid_t blockId =  in_blocks[j-NDIRECT];
        bm->read_block(blockId,init_buf_out+j*BLOCK_SIZE);
     }

   }
   
   printf("\tinit_buf_out %s\n", init_buf_out);

   memcpy(*buf_out,init_buf_out,inode->size);
   
  //  printf("\tbuf_out %s\n", *buf_out);

   *size = inode->size;
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

   if( (size <= 0 )|| size > (MAXFILE*BLOCK_SIZE)){printf("finish write_file\n");return;}//too big or too small
   inode_t *inode = get_inode(inum);
   
   if(inode==NULL){printf("finish write_file\n");return;}//bad inode
   

   blockid_t old_in_blocks[NINDIRECT];
   blockid_t new_in_blocks[NINDIRECT];

   int old_block_num = (inode->size-1)/BLOCK_SIZE+1;
   int new_block_num = (size-1)/BLOCK_SIZE+1;
 //  printf("\tnew_block_num  %d\n", new_block_num);
  // memcpy(old_blocks,inode->blocks,NDIRECT*sizeof(blockid_t));

   if(old_block_num > NDIRECT){ 
     bm->read_block(inode->blocks[NDIRECT],blockbuf);//indirect block
     memcpy(old_in_blocks,blockbuf,BLOCK_SIZE);
   }
   
   if(new_block_num < old_block_num){//no change to blocks, just write and free block
      for(int i = 0;i<old_block_num;i++){
        if(i < new_block_num){
          bm->write_block(inode->blocks[i],buf+BLOCK_SIZE*i);
        }
        else{
         // bm->free_block(old_in_blocks[i-NDIRECT]);
         bm->free_block(inode->blocks[i]);
        }
      }
   }
   else {//new_block num >= old_block_num;write, alloc and change the blocks
    if(new_block_num<=NDIRECT){// write and alloc change blocks  
      for(int i = 0;i<new_block_num;i++){//old write, new alloc 
        if(i<old_block_num-1){
          bm->write_block(inode->blocks[i],buf+i*BLOCK_SIZE);
        }
        else{
          blockid_t new_block_id = bm->alloc_block();
          inode->blocks[i] = new_block_id;
          bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
        }
      }
    }
    else{//new_block_num>NDIRECT,
      for(int i = 0;i<NDIRECT;i++){//old write, new alloc 
        if(i<old_block_num){
          bm->write_block(inode->blocks[i],buf+i*BLOCK_SIZE);
        }
        else{
          blockid_t new_block_id = bm->alloc_block();
          inode->blocks[i] = new_block_id;
          bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
        }
      }
      
      for(int i=NDIRECT;i<new_block_num; i++){
        if(i<old_block_num){
          bm->write_block(inode->blocks[i],buf+i*BLOCK_SIZE);
        }
        else{
          blockid_t new_block_id = bm->alloc_block();
          new_in_blocks[i-NDIRECT] = new_block_id;
          bm->write_block(new_block_id,buf+i*BLOCK_SIZE);
        }
      }

      blockid_t in_blockId = bm->alloc_block();
      inode->blocks[NDIRECT] = in_blockId;
      memcpy(blockbuf,new_in_blocks,BLOCK_SIZE);
      bm->write_block(in_blockId,blockbuf);
    }
   }
   
   inode->size = size;
   put_inode(inum,inode);
   
   char *buf2=NULL;
   int size1 = 0;
   read_file(inum, &buf2, &size);

   printf("after write_file :%s\n",buf2);
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

  int block_num = (inode->size-1)/BLOCK_SIZE+1;
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
