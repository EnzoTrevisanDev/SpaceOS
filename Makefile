# Makefile — compila, linka e roda o MeuOS

CC     = gcc
NASM   = nasm
LD     = ld

# Flags para compilar C sem sistema operacional embaixo:
# -m32             → gera codigo 32-bit (x86)
# -ffreestanding   → nao assume que existe stdlib ou OS
# -fno-builtin     → nao usa funcoes builtin do gcc (ex: memcpy otimizado)
# -fno-pic         → sem position-independent code
# -nostdlib        → nao linka a stdlib do C
# -O2              → otimizacao nivel 2
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-pic -nostdlib -O2 -Wall

# Flags do linker:
# -m elf_i386      → formato ELF 32-bit
# -T boot/linker.ld → usa nosso script
LFLAGS = -m elf_i386 -T boot/linker.ld

OBJ = boot/entry.o kernel/main.o

# Alvo padrao: compila tudo e gera a ISO
all: SpaceOS.iso

# Compila o Assembly → gera arquivo objeto ELF 32-bit
boot/entry.o: boot/entry.asm
	$(NASM) -f elf32 boot/entry.asm -o boot/entry.o

# Compila o C → gera arquivo objeto ELF 32-bit
kernel/main.o: kernel/main.c
	$(CC) $(CFLAGS) -c kernel/main.c -o kernel/main.o

# Linka os dois objetos em um binario ELF final
SpaceOS.bin: $(OBJ)
	$(LD) $(LFLAGS) -o SpaceOS.bin $(OBJ)

# Empacota em ISO bootavel com GRUB
SpaceOS.iso: SpaceOS.bin
	cp SpaceOS.bin iso/boot/SpaceOS.bin
	grub-mkrescue -o SpaceOS.iso iso/ 2>/dev/null

# Roda no QEMU — PC virtual com 64MB de RAM
rodar:
	qemu-system-x86_64 -cdrom SpaceOS.iso -m 64M

# Limpa tudo compilado
limpar:
	rm -f boot/*.o kernel/*.o SpaceOS.bin SpaceOS.iso
.PHONY: all rodar limpar