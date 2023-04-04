#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

/* 将位图btmp初始化 */
void bitmap_init(struct bitmap* btmp){
    memset(btmp->bits,0,btmp->btmp_bytes_len);
}

/* 判断bit_idx位是否为1，若为1，则返回true，否则返回false */
bool bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx){
    uint32_t byte_idx=bit_idx/8;   // 向下取整用于索引数组下标
    uint32_t bit_off=bit_idx%8;   // 取余用于索引数组内的值
    return (btmp->bits[byte_idx]&(BITMAP_MASK<<bit_off));
}

/* 在位图中申请连续cnt个位，成功则返回其起始位下标，失败则返回-1 */
int bitmap_scan(struct bitmap* btmp,uint32_t cnt){
    uint32_t idx_byte=0;   // 用于记录空闲位所在的字节
    /* 逐字节比较，蛮力法 */
    while ((0xff==btmp->bits[idx_byte]) && (idx_byte<btmp->btmp_bytes_len)){
        // 1表示已分配，若为0xff，说明该字节内已无空闲位，向下一字节继续找
        idx_byte++;
    }

    ASSERT(idx_byte<btmp->btmp_bytes_len);
    if (idx_byte==btmp->btmp_bytes_len) {   // 若内存池中找不到可用空间
	    return -1;
    }

    /* 若在位图数组范围内的某字节内找到了空闲位
     * 在该字节内逐位比对，返回空闲位索引 */
    int idx_bit=0;
    /* 和btmp->bits[idx_byte]这个字节逐位比对 */
    while ((uint8_t)(BITMAP_MASK<<idx_bit)&(btmp->bits[idx_byte])){
        ++idx_bit;
    }

    int bit_idx_start=idx_byte*8+idx_bit;   // 空闲位在位图内的下标
    if (cnt==1){
	    return bit_idx_start;
    }
    uint32_t bit_left=(btmp->btmp_bytes_len*8-bit_idx_start);
    // 记录还有多少位可以判断
    uint32_t next_bit=bit_idx_start+1;
    uint32_t count=1;   // 用于记录找到的空闲位的个数

    bit_idx_start=-1;
    while (bit_left-->0){
        if (!bitmap_scan_test(btmp,next_bit)){
            ++count;
        }else{
            count=0;
        }
        if (count==cnt){
            bit_idx_start=next_bit-cnt+1;
            break;
        }
        ++next_bit;
    }
    return bit_idx_start;
}

/* 将位图btmp的bit_idx位设置为value */
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value){
    ASSERT((value==0) || (value==1));
    uint32_t byte_idx=bit_idx/8;
    uint32_t bit_off=bit_idx%8;

    /* 将1移位进行操作 */
    if (value){
	    btmp->bits[byte_idx]|=(BITMAP_MASK<<bit_off);
    }
    else{
	    btmp->bits[byte_idx]&=~(BITMAP_MASK<<bit_off);
    }
}
