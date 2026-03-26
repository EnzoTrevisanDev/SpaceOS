ENTRY(init)
SECTIONS
{
    . = 1M;       /* carrega tudo a partir de 1MB de RAM */

    .text ALIGN(4K) : {
        *(.multiboot)   /* cabecalho Multiboot PRIMEIRO — o GRUB procura aqui */
        *(.text)        /* resto do codigo executavel */
    }

    .rodata ALIGN(4K) : {
        *(.rodata)      /* strings constantes: seus textos com escrever("...") */
    }

    .data ALIGN(4K) : {
        *(.data)        /* variaveis globais que tem valor inicial */
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)         /* stack e variaveis sem valor inicial */
    }
}
