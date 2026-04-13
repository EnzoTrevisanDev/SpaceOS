CC	 = gcc
NASM   = nasm
LD	 = ld

CFLAGS = -m32 -ffreestanding -fno-builtin -fno-pic -nostdlib -O2 -Wall
LFLAGS = -m elf_i386 -T boot/linker.ld

OBJ = boot/entry.o \
	  kernel/main.o \
	  kernel/cpu/gdt.o \
	  kernel/cpu/idt.o \
	  kernel/cpu/pic.o \
	  kernel/cpu/timer.o \
	  kernel/cpu/syscall.o \
	  kernel/teclado.o \
	  kernel/shell/shell.o \
	  kernel/shell/comandos.o \
	  kernel/mem/pmm.o \
	  kernel/mem/paging.o \
	  kernel/mem/paging_fault.o \
	  kernel/mem/kheap.o \
	  kernel/mem/slab.o \
	  kernel/proc/processo.o \
	  kernel/proc/sched.o \
	  kernel/proc/identidade.o \
	  kernel/disk/ata.o \
	  kernel/disk/kstring.o \
	  kernel/fs/fat32.o \
	  kernel/fs/vfs.o \

all: SpaceOS.iso

boot/entry.o: boot/entry.asm
	$(NASM) -f elf32 boot/entry.asm -o boot/entry.o

kernel/main.o: kernel/main.c
	$(CC) $(CFLAGS) -c kernel/main.c -o kernel/main.o

kernel/cpu/gdt.o: kernel/cpu/gdt.c
	$(CC) $(CFLAGS) -c kernel/cpu/gdt.c -o kernel/cpu/gdt.o

kernel/cpu/idt.o: kernel/cpu/idt.c
	$(CC) $(CFLAGS) -c kernel/cpu/idt.c -o kernel/cpu/idt.o

kernel/cpu/pic.o: kernel/cpu/pic.c
	$(CC) $(CFLAGS) -c kernel/cpu/pic.c -o kernel/cpu/pic.o

kernel/cpu/timer.o: kernel/cpu/timer.c
	$(CC) $(CFLAGS) -c kernel/cpu/timer.c -o kernel/cpu/timer.o

kernel/cpu/syscall.o: kernel/cpu/syscall.c
	$(CC) $(CFLAGS) -c kernel/cpu/syscall.c -o kernel/cpu/syscall.o

kernel/teclado.o: kernel/teclado.c
	$(CC) $(CFLAGS) -c kernel/teclado.c -o kernel/teclado.o

kernel/shell/shell.o: kernel/shell/shell.c
	$(CC) $(CFLAGS) -c kernel/shell/shell.c -o kernel/shell/shell.o

kernel/shell/comandos.o: kernel/shell/comandos.c
	$(CC) $(CFLAGS) -c kernel/shell/comandos.c -o kernel/shell/comandos.o

kernel/mem/pmm.o: kernel/mem/pmm.c
	$(CC) $(CFLAGS) -c kernel/mem/pmm.c -o kernel/mem/pmm.o

kernel/mem/paging.o: kernel/mem/paging.c
	$(CC) $(CFLAGS) -c kernel/mem/paging.c -o kernel/mem/paging.o

kernel/mem/paging_fault.o: kernel/mem/paging_fault.c
	$(CC) $(CFLAGS) -c kernel/mem/paging_fault.c -o kernel/mem/paging_fault.o

kernel/mem/kheap.o: kernel/mem/kheap.c
	$(CC) $(CFLAGS) -c kernel/mem/kheap.c -o kernel/mem/kheap.o

kernel/mem/slab.o: kernel/mem/slab.c
	$(CC) $(CFLAGS) -c kernel/mem/slab.c -o kernel/mem/slab.o

kernel/proc/processo.o: kernel/proc/processo.c
	$(CC) $(CFLAGS) -c kernel/proc/processo.c -o kernel/proc/processo.o

kernel/proc/sched.o: kernel/proc/sched.c
	$(CC) $(CFLAGS) -c kernel/proc/sched.c -o kernel/proc/sched.o

kernel/proc/identidade.o: kernel/proc/identidade.c
	$(CC) $(CFLAGS) -c kernel/proc/identidade.c -o kernel/proc/identidade.o

kernel/disk/ata.o: kernel/disk/ata.c
	$(CC) $(CFLAGS) -c kernel/disk/ata.c -o kernel/disk/ata.o

kernel/disk/kstring.o: kernel/disk/kstring.c
	$(CC) $(CFLAGS) -c kernel/disk/kstring.c -o kernel/disk/kstring.o

kernel/fs/fat32.o: kernel/fs/fat32.c
	$(CC) $(CFLAGS) -c kernel/fs/fat32.c -o kernel/fs/fat32.o

kernel/fs/vfs.o: kernel/fs/vfs.c
	$(CC) $(CFLAGS) -c kernel/fs/vfs.c -o kernel/fs/vfs.o

SpaceOS.bin: $(OBJ)
	$(LD) $(LFLAGS) -o SpaceOS.bin $(OBJ)

SpaceOS.iso: SpaceOS.bin
	cp SpaceOS.bin iso/boot/SpaceOS.bin
	grub-mkrescue -o SpaceOS.iso iso/ 2>/dev/null

rodar:
	qemu-system-x86_64 -cdrom SpaceOS.iso -m 64M -k pt-br -boot d \
		-drive file=disco.img,format=raw,if=ide

limpar:
	rm -f boot/*.o \
	kernel/*.o \
	kernel/cpu/*.o \
	kernel/mem/*.o \
	kernel/proc/*.o \
	kernel/shell/*.o \
	kernel/disk/*.o \
	kernel/fs/*.o \
	SpaceOS.bin SpaceOS.iso

.PHONY: all rodar limpar