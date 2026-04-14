#define VGA_BASE 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25
#define COLOR 0x0F /* 0=fundo preto, F=texto branco */

//imports
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/pic.h"
#include "teclado.h"
#include "shell/shell.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "mem/kheap.h"
#include "mem/slab.h"
#include "proc/processo.h"
#include "cpu/timer.h"
#include "proc/sched.h"
#include "cpu/syscall.h"
#include "disk/ata.h"
#include "fs/vfs.h"
#include "pkg/pkg_install.h"
#include "drivers/pci.h"
#include "drivers/acpi.h"
#include "drivers/hda.h"
#include "cpu/apic.h"
#include "cpu/smp.h"
#include "net/netbuf.h"
#include "net/rtl8139.h"
#include "net/ethernet.h"
#include "net/wifi.h"
#include "net/tls.h"
#include "fs/audit.h"
#include "disk/ahci.h"
#include "disk/dma.h"
#include "fs/spacefs.h"
#include "drivers/usb.h"
#include "drivers/usbhid.h"
#include "cpu/secboot.h"

//ponteiro para o buffer VGA
static unsigned short *vga = (unsigned short *)VGA_BASE;

//onde o cursor vai aparecer
static int col = 0;
static int lin = 0;

// limpa a tela
/* 80*25=2000 slots com espaco em branco */
void clean_screen() {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        vga[i] = ' ' | (COLOR << 8);
    }
    col = 0;
    lin = 0;
}

// escreve um caractere na tela
void write_char(char c) {
    //backspace
    if (c == '\b') {
        if (col > 0) {
            col--;
        } else if (lin > 0){
            lin--;
            col = VGA_COLS - 1;
        }

        int pos = lin * VGA_COLS + col;
        vga[pos] = (unsigned short)(' ' | (COLOR << 8));
        return;
    }

    if (c == '\n') {
        col = 0;
        lin++;
        return;
    }
    

    // calculate where the memory should find the VGA char should go
    int pos = lin * VGA_COLS + col;

    //  write: byte baixo = ASCII, byte alto = cor
    vga[pos] = (unsigned short)((unsigned char)c | (COLOR << 8));
    
    // move the cursor
    col++;
    if (col >= VGA_COLS) {
        col = 0;
        lin++;
    }
    
}

// -- WRINTING --- Percorre a string ate o \0 e chama escrever_char para cada letra
void write(const char *s) {
    while (*s != '\0') {
        write_char(*s);
        s++;
    }
}

// -- KERNEL MAIN ---
// call the assembly code to initialize the kernel and then call the C function
extern uint32_t multiboot_ptr;
void kernel_main(void) {
    clean_screen();

    write("SpaceOS v0.1 - Bare Metal\n");
    write("=================================\n");
    write("Teclado: inicializando...\n");

    gdt_init();           /* GDT PRIMEIRO — sempre */
    write("GDT ok\n");

    idt_init();          /* monta a tabela de interrupcoes */
    write("IDT ok\n");

    pic_init(); /* configura o chip de interrupcoes */
    write("PIC ok\n");

    teclado_init(); /* registra o handler do teclado */
    write("Teclado ok\n");
    
    
    /* Habilita interrupcoes na CPU — sem isso nada funciona */
    __asm__ volatile ("sti");

    pmm_init(multiboot_ptr);
    write("PMM ok\n");

    // registra o page fault handler na IDT - interrupcao 14
    extern void page_fault_asm(void);
    //extern void idt_set(uint8_t, uint32_t, uint16_t, uint8_t);
    idt_set(14, (uint32_t)page_fault_asm, 0x08, 0x8E);

    paging_init();
    write("Paging ok!");
    
    kheap_init();
    write("Heap ok\n");

    slab_init();
    write("Slab ok\n");

    proc_init();
    write("Proc ok\n");

    //timer - IRQ0 remapeado para interrupcao 32
    idt_set(32, (uint32_t)timer_handler_asm, 0x08, 0x8E);
    timer_init();
    write("timer ok!\n");

    sched_init();
    write("Sched ok\n");
    int disco = ata_init();
    if (disco == 0)
        write("Disco ok\n");
    else
        write("Sem disco\n");

    /* Disco 1 (slave) para SpaceFS */
    if (ata_init1() == 0)
        write("Disco1 ok\n");

    vfs_init();
    if (vfs_mounted())
        write("VFS: FAT32 montado\n");
    else
        write("VFS: sem filesystem\n");

    /* DMA para ATA */
    if (dma_init() == 0)
        write("DMA ok\n");

    /* AHCI (SATA) */
    if (ahci_init() == 0)
        write("AHCI ok\n");
    else
        write("AHCI: sem dispositivo\n");

    /* SpaceFS no disco 1 */
    if (spfs_mount() == 0)
        write("SpaceFS montado\n");

    pkg_init();
    write("PKG ok\n");

    /* Audit log */
    audit_init();

    /* v0.7 Netuno — Hardware Drivers */
    int n_pci = pci_init();
    if (n_pci > 0)
        write("PCI ok\n");

    if (acpi_init() == 0)
        write("ACPI ok\n");
    else
        write("ACPI: sem tabelas\n");

    apic_init();
    write("APIC ok\n");

    smp_init();

    if (hda_init() == 0)
        write("HDA ok\n");
    else
        write("HDA: sem dispositivo\n");

    /* v0.8 Galaxia — Stack de Rede */
    netbuf_init();
    write("Netbuf ok\n");

    if (rtl8139_init() == 0) {
        rtl8139_set_recv_cb(eth_recv);
        write("RTL8139 ok\n");
    } else {
        write("RTL8139: sem placa\n");
    }

    wifi_init();
    if (wifi_disponivel())
        write("WiFi: chipset detectado\n");

    /* USB EHCI */
    if (usb_init() == 0)
        write("USB ok\n");
    else
        write("USB: sem EHCI\n");

    /* Secure Boot — verificacao de integridade */
    secboot_init();

    syscall_init();
    write("Syscall ok\n");
    
    /* Entra na shell — nunca retorna */
    shell_init();

}
