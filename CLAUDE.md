# SpaceOS — Roadmap de Desenvolvimento

> Cole este arquivo no início de cada novo chat para continuar de onde parou.
> Atualize os checkboxes conforme for concluindo cada item.

## Contexto do Projeto

**OS:** SpaceOS — construído do zero em C e Assembly  
**Ambiente:** Linux Mint | Pasta: `~/Documents/SpaceOS/`  
**Versão atual:** v0.6 Urano (SPK Package Manager)  
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
- [ ] Driver AHCI — SATA moderno via PCI
- [ ] DMA — Direct Memory Access (disco escreve direto na RAM)

### Filesystem
- [x] Parser FAT32 — ler tabela FAT, diretório raiz, clusters
- [x] VFS — Virtual Filesystem (abstração sobre qualquer filesystem)
- [ ] SpaceFS — filesystem próprio do SpaceOS
- [ ] Journaling — proteção contra corrupção em queda de energia
- [ ] Audit log imutável — append-only com hash encadeado

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

## v0.6 Urano — SPK Package Manager

### Formato do Pacote
- [ ] Estrutura `.spk` — header + arquivos + assinatura criptográfica
- [ ] Manifest do pacote — nome, versão, autor, deps, destinos

### Comandos SPK
- [ ] `spk install pacote.spk`
- [ ] `spk remove nome`
- [ ] `spk list`
- [ ] `spk info nome`
- [ ] `spk search` + repositório remoto
- [ ] Resolução de dependências — ordenação topológica
- [ ] Atualizações atômicas — rollback automático se falhar

---

## v0.7 Netuno — Drivers de Hardware

### Infraestrutura
- [ ] Enumeração PCI — varrer barramento, detectar dispositivos
- [ ] ACPI — tabelas de hardware (DSDT, MADT), shutdown, sleep

### Drivers Essenciais
- [ ] USB XHCI/EHCI — host controller (spec de 900 páginas)
- [ ] HID — teclado e mouse USB (em cima do USB)
- [ ] Driver de áudio HDA — Intel High Definition Audio
- [ ] SMP — múltiplos núcleos (APIC, SIPI, locks thread-safe)
- [ ] Boot verificado em BIOS antigo — Secure Boot sem UEFI

---

## v0.8 Galáxia — Rede e Conectividade

### Stack de Rede Completa
- [ ] Driver Ethernet (RTL8139) — frames via PCI e DMA
- [ ] ARP + IPv4 — resolução MAC, roteamento, checksum
- [ ] TCP — handshake, ACKs, retransmissão, controle de fluxo
- [ ] UDP + DNS + DHCP — comunicação rápida e resolução de nomes
- [ ] TLS — comunicação segura (necessário para repositório SPK)
- [ ] WiFi 802.11 — driver por chipset + WPA2/3

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
- [ ] GitHub Sponsors + Open Collective
- [ ] SpaceOS Enterprise Edition
- [ ] Certificações SpaceOS
- [ ] SPK Store — loja de pacotes premium

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
| v0.6 | Urano | 🔄 Em progresso |
| v0.7 | Netuno | ⬜ Pendente |
| v0.8 | Galáxia | ⬜ Pendente |
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