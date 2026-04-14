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
	  kernel/pkg/pkg_checksum.o \
	  kernel/pkg/pkg_db.o \
	  kernel/pkg/pkg_solver.o \
	  kernel/pkg/pkg_install.o \
	  kernel/drivers/pci.o \
	  kernel/drivers/acpi.o \
	  kernel/drivers/hda.o \
	  kernel/cpu/apic.o \
	  kernel/cpu/smp.o \
	  kernel/net/netbuf.o \
	  kernel/net/rtl8139.o \
	  kernel/net/ethernet.o \
	  kernel/net/arp.o \
	  kernel/net/ipv4.o \
	  kernel/net/udp.o \
	  kernel/net/dhcp.o \
	  kernel/net/dns.o \
	  kernel/net/tcp.o \
	  kernel/pkg/pkg_remote.o \
  kernel/fs/audit.o \
  kernel/disk/ahci.o \
  kernel/disk/dma.o \
  kernel/fs/spacefs.o \
  kernel/fs/journal.o \
  kernel/drivers/usb.o \
  kernel/drivers/usbhid.o \
  kernel/cpu/secboot.o \
  kernel/net/tls.o \
  kernel/net/wifi.o \

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

kernel/pkg/pkg_checksum.o: kernel/pkg/pkg_checksum.c
	$(CC) $(CFLAGS) -c kernel/pkg/pkg_checksum.c -o kernel/pkg/pkg_checksum.o

kernel/pkg/pkg_db.o: kernel/pkg/pkg_db.c
	$(CC) $(CFLAGS) -c kernel/pkg/pkg_db.c -o kernel/pkg/pkg_db.o

kernel/pkg/pkg_solver.o: kernel/pkg/pkg_solver.c
	$(CC) $(CFLAGS) -c kernel/pkg/pkg_solver.c -o kernel/pkg/pkg_solver.o

kernel/pkg/pkg_install.o: kernel/pkg/pkg_install.c
	$(CC) $(CFLAGS) -c kernel/pkg/pkg_install.c -o kernel/pkg/pkg_install.o

kernel/drivers/pci.o: kernel/drivers/pci.c
	$(CC) $(CFLAGS) -c kernel/drivers/pci.c -o kernel/drivers/pci.o

kernel/drivers/acpi.o: kernel/drivers/acpi.c
	$(CC) $(CFLAGS) -c kernel/drivers/acpi.c -o kernel/drivers/acpi.o

kernel/drivers/hda.o: kernel/drivers/hda.c
	$(CC) $(CFLAGS) -c kernel/drivers/hda.c -o kernel/drivers/hda.o

kernel/cpu/apic.o: kernel/cpu/apic.c
	$(CC) $(CFLAGS) -c kernel/cpu/apic.c -o kernel/cpu/apic.o

kernel/cpu/smp.o: kernel/cpu/smp.c
	$(CC) $(CFLAGS) -c kernel/cpu/smp.c -o kernel/cpu/smp.o

kernel/net/netbuf.o: kernel/net/netbuf.c
	$(CC) $(CFLAGS) -c kernel/net/netbuf.c -o kernel/net/netbuf.o

kernel/net/rtl8139.o: kernel/net/rtl8139.c
	$(CC) $(CFLAGS) -c kernel/net/rtl8139.c -o kernel/net/rtl8139.o

kernel/net/ethernet.o: kernel/net/ethernet.c
	$(CC) $(CFLAGS) -c kernel/net/ethernet.c -o kernel/net/ethernet.o

kernel/net/arp.o: kernel/net/arp.c
	$(CC) $(CFLAGS) -c kernel/net/arp.c -o kernel/net/arp.o

kernel/net/ipv4.o: kernel/net/ipv4.c
	$(CC) $(CFLAGS) -c kernel/net/ipv4.c -o kernel/net/ipv4.o

kernel/net/udp.o: kernel/net/udp.c
	$(CC) $(CFLAGS) -c kernel/net/udp.c -o kernel/net/udp.o

kernel/net/dhcp.o: kernel/net/dhcp.c
	$(CC) $(CFLAGS) -c kernel/net/dhcp.c -o kernel/net/dhcp.o

kernel/net/dns.o: kernel/net/dns.c
	$(CC) $(CFLAGS) -c kernel/net/dns.c -o kernel/net/dns.o

kernel/net/tcp.o: kernel/net/tcp.c
	$(CC) $(CFLAGS) -c kernel/net/tcp.c -o kernel/net/tcp.o

kernel/pkg/pkg_remote.o: kernel/pkg/pkg_remote.c
	$(CC) $(CFLAGS) -c kernel/pkg/pkg_remote.c -o kernel/pkg/pkg_remote.o

kernel/fs/audit.o: kernel/fs/audit.c
	$(CC) $(CFLAGS) -c kernel/fs/audit.c -o kernel/fs/audit.o

kernel/disk/ahci.o: kernel/disk/ahci.c
	$(CC) $(CFLAGS) -c kernel/disk/ahci.c -o kernel/disk/ahci.o

kernel/disk/dma.o: kernel/disk/dma.c
	$(CC) $(CFLAGS) -c kernel/disk/dma.c -o kernel/disk/dma.o

kernel/fs/spacefs.o: kernel/fs/spacefs.c
	$(CC) $(CFLAGS) -c kernel/fs/spacefs.c -o kernel/fs/spacefs.o

kernel/fs/journal.o: kernel/fs/journal.c
	$(CC) $(CFLAGS) -c kernel/fs/journal.c -o kernel/fs/journal.o

kernel/drivers/usb.o: kernel/drivers/usb.c
	$(CC) $(CFLAGS) -c kernel/drivers/usb.c -o kernel/drivers/usb.o

kernel/drivers/usbhid.o: kernel/drivers/usbhid.c
	$(CC) $(CFLAGS) -c kernel/drivers/usbhid.c -o kernel/drivers/usbhid.o

kernel/cpu/secboot.o: kernel/cpu/secboot.c
	$(CC) $(CFLAGS) -c kernel/cpu/secboot.c -o kernel/cpu/secboot.o

kernel/net/tls.o: kernel/net/tls.c
	$(CC) $(CFLAGS) -c kernel/net/tls.c -o kernel/net/tls.o

kernel/net/wifi.o: kernel/net/wifi.c
	$(CC) $(CFLAGS) -c kernel/net/wifi.c -o kernel/net/wifi.o

SpaceOS.bin: $(OBJ)
	$(LD) $(LFLAGS) -o SpaceOS.bin $(OBJ)

SpaceOS.iso: SpaceOS.bin
	cp SpaceOS.bin iso/boot/SpaceOS.bin
	grub-mkrescue -o SpaceOS.iso iso/ 2>/dev/null

disco2.img:
	qemu-img create disco2.img 32M

rodar: disco2.img
	qemu-system-x86_64 -cdrom SpaceOS.iso -m 64M -k pt-br -boot d \
		-drive file=disco.img,format=raw,if=ide,index=0 \
		-drive file=disco2.img,format=raw,if=ide,index=1 \
		-device rtl8139,netdev=net0 \
		-netdev user,id=net0 \
		-device intel-hda -device hda-duplex \
		-smp 2 -M pc

rodar-simples:
	qemu-system-x86_64 -cdrom SpaceOS.iso -m 64M -k pt-br -boot d \
		-drive file=disco.img,format=raw,if=ide,index=0

rodar-pcap:
	qemu-system-x86_64 -cdrom SpaceOS.iso -m 64M -k pt-br -boot d \
		-drive file=disco.img,format=raw,if=ide \
		-device rtl8139,netdev=net0 \
		-netdev user,id=net0,hostfwd=tcp::8080-:80 \
		-object filter-dump,id=f0,netdev=net0,file=/tmp/spaceos.pcap \
		-smp 2 -M pc

limpar:
	rm -f boot/*.o \
	kernel/*.o \
	kernel/cpu/*.o \
	kernel/mem/*.o \
	kernel/proc/*.o \
	kernel/shell/*.o \
	kernel/disk/*.o \
	kernel/fs/*.o \
	kernel/pkg/*.o \
	kernel/drivers/*.o \
	kernel/net/*.o \
	SpaceOS.bin SpaceOS.iso

.PHONY: all rodar limpar