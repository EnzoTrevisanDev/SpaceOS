;
;
;
[ BITS 32 ]
[ GLOBAL init ]
[ EXTERN kernel_main ]

; --- MuiltBoot ---
MULTIBOOT_MAGIC equ 0x1BADB002
MULTIBOOT_FLAGS equ 0x00
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
    dd  MULTIBOOT_MAGIC
    dd  MULTIBOOT_FLAGS
    dd  MULTIBOOT_CHECKSUM

; STACK
section .bss
    align 16
    stack_bottom:
        resb 16384 ; 16KB para a stack
    stack_top:


; codigo de entrada
section .text
init:
    mov esp, stack_top ; esp -> topo da stack
    call kernel_main ; chama C
    cli

.stuck:
    hlt
    jmp .stuck

section .note.GNU-stack noalloc noexec nowrite progbits