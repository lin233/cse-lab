// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    lc->acquire(inum);
    bool r = isfile2(inum);
    lc->release(inum);
    return r;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    lc->acquire(inum);
    bool r = isdir2(inum);
    lc->release(inum);
    return r;
}


bool 
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    lc->acquire(inum);
    bool r = issymlink2(inum);
    lc->release(inum);
    return r;
}

int yfs_client::getsymlink(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getsymlink %016llx\n", inum);
    extent_protocol::attr a;

    lc->acquire(inum);
    r = ec->getattr(inum, a);

    if (a.type != extent_protocol::T_SYMLINK){lc->release(inum);return IOERR;}

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getsymlink %016llx -> sz %llu\n", inum, fin.size);

release:
    lc->release(inum);
    return r;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

   // printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;

    
    lc->acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
    printf("begin setattr\n");
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
     if( size<0)return IOERR;

     lc->acquire(ino);

     std::string content;
     ec->get(ino,content);

     int old_size = content.size();
     if(old_size==size){
         lc->release(ino);
         return r;
     }
     if(old_size > size){
         content = content.substr(0,size);
         ec->put(ino,content);
     }
     else{
        content.resize(size,'\0');
        ec->put(ino,content);
     }
 
     lc->release(ino);
     return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     lc->acquire(parent);
     r = create_sth(parent,name,mode,ino_out,extent_protocol::T_FILE);
     lc->release(parent);
     return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     lc->acquire(parent);
     r = create_sth(parent,name,mode,ino_out,extent_protocol::T_DIR);
     lc->release(parent);
     return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    printf("begin lookup\n");
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

     lc->acquire(parent);
     r = lookup2(parent,name,found,ino_out);
     lc->release(parent);
     return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    printf("begin readdir\n");
    /*
     * your code goes here.
     * note: you should parse the directory content using your defined format,
     * and push the dirents to the list.
     */

     /*
      * filename/inum/filename/inum/
      */

     lc->acquire(dir);
     if (!isdir2(dir))return IOERR;

     std::stringstream stream; 
     std::string content;
     list.clear();

     char c;

     r = ec->get(dir,content);
     int fname_begin=0;
     int fname_final;
     int inum_begin;
     int inum_final;
     std::string fname;

     while(content.find('/')!=-1){

        fname_final = content.find('/');
        
        fname = content.substr(fname_begin,fname_final);

        content = content.substr(fname_final+1);

        inum_final=content.find('/');

        stream.clear();
        stream << content.substr(0,inum_final);

        int inumber ;
        stream>>inumber;

        if(inum_final!=content.length()){
            content = content.substr(inum_final+1);
        }
        else{
            content = "";
        }
        dirent dir;
        dir.name = fname;
        dir.inum = inumber;
        list.push_back(dir);
     }
    lc->release(dir);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    printf("begin read\n");
    /*
     * your code goes here.
     * note: read using ec->get().
     */
     lc->acquire(ino);
     if(!isfile2(ino))return IOERR;
     
     std::string content;
     r = ec->get(ino,content);

     int len_content = content.length();
     
     if(off >= len_content){
         data = "";
     }
     else{
         data = content.substr(off,size);
     }
    
     lc->release(ino);
     return r;
     
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    printf("begin write\n");
     /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
     lc->acquire(ino);
     if(!isfile2(ino))return IOERR;
     std::string content;
     
     r = ec->get(ino,content);
     std::string buf(data,size);

     int len_content = content.size();


     if(off > len_content){
         content.resize(off,'\0');
         content = content + buf;
         r=ec->put(ino,content);
         bytes_written = off+size-len_content;
     }
     else if(off==len_content){
         content+=buf;
         r=ec->put(ino,content);
         bytes_written = size;
     }
     else if(off+size< len_content){
         content = content.substr(0,off)+buf+content.substr(off+size,len_content-off-size);
         r=ec->put(ino,content);
         bytes_written = size;
     }

     else{
         content = content.substr(0,off)+buf;
         printf("already to write content :%s\n",content.c_str());
         r=ec->put(ino,content);
         bytes_written = size;
         
     }
     r=ec->get(ino,buf);
     lc->release(ino);
     return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    
    if (!name)
        return NOENT;
    std::string fname = std::string(name);
    if((fname.find('/')!=-1)||fname.find('\0')!=-1||fname.length()==0)return NOENT;

    lc->acquire(parent);

    if (!isdir2(parent))
    return IOERR;

    std::list<dirent> list;
    r = readdir2(parent, list);
    if (r != OK){ 
        lc->release(parent);
        return r;
    }
    
    std::string content;
    std::string new_content="";
    std::stringstream stream;  
    std::list<dirent>::iterator it;
    std::list<dirent>::iterator it2;

    r = NOENT;
    
    for (it = list.begin(); it != list.end(); it++){
        if(it->name==fname){
            if (!isfile2(it->inum) && !issymlink2(it->inum)) return IOERR;
            if(ec->remove(it->inum)!= extent_protocol::OK)return IOERR ;
            list.erase(it);
            r = OK;
            break;
        }
    }

    for(it2=list.begin();it2 != list.end(); it2++){
            stream.clear();
            stream << it2->inum;  
            std::string inumber;
            stream >> inumber;
            std::string fname = it2->name;
            new_content += fname + '/' + inumber +'/';
    }
    ec->put(parent,new_content);
    lc->release(parent);
    return r;
 
}


int yfs_client::symlink(inum parent, const char *name, const char *target, inum &ino_out)
{
    // Check input parameters.
    int r = OK;
    if (!target || !strlen(target))
        return IOERR;

    // Create a symlink type inode.
    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     lc->acquire(parent);
     r = create_sth(parent,name,0,ino_out,extent_protocol::T_SYMLINK);
 
    if (r != OK)
        goto release;

    
    // Write target path as its content.
    lc->acquire(ino_out);

    r = ec->put(ino_out, std::string(target));

    lc->release(ino_out);

release:
    lc->release(parent);
    return r;
}

int yfs_client::readlink(inum ino, std::string &target)
{
    int r = OK;
    lc->acquire(ino);

    if (!issymlink2(ino)) {
        target.clear();
        lc->release(ino);
        return IOERR;
    }

    r = ec->get(ino, target);
    lc->release(ino);
    return r;
}


///////

int
yfs_client::create_sth(inum parent, const char *name, mode_t mode, inum &ino_out,uint32_t type)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     printf("begin create\n");
     if(!isdir2(parent))return IOERR;
     
     bool found;
     lookup2(parent,name,found,ino_out);
     
     if(found){
         printf("faile create -- exist"); 
         return EXIST;
     }
     
     if(name==NULL){return IOERR;}
     
     std::string fname = std::string(name);

     if((fname.find('/')!=-1)||fname.find('\0')!=-1||fname.length()==0){
        return IOERR;
     }
     if(ec->create(type,ino_out)!=extent_protocol::OK){//create file 
         return IOERR;
     }

     std::string content;
     std::string newfile;
     std::string inumber;
     std::stringstream stream;  
     stream << ino_out;  
     stream >> inumber;
     newfile = fname+'/'+ inumber +'/';

     ec->get(parent,content);
     content += newfile;
     ec->put(parent,content);
     printf("finish create\n");
    
     return r;
}


bool
yfs_client::isfile2(inum inum)
{
    
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
         
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);

    
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir2(inum inum)
{
    // Oops! is this still correct when you implement symlink?
   
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
    
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
      
        return true;
    } 
    printf("isfile: %lld is not a dir\n", inum);
  
    return false;
}


bool 
yfs_client::issymlink2(inum inum)
{
     extent_protocol::attr a;
   
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
     
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("isfile: %lld is a symlink\n", inum);
  
        return true;
    } 
    printf("isfile: %lld is not a symblink\n", inum);
   
    return false;
}


int
yfs_client::readdir2(inum dir, std::list<dirent> &list)
{
    int r = OK;
    printf("begin readdir\n");
    /*
     * your code goes here.
     * note: you should parse the directory content using your defined format,
     * and push the dirents to the list.
     */

     /*
      * filename/inum/filename/inum/
      */
     if (!isdir2(dir))return IOERR;
 
     std::stringstream stream; 
     std::string content;
     list.clear();

     char c;

     r = ec->get(dir,content);
     int fname_begin=0;
     int fname_final;
     int inum_begin;
     int inum_final;
     std::string fname;

     while(content.find('/')!=-1){

        fname_final = content.find('/');
        
        fname = content.substr(fname_begin,fname_final);

        content = content.substr(fname_final+1);

        inum_final=content.find('/');

        stream.clear();
        stream << content.substr(0,inum_final);

        int inumber ;
        stream>>inumber;

        if(inum_final!=content.length()){
            content = content.substr(inum_final+1);
        }
        else{
            content = "";
        }

        dirent dir;
    
        dir.name = fname;
        dir.inum = inumber;

        list.push_back(dir);

     }
     printf("finish readdir\n");
 
    return r;
}


int
yfs_client::lookup2(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    printf("begin lookup\n");
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
     if(!name)return IOERR;
     found = false;
     if(!isdir2(parent))return IOERR;
     std::list<dirent> myList;
     readdir2(parent,myList);

     std::list<dirent>::iterator itor;
     itor = myList.begin();

     while(itor!=myList.end())
     {
         if(itor->name == name){
             found = true;
             ino_out = itor->inum;
             return r;
         }
         itor++;
     } 
     printf("finish lookup\n");
 
     return r;
}