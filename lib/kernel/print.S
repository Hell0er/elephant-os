TI_GDT equ 0
RPL0   equ 0
SELECTOR_VIDEO equ (0x0003<<3)+TI_GDT+RPL0

section .data
put_int_buffer dq 0        ;定义8字节缓冲区用于数字到字符的转换

[bits 32]
section .text
   ;-------------------- put_char ---------------------
   ;功能描述：把栈中的1个字符写入光标所在处
   ;---------------------------------------------------
global put_char:
put_char:
   pushad                  ;备份32位寄存器环境，需要保证gs为正确的视频段选择子，保险起见，每次打印时都为gs赋值
   mov ax,SELECTOR_VIDEO
   mov gs,ax
   
   ;获取当前光标位置
   ;先获取高8位
   mov dx,0x03d4
   mov al,0x0e
   out dx,al
   mov dx,0x03d5
   in al,dx
   mov ah,al
   ;再获取低8位
   mov dx,0x03d4
   mov al,0x0f
   out dx,al
   mov dx,0x03d5
   in al,dx
   
   ;将光标存入bx
   mov bx,ax
   mov ecx,[esp+36]        ;pushad压入4*8=32B，加上主调函数4B的返回地址，故esp+36B
   cmp cl,0xd              ;CR是0x0d，LF是0x0a
   jz .is_carriage_return
   cmp cl,0xa
   jz .is_line_feed
  
   cmp cl,0x8              ;BS(backspace)的ASCII码是8
   jz .is_backspace
   jmp .put_other
   
.is_backspace:
   dec bx
   shl bx,1                ;光标左移1位等于乘2，表示光标对应显存中的偏移字节
   mov byte [gs:bx],0x20   ;将待删除的字节补为0或空格皆可
   inc bx
   mov byte [gs:bx],0x07
   shr bx,1
   jmp set_cursor
   
.put_other:
   shl bx,1                ;光标位置用2字节表示，将光标值*2表示对应显存中的偏移字节
   mov [gs:bx],cl          ;ASCII字符本身
   inc bx
   mov byte [gs:bx],0x07   ;字符属性
   shr bx,1                ;恢复老光标值
   inc bx                  ;下一个光标值
   cmp bx,2000             
   jl set_cursor          ;若光标值小于2000，表示未写到显存的最后，则去设置新的光标值，若超出屏幕字符大小（2000），则换行处理

.is_line_feed:             ;换行符LF(\n)
.is_carriage_return:       ;回车符CR(\r)
   ;如果是CR(\r)，只要把光标移到行首就行了
   xor dx,dx               ;dx是被除数的高16位，清0
   mov ax,bx               ;ax是被除数的低16位
   mov si,80
   div si
   sub bx,dx               ;光标值取整

.is_carriage_return_end:
   add bx,80
   cmp bx,2000
.is_line_feed_end:         ;若是LF(\n)，将光标移+80即可
   jl set_cursor
         
   ;屏幕行范围是0~24，滚屏的原理是将屏幕第1~24行搬运到第0~23行，再将第23行用空格填充
.roll_screen:              ;若超出屏幕大小，滚屏
   cld
   mov ecx,960             ;2000-80=1920个字符，共1920*2=3840B，一次搬4B，共3840/4=960次
   mov esi,0xc00b80a0      ;第1行行首
   mov edi,0xc00b8000      ;第0行行首
   rep movsd

   ;将最后一行填充为空白
   mov ebx,3840
   mov ecx,80

.cls:
   mov word [gs:ebx],0x0720;0x0720是黑底白字的空格键
   add ebx,2
   loop .cls
   mov bx,1920             ;将光标值重置为1920，最后一行首字符

global set_cursor:
set_cursor:
   ;将光标设为bx值
   ;1.先设置高8位
   mov dx,0x03d4           ;索引寄存器
   mov al,0x0e             ;光标位置高8位
   out dx,al
   mov dx,0x03d5           ;通过读写数据端口0x3d5来获得或设置光标位置
   mov al,bh
   out dx,al
   
   ;2.再设置低8位
   mov dx,0x03d4
   mov al,0x0f
   out dx,al
   mov dx,0x03d5
   mov al,bl
   out dx,al
.put_char_done:
   popad
   ret
   
   ;--------------------- put_str -----------------------
   ;-- 功能描述：通过put_char来打印以0字符结尾的字符串 --
   ;-----------------------------------------------------
global put_str:
put_str:
   push ebx
   push ecx
   xor ecx,ecx
   mov ebx,[esp+12]
.put_char_loop:
   mov cl,[ebx]
   cmp cl,0
   jz .str_over
   push ecx
   call put_char
   add esp,4
   inc ebx
   jmp .put_char_loop
.str_over:
   pop ecx
   pop ebx
   ret

global cls_screen
cls_screen:
   pushad
   ;;;;;;;;;;
        ; 由于用户程序的cpl为3，显存段的dpl为0，故用于显存段的选择子gs在低于自己特权的环境中为0，
        ; 由于用户程序再次进入中断后，gs为0，故直接在put_str中每次都为gs赋值
   mov ax,SELECTOR_VIDEO
   mov gs,ax
   
   mov ebx,0
   mov ecx,80*25
.cls:
   mov word [gs:ebx],0x0720   ; 0x0720是黑底白字的空格键
   add ebx,2
   loop .cls
   mov ebx,0

.set_cursor:
   ;;;;;;;;;; 1.先设置高8位 ;;;;;;;;;;
   mov dx,0x03d4
   mov al,0x0e
   out dx,al
   mov dx,0x03d5
   mov al,bh
   out dx,al

   ;;;;;;;;;; 2.再设置低8位 ;;;;;;;;;;
   mov dx,0x03d4
   mov al,0x0f
   out dx,al
   mov dx,0x03d5
   mov al,bl
   out dx,al
   popad
   ret

   ;--------------------- put_int ---------------------
   ;--功能描述：将小端字节序的数字变成对应的ASCII后，倒置
   ;----------------------------------------------------
global put_int
put_int:
   pushad
   mov ebp,esp
   mov eax,[esp+36]        ;32B+4B返回地址
   mov edx,eax
   mov edi,7               ;put_int_buffer偏移量
   mov ecx,8               ;循环八次
   mov ebx,put_int_buffer
    
.16based_4bits:
   and edx,0x0000000F
   cmp edx,9             
   jg  .is_A2F             ;A～F的ASCII码
   add edx,'0'             ;0~9的ASCII码
   jmp .store    
    
.is_A2F:
   sub edx,10
   add edx,'A'             ;减去10等于A～F的字符序+'A'得ASCII码
    
.store:
   mov [ebx+edi],dl        ;此时dl中是数字对应字符的ASCII码
   dec edi                
   shr eax,4
   mov edx,eax
   loop .16based_4bits    
    
.ready_to_print:
   inc edi                 ;edi减退为-1
.skip_prefix_0:            ;跳过前导0
   cmp edi,8               ;edi偏移量为8的时候表示到了第9个字符
   je .full0               ;前导0有8个，说明全是0
    
.go_on_skip:   
   mov cl,[put_int_buffer+edi]
   inc edi                 ;下一个字符
   cmp cl,'0'
   je .skip_prefix_0       ;判断下一个字符是否为'\0'
   dec edi                 ;不是'\0'，edi减1恢复当前字符
   jmp .put_each_num
    
.full0:
   mov cl,'0'              ;全为0，输出一个0即可
.put_each_num:
   push ecx
   call put_char
   add esp,4
   inc edi                 ;使edi指向下一个字符
   mov cl,[put_int_buffer+edi]
   cmp edi,8
   jl .put_each_num
   popad 
   ret 
