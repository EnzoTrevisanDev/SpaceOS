ENTRY(init)
SECTIONS
{
    . = 1M;       /* carrega tudo a partir de 1MB de RAM */

    .text ALIGN(4K) : {
        _start = .;     /* inicio do kernel (para secboot) */
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
        _end = .;       /* fim do kernel                      */
    }
}
