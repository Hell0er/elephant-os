#include "dir.h"
#include "stdint.h"
#include "inode.h"
#include "file.h"
#include "fs.h"
#include "stdio-kernel.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"
#include "super_block.h"

struct dir root_dir;   // 根目录

/* 打开根目录 */
void open_root_dir(struct partition* part){
    root_dir.inode=inode_open(part,part->sb->root_inode_no);
    root_dir.dir_pos=0;
}

/* 在分区part上打开inode_no的目录并返回目录指针 */
struct dir* dir_open(struct partition* part,uint32_t inode_no){
    struct dir* pdir=(struct dir*)sys_malloc(sizeof(struct dir));
    pdir->inode=inode_open(part,inode_no);
    pdir->dir_pos=0;
    return pdir;
}

/* 在part分区内的pdir目录内寻找名为name的文件或目录，
 * 找到后返回true并将其目录项存入dir_e，否则返回false */
bool search_dir_entry(struct partition* part,struct dir* pdir,\
        const char* name,struct dir_entry* dir_e){
    uint32_t block_cnt=140;   // 12个直接块+128个间接块=140块

    /* 共560B */
    uint32_t* all_blocks=(uint32_t*)sys_malloc(48+512);
    if (all_blocks==NULL){
    	printk("search_dir_entry: sys_malloc for all_blocks failed");
    	return false;
    }

    uint32_t block_idx=0;
    while (block_idx<12){
    	all_blocks[block_idx]=pdir->inode->i_sectors[block_idx];
    	++block_idx;
    }
    block_idx=0;

    if (pdir->inode->i_sectors[12]!=0){   // 若含有一级间接块表
    	ide_read(part->my_disk,pdir->inode->i_sectors[12],all_blocks+12,1);
    }
    /* 至此，all_blocks存储的是该文件或目录的所有扇区地址 */

    /* 目录项不跨区
     * 这样目录项容易处理，只申请容纳一个扇区的内存 */
    uint8_t* buf=(uint8_t*)sys_malloc(SECTOR_SIZE);
    struct dir_entry* p_de=(struct dir_entry*)buf;   // p_de为指向目录项的指针
    uint32_t dir_entry_size=part->sb->dir_entry_size;
    uint32_t dir_entry_cnt=SECTOR_SIZE/dir_entry_size;   // 一个扇区可容纳的目录项个数

    /* 开始在所有块中查找目录项 */
    while (block_idx<block_cnt){
    /* 块地址为0时表示该块中无数据，继续在其它块中找 */
    	if (all_blocks[block_idx]==0){
            ++block_idx;
            continue;
    	}
	ide_read(part->my_disk,all_blocks[block_idx],buf,1);

	uint32_t dir_entry_idx=0;
    	/* 遍历扇区中所有目录项 */
    	while (dir_entry_idx<dir_entry_cnt){
    	    /* 若找到，就直接复制整个目录项 */
    	    if (!strcmp(p_de->filename,name)){
    	    	memcpy(dir_e,p_de,dir_entry_size);
            	sys_free(buf);
            	sys_free(all_blocks);
            	return true;
            }
            ++dir_entry_idx;
            ++p_de;
    	}
    	++block_idx;
    	p_de=(struct dir_entry*)buf;   // 此时p_de已经指向扇区内最后一个完整目录项了，需要恢复p_de指向buf
        memset(buf,0,SECTOR_SIZE);   // buf清0
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/* 关闭目录 */
void dir_close(struct dir* dir){
    /********** 根目录不能关闭 ***********
     * 1.根目录自打开后就不能关闭，否则还需要再次open_root_dir()，
     * 2.root_dir所在的内存是低端1M之内，并非在堆中，free()会出现问题 */
    if (dir==&root_dir){
    	/* 不做任何处理直接返回 */
    	return;
    }
    inode_close(dir->inode);
    sys_free(dir);
}

/* 在内存中初始化目录项p_de */
void create_dir_entry(char* filename,uint32_t inode_no,uint8_t file_type,struct dir_entry* p_de){
    ASSERT(strlen(filename)<=MAX_FILE_NAME_LEN);

    /* 初始化目录项 */
    memcpy(p_de->filename,filename,strlen(filename));
    p_de->i_no=inode_no;
    p_de->f_type=file_type;
}

/* 将目录项p_de写入父目录parent_dir中，io_buf由主调函数提供 */
bool sync_dir_entry(struct dir* parent_dir,struct dir_entry* p_de,void* io_buf){
    struct inode* dir_inode=parent_dir->inode;
    uint32_t dir_size=dir_inode->i_size;
    uint32_t dir_entry_size=cur_part->sb->dir_entry_size;

    ASSERT(dir_size%dir_entry_size==0);

    uint32_t dir_entrys_per_sec=(512/dir_entry_size);   // 每扇区最大目录数
    int32_t block_lba=-1;

    /* 将该目录的所有扇区地址（12个直接块+128个间接块）存入all_blocks */
    uint8_t block_idx=0;
    uint32_t all_blocks[140]={0};

    /* 将12个直接块存入all_blocks */
    while (block_idx<12){
    	all_blocks[block_idx]=dir_inode->i_sectors[block_idx];
    	++block_idx;
    }

    struct dir_entry* dir_e=(struct dir_entry*)io_buf;   // dir_e用来遍历io_buf
    int32_t block_bitmap_idx=-1;

    /* 开始遍历所有块以寻找目录项空位，若已有扇区中没有空位，
     * 在不超过文件大小的情况下上申请新扇区来存储新目录项 */
    block_idx=0;
    while (block_idx<140){   // 共140个块
    	block_bitmap_idx=-1;
    	if (all_blocks[block_idx]==0){
            block_lba=block_bitmap_alloc(cur_part);
            if (block_lba==-1){
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }

	    /* 每分配一个块就同步一次block_bitmap */
            block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
            ASSERT(block_bitmap_idx!=-1);
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

            block_bitmap_idx=-1;
            if (block_idx<12){   // 直接块
		        dir_inode->i_sectors[block_idx]=all_blocks[block_idx]=block_lba;
            }else if (block_idx==12){   // 尚未分配一级间接块表
            	dir_inode->i_sectors[12]=block_lba;
            	block_lba=-1;
            	block_lba=block_bitmap_alloc(cur_part);   // 在分配一个块作第0个间接块
            	if (block_lba==-1){
            	    block_bitmap_idx=dir_inode->i_sectors[12]-cur_part->sb->data_start_lba;
            	    bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);
            	    dir_inode->i_sectors[12]=0;
            	    printk("alloc block bitmap for sync_dir_entry failed\n");
            	    return false;
                }

                /* 每分配一个块就同步一次block_bitmap */
                block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx!=-1);
                bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

                all_blocks[12]=block_lba;
                /* 把新分配的第0个间接块写入一级间接块表 */
                ide_write(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
            }else {   // 若是间接块未分配
            	all_blocks[block_idx]=block_lba;
            	/* 把新分配的第(block_idx-12)个间接块地址写入一级间接块表 */
            	ide_write(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
            }

            /* 再将新目录项p_de写入新分配的间接块 */
            memset(io_buf,0,512);
            memcpy(io_buf,p_de,dir_entry_size);
            ide_write(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
            dir_inode->i_size+=dir_entry_size;
            return true;
    	}

        /* 若第block_idx块已存在，将其读入内存，然后在该块中查找空目录项 */
        ide_read(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
        /* 在扇区内查找空目录项 */
        uint8_t dir_entry_idx=0;
        while (dir_entry_idx<dir_entrys_per_sec){
            if ((dir_e+dir_entry_idx)->f_type==FT_UNKNOWN){
            	memcpy(dir_e+dir_entry_idx,p_de,dir_entry_size);
            	ide_write(cur_part->my_disk,all_blocks[block_idx],io_buf,1);

            	dir_inode->i_size+=dir_entry_size;
                return true;
            }
            ++dir_entry_idx;
    	}
    	++block_idx;
    }
    printk("directory is full!\n");
    return false;
}

/* 把分区part目录pdir中编号为inode_no的目录项删除 */
bool delete_dir_entry(struct partition* part,struct dir* pdir,uint32_t inode_no,void* io_buf){
    struct inode* dir_inode=pdir->inode;
    uint32_t block_idx=0,all_blocks[140]={0};
    /* 收集目录全部地址 */
    while (block_idx<12){
    	all_blocks[block_idx]=dir_inode->i_sectors[block_idx];
    	++block_idx;
    }
    if (dir_inode->i_sectors[12]){
    	ide_read(part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
    }

    /* 目录项在存储时保证不会跨扇区 */
    uint32_t dir_entry_size=part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec=(SECTOR_SIZE/dir_entry_size);
    struct dir_entry* dir_e=(struct dir_entry*)io_buf;
    struct dir_entry* dir_entry_found=NULL;
    uint8_t dir_entry_idx,dir_entry_cnt;
    bool is_dir_first_block=false;   // 目录的第一个块

    /* 遍历所有块，寻找目录项 */
    block_idx=0;
    while (block_idx<140){
    	is_dir_first_block=false;
    	if (all_blocks[block_idx]==0){
            ++block_idx;
            continue;
    	}
    	dir_entry_idx=dir_entry_cnt=0;
    	memset(io_buf,0,SECTOR_SIZE);
    	/* 读取扇区，获得目录项 */
    	ide_read(part->my_disk,all_blocks[block_idx],io_buf,1);

    	/* 遍历所有的目录项，统计该扇区的目录项数量及是否有待删除的目录项 */
    	while (dir_entry_idx<dir_entrys_per_sec){
            if ((dir_e+dir_entry_idx)->f_type!=FT_UNKNOWN){
                if (!strcmp((dir_e+dir_entry_idx)->filename,".")){
                    is_dir_first_block=true;
                }else if (strcmp((dir_e+dir_entry_idx)->filename,".") &&
                    strcmp((dir_e+dir_entry_idx)->filename,"..")){
                    ++dir_entry_cnt;   // 统计此扇区内的目录项个数，用来判断删除目录项后是否回收该扇区
                    if ((dir_e+dir_entry_idx)->i_no==inode_no){
                        ASSERT(dir_entry_found==NULL);
                        dir_entry_found=dir_e+dir_entry_idx;
                    /* 找到后也继续遍历，统计总共的目录项数 */
                    }
                }
            }
            ++dir_entry_idx;
        }

    	/* 若此扇区未找到该目录项，继续在下个扇区中找 */
    	if (dir_entry_found==NULL){
            ++block_idx;
            continue;
    	}	

    	/* 在此扇区中找到目录项后，清除该目录项并判断是否回收扇区，随后退出循环 */
    	ASSERT(dir_entry_cnt>=1);
    	/* 除目录第一个扇区外，若该扇区上只有目录项自己，则将整个扇区回收 */
    	if (dir_entry_cnt==1 && !is_dir_first_block){
            /* 1.在块位图中回收该块 */
            uint32_t block_bitmap_idx=all_blocks[block_idx]-part->sb->data_start_lba;
            bitmap_set(&part->block_bitmap,block_bitmap_idx,0);
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

            /* 2.将块地址从数组i_sectors或索引表中去掉 */
            if (block_idx<12){
        	    dir_inode->i_sectors[block_idx]=0;
            }else {   // 在一级 间接索引表中擦除该间接块地址
        /* 先判断一级间接索引表中间接块的数量，如果仅有这一个间接块，连同间接索引表所在块一同回收 */
            	uint32_t indirect_blocks=0;
            	uint32_t indirect_block_idx=12;
            	while (indirect_block_idx<140){
            	    if (all_blocks[indirect_block_idx]!=0){
            	        ++indirect_blocks;
                    }
                }
            	ASSERT(indirect_blocks>=1);   // 包括当前块

            	if (indirect_blocks>1){   // 还有其它间接块
            	    all_blocks[block_idx]=0;
            	    ide_write(part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
            	}else {
            	    /* 回收间接索引表所在的块 */
            	    block_bitmap_idx=dir_inode->i_sectors[12]-part->sb->data_start_lba;
            	    bitmap_set(&part->block_bitmap,block_bitmap_idx,0);
            	    bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

            	    /* 将间接索引表地址清0 */
            	    dir_inode->i_sectors[12]=0;
            	}
            }
    	}else {   // 仅将该目录清空
            memset(dir_entry_found,0,dir_entry_size);
            ide_write(part->my_disk,all_blocks[block_idx],io_buf,1);
    	}

    	/* 更新inode并同步到硬盘 */
    	ASSERT(dir_inode->i_size>=dir_entry_size);
    	dir_inode->i_size-=dir_entry_size;
    	memset(io_buf,0,SECTOR_SIZE*2);
    	inode_sync(part,dir_inode,io_buf);

    	return true;
    }
    /* 所有块中未找到则返回false，若出现这种情况应该是search_file出错 */
    return false;
}

/* 读取目录，成功返回一个目录项，失败返回NULL */
struct dir_entry* dir_read(struct dir* dir){
    struct dir_entry* dir_e=(struct dir_entry*)dir->dir_buf; //哈哈 目录项里面自己有缓冲区
    struct inode* dir_inode=dir->inode;
    uint32_t all_blocks[140]={0},block_cnt=12;
    uint32_t block_idx=0,dir_entry_idx=0;
    while (block_idx<12){
        all_blocks[block_idx]=dir_inode->i_sectors[block_idx];
        ++block_idx;
    }
    if (dir_inode->i_sectors[12]!=0){
        ide_read(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);
        block_cnt=140;
    }
    block_idx=0;

    uint32_t cur_dir_entry_pos=0;   // 当前目录项的偏移
    uint32_t dir_entry_size=cur_part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec=SECTOR_SIZE/dir_entry_size;
    /* 在目录大小内遍历 */
    while (dir->dir_pos<dir_inode->i_size){
        if(dir->dir_pos>=dir_inode->i_size){
            return NULL;
	    }
        if(all_blocks[block_idx]==0){
            ++block_idx;
            continue;
        }
        memset(dir_e,0,SECTOR_SIZE);
        ide_read(cur_part->my_disk,all_blocks[block_idx],dir_e,1);
        dir_entry_idx=0;
	/* 遍历扇区内所有目录项 */
        while (dir_entry_idx<dir_entrys_per_sec){
            if ((dir_e + dir_entry_idx)->f_type){
                /* 已经遍历过 */
                if (cur_dir_entry_pos<dir->dir_pos){
                    cur_dir_entry_pos+=dir_entry_size;
                    ++dir_entry_idx;
                    continue;
                }
                ASSERT(cur_dir_entry_pos==dir->dir_pos);
                dir->dir_pos+=dir_entry_size;   // 返回目录地址
                return dir_e+dir_entry_idx;
            }
            ++dir_entry_idx;
        }
        ++block_idx;
    }
    return NULL;
}

/* 判断目录是否为空 */
bool dir_is_empty(struct dir* dir){
    struct inode* dir_inode=dir->inode;
    /* 若目录下只有'.'和'..'这两个目录项，则视为空 */
    return (dir_inode->i_size==cur_part->sb->dir_entry_size*2);
}

/* 在父目录parent_dir中删除child_dir */
int32_t dir_remove(struct dir* parent_dir,struct dir* child_dir){
    struct inode* child_dir_inode=child_dir->inode;
    /* 空目录中只在inode->i_sectors[0]中有扇区，其它扇区都应该为空 */
    int32_t block_idx=1;
    while (block_idx<13){
        ASSERT(child_dir_inode->i_sectors[block_idx]==0);
        ++block_idx;
    }
    void* io_buf=sys_malloc(SECTOR_SIZE*2);
    if (io_buf==NULL){
        printk("dir_remove: malloc for io_buf failed!\n");
        return -1;
    }

    /* 在父目录parent_dir中删除子目录child_dir对应的目录项 */
    delete_dir_entry(cur_part,parent_dir,child_dir_inode->i_no,io_buf);

    /* 回收inode中i_sectors中所占用的扇区，并同步inode_bitmap和block_bitmap */
    inode_release(cur_part,child_dir_inode->i_no);
    sys_free(io_buf);
    return 0;
}

