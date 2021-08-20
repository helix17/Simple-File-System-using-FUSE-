#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct filep {
    char name[128];
    char type;
    char content[256];
    struct filep* next;
    int link;
    mode_t perm;
    uid_t user_id;
    gid_t group_id;
    time_t access_t;
    time_t created_t;
    time_t mod_t;
} filep;

int no_files=0;
filep* head=NULL;

void newfile(const char *name){
    filep* newf=(filep*)malloc(sizeof(filep));
    newf->next=head;
    head=newf;
    strcpy(newf->name,name);
    strcpy(newf->content,"");
    newf->type='f';
    newf->link=1;
    newf->user_id=getuid();
    newf->group_id=getgid();
    newf->perm=S_IFREG | 0644;
    newf->access_t=time(NULL);
    newf->created_t=time(NULL);
    newf->mod_t=time(NULL);
    no_files++;
}

filep* findfile(const char *path){
    filep* trav;
    path++;
    trav=head;
    for(int i=0;i<no_files;i++){
        if(strcmp(trav->name,path)==0 && trav->type=='f')
            return trav;
        trav=trav->next;
    }
}

int infile(const char *path){
    filep* trav;
    path++;
    trav=head;
    for(int i=0;i<no_files;i++){
        if(strcmp(trav->name,path)==0 && trav->type=='f')
            return 1;
        trav=trav->next;
    }
    return 0;
}

void newdir(const char *name){
    filep* newf=(filep*)malloc(sizeof(filep));
    newf->next=head;
    head=newf;
    strcpy(newf->name,name);
    strcpy(newf->content,"");
    newf->type='d';
    newf->link=2;
    newf->user_id=getuid();
    newf->group_id=getgid();
    newf->perm=S_IFDIR | 0755;
    newf->access_t=time(NULL);
    newf->created_t=time(NULL);
    newf->mod_t=time(NULL);
    no_files++;
}

filep* finddir(const char *path){
    filep* trav;
    path++;
    trav=head;
    for(int i=0;i<no_files;i++){
        if(strcmp(trav->name,path)==0 && trav->type=='d')
            return trav;
        trav=trav->next;
    }
}

int indir(const char *path){
    filep* trav;
    path++;
    trav=head;
    for(int i=0;i<no_files;i++){
        if(strcmp(trav->name,path)==0 && trav->type=='d')
            return 1;
        trav=trav->next;
    }
    return 0;
}

void file_write(const char *path,const char *content){
    filep* rfile;
    if(infile(path)==0)
        return;
    rfile=findfile(path);
    strcpy(rfile->content,content);
    rfile->access_t=time(NULL);
    rfile->mod_t=time(NULL);
}

static int fn_getattr(const char *path, struct stat *st){
    st->st_uid=getuid();
	st->st_gid=getgid();
	st->st_atime=time( NULL );
	st->st_mtime=time( NULL );
    if(strcmp(path,"/")==0 || indir(path)==1){
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if(infile(path)==1){
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
	}
	else{
		return -ENOENT;
	}
	return 0;
}

static int fn_readdir(const char *path,void *buffer,fuse_fill_dir_t filler,off_t offset,struct fuse_file_info *fi){
    filler(buffer,".",NULL,0);
	filler(buffer,"..",NULL,0);
	filep* req=head;
    if(strcmp(path,"/")==0){
        for(int i=0;i<no_files;i++){
            filler(buffer,req->name,NULL,0);
            req=req->next;
        }
    }
    return 0;
}

static int fn_read(const char *path,char *buffer,size_t size,off_t offset,struct fuse_file_info *fi ){
    filep* rfile;
    if(infile(path)==0)
        return -1;
    rfile=findfile(path);
    char *content=rfile->content;
	memcpy(buffer,content+offset,size );
	return strlen(content)-offset;
}

static int fn_mkdir(const char *path,mode_t mode){
	path++;
	newdir(path);
	return 0;
}

static int fn_mknod(const char *path,mode_t mode,dev_t rdev){
	path++;
	newfile(path);
	return 0;;
}

static int fn_write(const char *path,const char *buffer,size_t size,off_t offset,struct fuse_file_info *info){
	file_write(path,buffer);
	return size;
}

int fn_rename(const char* path, const char* to){
    filep* rfile=NULL;
    if(infile(path)==1)
        rfile=findfile(path);
    if(indir(path)==1)
        rfile=finddir(path);
    if(rfile==NULL)
        return -ENOENT;
    strcpy(rfile->name,to+1);
    return 0;
}

int fn_rmdir(const char* path){
    if(indir(path)==1){
        filep* trav;
        filep* prev;
        int chk=0;
        path++;
        trav=head;
        prev=head;
        for(int i=0;i<no_files;i++){
            if(strcmp(trav->name,path)==0 && trav->type=='d')
                break;
            prev=trav;
            trav=trav->next;
            chk=1;
        }
        if(chk==0)
            head=head->next;
        else
            prev->next=trav->next;
        no_files--;
        return 0;
    }
    else
       return -ENOENT;
}

int fn_rm(const char* path){
    if(infile(path)==1){
        filep* trav;
        filep* prev;
        int chk=0;
        path++;
        trav=head;
        prev=head;
        for(int i=0;i<no_files;i++){
            if(strcmp(trav->name,path)==0 && trav->type=='f')
                break;
            prev=trav;
            trav=trav->next;
            chk=1;
        }
        if(chk==0)
            head=head->next;
        else
            prev->next=trav->next;
        free(trav);
        no_files--;
        return 0;
    }
    else
       return -ENOENT;
}

static struct fuse_operations operations={
    .getattr=fn_getattr,
    .readdir=fn_readdir,
    .read	=fn_read,
    .mkdir	=fn_mkdir,
    .mknod	=fn_mknod,
    .write	=fn_write,
    .rename =fn_rename,
    .rmdir  =fn_rmdir,
    .unlink =fn_rm
};

int main(int argc,char *argv[]){
    return fuse_main(argc,argv,&operations,NULL);
}
