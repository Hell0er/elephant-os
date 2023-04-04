BIN="prog_arg"
CFLAGS="-Wall -m32 -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
#LIB="../lib/ ../lib/kernel/ ../lib/user/ ../kernel/ ../device/ ../thread/ ../userprog ../fs/ ../shell/"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o start.o"
DD_IN=$BIN
DD_OUT="/home/zbb/bochs/hd60M.img"

nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a $OBJS start.o

gcc $CFLAGS -I "../lib/" -I "../lib/kernel/" -I "../lib/user/" -I "../kernel/" -I "../device/"  -I "../thread/" -I "../userprog/" -I "../fs/" -I "../shell/"  -o  $BIN".o" $BIN".c"
ld -m elf_i386 $BIN".o" simple_crt.a -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [ -f $BIN ];then
    dd if=./$DD_IN of=$DD_OUT bs=512 \
    count=$SEC_CNT seek=300 conv=notrunc
fi
