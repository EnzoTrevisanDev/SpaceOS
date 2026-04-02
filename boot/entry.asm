[BITS 32]
[GLOBAL init]
[EXTERN kernel_main]
[GLOBAL teclado_handler_asm]
[EXTERN teclado_handler]
[GLOBAL multiboot_ptr]
[GLOBAL page_fault_asm]
[EXTERN page_fault_handler]
[GLOBAL timer_handler_asm]
[EXTERN timer_handler_c]


; --- Multiboot ---
MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x00
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

; --- Stack ---
section .bss
    align 16
    stack_bottom:
        resb 16384
    stack_top:

section .data
    multiboot_ptr dd 0 ;armazena o ponteiro multiboot

; --- Todo o codigo executavel numa section .text so ---
section .text

init:
    mov esp, stack_top
    mov [multiboot_ptr], ebx ; salva ebs numa variavel global
    call kernel_main ; kernel_main(uiint32_t mboot_addr)
    cli
.stuck:
    hlt
    jmp .stuck

teclado_handler_asm:
    pushad
    call teclado_handler
    popad
    iret
page_fault_asm:
    pushad
    call page_fault_handler
    popad
    add esp, 4 ; remove o error code que o CPU empurrou
    iret
timer_handler_asm:
    pushad                      ; salva todos os registradores na stack atual
    mov eax, esp                ; eax = ESP atual (stack do processo em pausa)
    push eax                    ; passa como argumento para o handler C
    call timer_handler_c        ; chama o scheduler — retorna o novo ESP
    add esp, 4                  ; limpa o argumento da stack
    mov esp, eax                ; troca para a stack do novo processo
; a partir daqui estamos no contexto do novo processo
    popad                       ; restaura registradores do novo processo
    iret                        ; retorna para onde o novo processo estava


; --- Deve ficar por ultimo, fora do .text ---
section .note.GNU-stack noalloc noexec nowrite progbits