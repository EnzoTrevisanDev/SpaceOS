#include "../shell/shell.h"
#include "../cpu/idt.h"
#include "../cpu/pic.h"

//handler do page fault - interrupcao 14
//cpu empurra um error code na stack antes de chamar esse handler
void page_fault_handler(void){
    //CR2 contem o endereco virtual que causou o fault
    uint32_t endereco;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(endereco));

    shell_write("");
    shell_writeln("*** PAGE FAULT ***");
    shell_write("Endereco: 0X");

    //imprime o endereco hex
    char hex[9];
    uint32_t v = endereco;
    for(int i = 7; i >= 0; i--){
        hex[i] = "0123456789ABCDEF"[v & 0xF];
        v >>= 4;
    }

    hex[8] = '\0';
    shell_write(hex);
    shell_writeln("");
    shell_writeln("Processo encerrado.");

    // trava - futuramente vai matar so o processo culpado!
    while(1) {}


}