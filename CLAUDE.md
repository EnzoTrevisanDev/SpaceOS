# SpaceOS — Roadmap de Desenvolvimento

> Cole este arquivo no início de cada novo chat para continuar de onde parou.
> Atualize os checkboxes conforme for concluindo cada item.

## Contexto do Projeto

**OS:** SpaceOS — construído do zero em C e Assembly  
**Ambiente:** Linux Mint | Pasta: `~/Documents/SpaceOS/`  
**Versão atual:** v0.8 Galáxia (Stack de Rede completa)  
**QEMU:** `qemu-system-x86_64 -cdrom SpaceOS.iso -m 64M -k pt-br -boot d`  
**Com disco:** adicionar `-drive file=disco.img,format=raw,if=ide`

---

## Blocos Concluídos ✅

- **v0.1 Terra** — Boot, GDT, IDT, PIC, VGA, Teclado PS/2
- **v0.2 Lua** — Shell interativa, histórico, comandos builtin
- **v0.3 Marte** — PMM, Paging, kmalloc, Slab allocator
- **v0.4 Jupiter** — PCB, Scheduler, Timer 100Hz, Syscalls, Fork, Identidade criptográfica
- **Espaço kernel vs usuário** — Ring 0/3 separados, páginas kernel inacessíveis
- **v0.5 Saturno** — ATA PIO, FAT32, VFS, ls/cd/pwd/cat/mkdir/rm/cp/mv

---

## v0.5 Saturno — Filesystem e Storage

### Driver de Disco
- [x] Driver ATA PIO — leitura/escrita de setores via portas I/O
- [x] Driver AHCI — SATA moderno via PCI (BAR5, FIS H2D, command list)
- [x] DMA — Bus Master IDE DMA (PRDT, ATA DMA commands 0xC8/0xCA)

### Filesystem
- [x] Parser FAT32 — ler tabela FAT, diretório raiz, clusters
- [x] VFS — Virtual Filesystem (abstração sobre qualquer filesystem)
- [x] SpaceFS — filesystem próprio do SpaceOS (inode-based, disco 1)
- [x] Journaling — write-ahead journal com recovery (SpaceFS)
- [x] Audit log imutável — append-only com djb2 hash encadeado

### Comandos Shell de Arquivo
- [x] `ls` — listar diretório
- [x] `cd` — mudar diretório
- [x] `pwd` — mostrar diretório atual
- [x] `cat` — mostrar conteúdo de arquivo
- [x] `mkdir` — criar diretório
- [x] `rm` — apagar arquivo
- [x] `cp` — copiar arquivo
- [x] `mv` — mover/renomear arquivo

---

## v0.6 Urano — SPK Package Manager ✅

### Formato do Pacote
- [x] Estrutura `.spk` — header + arquivos + checksum djb2 (HMAC-SHA256 futuro)
- [x] Manifest do pacote — nome, versão, autor, deps, destinos (`/pkg/name.lst`)

### Comandos SPK
- [x] `spk install pacote.spk`
- [x] `spk remove nome`
- [x] `spk list`
- [x] `spk info nome`
- [x] `spk search` + repositório remoto (HTTP/1.0 sobre TCP, v0.8)
- [x] Resolução de dependências — ordenação topológica (BFS)
- [x] Atualizações atômicas — rollback automático se falhar

---

## v0.7 Netuno — Drivers de Hardware ✅

### Infraestrutura
- [x] Enumeração PCI — varrer barramento, detectar dispositivos
- [x] ACPI — tabelas de hardware (MADT), shutdown via porta 0x604

### Drivers Essenciais
- [x] USB XHCI/EHCI — host controller (EHCI, QH/qTD, SET_ADDRESS/SET_CONFIG)
- [x] HID — teclado USB boot protocol (SET_PROTOCOL, 8-byte reports)
- [x] Driver de áudio HDA — Intel High Definition Audio (init + CORB/RIRB)
- [x] SMP — múltiplos núcleos (APIC, SIPI, spinlocks thread-safe)
- [x] Boot verificado em BIOS antigo — Secure Boot via djb2 hash + /boot/kernel.hash

---

## v0.8 Galáxia — Rede e Conectividade ✅

### Stack de Rede Completa
- [x] Driver Ethernet (RTL8139) — frames via PCI, polling RX/TX
- [x] ARP + IPv4 — resolução MAC, roteamento, checksum RFC 1071
- [x] TCP — handshake 3-way, PSH+ACK, FIN (sem retransmissão — v1.x)
- [x] UDP + DNS + DHCP — DISCOVER→ACK, query DNS, binding table
- [x] TLS — ChaCha20-Poly1305 + HMAC-SHA256 + SpaceTLS PSK channel
- [x] WiFi 802.11 — detecção PCI de chipset (driver full ~10k linhas, v2.x)

---

## v1.0 Nebulosa — Interface Gráfica

### Do Texto para Pixels
- [ ] Framebuffer VESA/GOP — modo gráfico via GRUB
- [ ] Renderização de primitivas — pixel, linha, retângulo, círculo
- [ ] Renderização de fontes TTF — curvas de Bézier, rasterizador
- [ ] Driver de mouse — PS/2 IRQ12, cursor na tela
- [ ] Compositor de janelas — z-order, drag, resize, minimize
- [ ] SpaceUI — widget toolkit (botão, input, menu, scrollbar)
- [ ] Terminal gráfico — shell dentro de janela
- [ ] Aceleração GPU — compositing acelerado por hardware

### Monetização (ativar após este bloco)
- [x] GitHub Sponsors + Open Collective
- [x] SpaceOS Enterprise Edition
- [x] Certificações SpaceOS
- [x] SPK Store — loja de pacotes premium

---

## v1.x Cosmos — Segurança e Confiabilidade

### Segurança do Kernel
- [ ] KASLR — kernel em endereço aleatório a cada boot
- [ ] Sistema de permissões rwx — dono, grupo, UID/GID
- [ ] Criptografia de disco AES-256 — esquema próprio ou LUKS
- [ ] Memória imutável do kernel — páginas read-only com hash
- [ ] Stack canaries — detectar buffer overflow
- [ ] Zero-fill na desalocação — RAM liberada é zerada
- [ ] Syscalls restritas por processo — estilo seccomp
- [ ] Sandboxing nativo — processo nasce sem acesso ao FS
- [ ] KASLR agressivo — randomização de bibliotecas também
- [ ] Proteção Spectre/Meltdown — KPTI desde o início
- [ ] CFI — Control Flow Integrity

### Confiabilidade Nível CentOS
- [ ] Testes automatizados do kernel — suite no QEMU a cada commit
- [ ] Fuzzing de syscalls — inputs inválidos para encontrar bugs
- [ ] Suporte LTS 10 anos por versão major
- [ ] Portabilidade de hardware — testar em múltiplas máquinas
- [ ] Live patching — atualizar kernel sem reboot
- [ ] Self-healing — detectar drivers travados e reiniciar
- [ ] Panic log para flash — "caixa preta" do kernel

---

## Versões e Nomes

| Versão | Nome | Status |
|--------|------|--------|
| v0.1 | Terra | ✅ Concluído |
| v0.2 | Lua | ✅ Concluído |
| v0.3 | Marte | ✅ Concluído |
| v0.4 | Jupiter | ✅ Concluído |
| v0.5 | Saturno | ✅ Concluído |
| v0.6 | Urano | ✅ Concluído |
| v0.7 | Netuno | ✅ Concluído |
| v0.8 | Galáxia | ✅ Concluído |
| v1.0 | Nebulosa | ⬜ Pendente |
| v1.x | Cosmos | ⬜ Nunca termina |

---

## Pilares do SpaceOS

1. **Segurança** — Zero Trust nativo, memória imutável, supply chain verificada
2. **Transparência** — verificável matematicamente, audit log imutável
3. **Portabilidade** — roda em 256MB RAM, hardware de 2005+

**Posicionamento:** *"O único OS onde você controla tudo — e pode provar."*

---

## Estrutura de Arquivos Atual

```
~/Documents/SpaceOS/
├── boot/
│   ├── entry.asm        — multiboot, stack, handlers ASM
│   └── linker.ld        — layout de memória
├── kernel/
│   ├── main.c           — kernel_main
│   ├── multiboot.h      — estrutura multiboot
│   ├── teclado.c/h      — driver PS/2
│   ├── cpu/
│   │   ├── gdt.c/h      — segmentos Ring 0/3
│   │   ├── idt.c/h      — tabela de interrupções
│   │   ├── pic.c/h      — remapeamento IRQs
│   │   ├── timer.c/h    — PIT 100Hz
│   │   └── syscall.c/h  — int 0x80
│   ├── mem/
│   │   ├── pmm.c/h      — physical memory manager
│   │   ├── paging.c/h   — page directory + tables
│   │   ├── paging_fault.c — handler int 14
│   │   ├── kheap.c/h    — kmalloc/kfree
│   │   └── slab.c/h     — slab allocator
│   ├── proc/
│   │   ├── processo.c/h — PCB, fork, exec
│   │   ├── sched.c/h    — scheduler com prioridades
│   │   └── identidade.c/h — hash criptográfico
│   ├── disk/
│   │   └── ata.c/h      — driver ATA PIO ← AQUI ESTAMOS
│   └── shell/
│       ├── shell.c/h    — loop, buffer, scroll
│       └── comandos.c/h — tabela de comandos
├── iso/boot/grub/
│   └── grub.cfg
└── Makefile
```