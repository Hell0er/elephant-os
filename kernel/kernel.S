[bits 32]
%define ERROR_CODE nop
%define ZERO push 0

extern idt_table

section .data
global intr_entry_table
intr_entry_table:

%macro VECTOR 2
section .text
intr%1entry:                ;每个中断处理程序都要压入中断向量号，一个中断类型一个中断处理程序
   %2
;以下是保存上下文环境
   push ds
   push es
   push fs
   push gs
   pushad                   ;pushad压入32位寄存器，压栈顺序为eax,ecx,edx,ebx,esp,ebp,esi,edi
   
   ;如果是从片上进入的中断，除了往从片上发送EOI外，还要往主片上发送EOI
   mov al,0x20              ;中断结束命令EOI
   out 0xa0,al              ;向从片发送
   out 0x20,al              ;向主片发送
   
   push %1                  ;不管idt_table中的目标程序是否需要参数，都一律压入中断向量号，调试时很方便
   call [idt_table+%1*4]    ;调用idt_table中的C版本中断处理函数
   jmp intr_exit
   
section .data
   dd intr%1entry           ;存储各个中断入口程序的地址，形成intr_entry_table数组
%endmacro

section .text
global intr_exit
intr_exit:
;以下是恢复上下文环境
   add esp,4                ;跳过中断号
   popad
   pop gs
   pop fs
   pop es
   pop ds
   add esp,4                ;跳过error_code
   iretd

VECTOR 0x0 ,ZERO
VECTOR 0x1 ,ZERO
VECTOR 0x2 ,ZERO
VECTOR 0x3 ,ZERO
VECTOR 0x4 ,ZERO
VECTOR 0x5 ,ZERO
VECTOR 0x6 ,ZERO
VECTOR 0x7 ,ZERO
VECTOR 0x8 ,ERROR_CODE
VECTOR 0x9 ,ZERO
VECTOR 0xA ,ERROR_CODE
VECTOR 0xB ,ERROR_CODE
VECTOR 0xC ,ERROR_CODE
VECTOR 0xD ,ERROR_CODE
VECTOR 0xE ,ERROR_CODE
VECTOR 0xF ,ZERO
VECTOR 0x10 ,ZERO
VECTOR 0x11 ,ERROR_CODE
VECTOR 0x12 ,ZERO
VECTOR 0x13 ,ZERO
VECTOR 0x14 ,ZERO
VECTOR 0x15 ,ZERO
VECTOR 0x16 ,ZERO
VECTOR 0x17 ,ZERO
VECTOR 0x18 ,ZERO
VECTOR 0x19 ,ZERO
VECTOR 0x1A ,ZERO
VECTOR 0x1B ,ZERO
VECTOR 0x1C ,ZERO
VECTOR 0x1D ,ZERO
VECTOR 0x1E ,ERROR_CODE
VECTOR 0x1F ,ZERO
VECTOR 0x20 ,ZERO   ; 时钟中断
VECTOR 0x21 ,ZERO   ; 键盘中断
VECTOR 0x22 ,ZERO
VECTOR 0x23 ,ZERO
VECTOR 0x24 ,ZERO
VECTOR 0x25 ,ZERO
VECTOR 0x26 ,ZERO
VECTOR 0x27 ,ZERO
VECTOR 0x28 ,ZERO
VECTOR 0x29 ,ZERO
VECTOR 0x2A ,ZERO
VECTOR 0x2B ,ZERO
VECTOR 0x2C ,ZERO
VECTOR 0x2D ,ZERO
VECTOR 0x2E ,ZERO
VECTOR 0x2F ,ZERO

;;;;;;;;;;;;;;; 0x80号中断 ;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
   ;1.保存上下文环境
   push 0   ; ERROR_CODE
   push ds
   push es
   push fs
   push gs
   pushad   ; PUSHAD压入32位寄存器


   push 0x80 ; VEC_NO

   ; 2.为系统调用子功能传入参数
   push edx
   push ecx
   push ebx

   ; 3.调用子功能处理函数
   call [syscall_table+eax*4]
   add esp,12   ; 跨过上面3个参数

   ; 4.将call调用后的返回值存入当前内核栈中eax的位置
   mov [esp+8*4],eax
   jmp intr_exit   ; intr_exit返回，恢复上下文

