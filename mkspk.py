#!/usr/bin/env python3
"""
mkspk.py — Empacotador de pacotes .spk para o SpaceOS

Uso:
  python3 mkspk.py <diretorio/> <saida.spk> [opcoes]

Opcoes:
  --name    <nome>     Nome do pacote (max 8 chars para FAT32)
  --version <versao>   Versao (ex: 1.0.0)
  --author  <autor>    Nome do autor
  --dep     <dep>      Adicionar dependencia (pode repetir)

O diretorio de entrada deve conter um arquivo 'MANIFEST' com linhas:
  arquivo_local  /caminho/install/no/os

Exemplo de MANIFEST:
  bin/hello    /usr/bin/hello
  share/readme /usr/share/hello/readme

Exemplo:
  python3 mkspk.py hello_pkg/ hello.spk --name hello --version 1.0.0 --author enzo
"""

import struct
import sys
import os
import argparse
import zlib  # Para CRC32 — mas usamos djb2 igual ao kernel

def djb2(data: bytes) -> int:
    """Mesmo hash djb2 que o kernel usa em pkg_checksum.c"""
    h = 5381
    for b in data:
        h = ((h << 5) + h + b) & 0xFFFFFFFF
    return h

# Tamanhos das structs (devem bater com spk.h)
SPK_MAGIC   = 0x53504B21
SPK_VERSION = 1

def pack_str(s: str, size: int) -> bytes:
    """Codifica string em bytes com padding de zeros"""
    b = s.encode('ascii')[:size-1]
    return b + b'\x00' * (size - len(b))

def criar_spk(src_dir: str, out_path: str,
              name: str, version: str, author: str, deps: list):
    # Le MANIFEST
    manifest_path = os.path.join(src_dir, 'MANIFEST')
    if not os.path.exists(manifest_path):
        print(f"ERRO: MANIFEST nao encontrado em {src_dir}")
        sys.exit(1)

    files = []
    with open(manifest_path) as f:
        for linha in f:
            linha = linha.strip()
            if not linha or linha.startswith('#'):
                continue
            partes = linha.split()
            if len(partes) < 2:
                print(f"MANIFEST: linha invalida: {linha}")
                continue
            src_rel, install_path = partes[0], partes[1]
            src_abs = os.path.join(src_dir, src_rel)
            if not os.path.exists(src_abs):
                print(f"ERRO: arquivo nao encontrado: {src_abs}")
                sys.exit(1)
            files.append((src_abs, install_path))

    if len(files) > 8:
        print("ERRO: maximo 8 arquivos por pacote")
        sys.exit(1)

    if len(deps) > 8:
        print("ERRO: maximo 8 dependencias")
        sys.exit(1)

    # Le conteudo de todos os arquivos
    file_contents = []
    for src_abs, _ in files:
        with open(src_abs, 'rb') as f:
            file_contents.append(f.read())

    # Calcula offsets no payload
    offsets = []
    offset = 0
    for content in file_contents:
        offsets.append(offset)
        offset += len(content)
    total_payload = offset

    # Monta payload completo para checksum
    payload = b''.join(file_contents)
    checksum = djb2(payload)

    print(f"Pacote: {name} {version}")
    print(f"Arquivos: {len(files)}")
    print(f"Payload: {total_payload} bytes")
    print(f"Checksum: 0x{checksum:08X}")

    # --- Constroi o arquivo .spk ---

    # spk_header_t (392 bytes)
    header  = struct.pack('<I', SPK_MAGIC)       # magic
    header += struct.pack('<I', SPK_VERSION)      # version
    header += pack_str(name, 32)                  # name
    header += pack_str(version, 16)               # pkg_version
    header += pack_str(author, 32)                # author
    header += struct.pack('<I', len(deps))         # num_deps
    for i in range(8):
        dep = deps[i] if i < len(deps) else ''
        header += pack_str(dep, 32)               # deps[8][32]
    header += struct.pack('<I', len(files))        # num_files
    header += struct.pack('<I', total_payload)     # total_payload_size
    header += struct.pack('<I', checksum)          # checksum
    header += b'\x00' * 32                         # _pad

    assert len(header) == 392, f"Header size wrong: {len(header)}"

    # spk_file_record_t (73 bytes cada)
    records = b''
    for i, (src_abs, install_path) in enumerate(files):
        rec  = pack_str(install_path, 64)          # install_path
        rec += struct.pack('<I', len(file_contents[i]))  # size
        rec += struct.pack('<I', offsets[i])        # offset
        # executable: 1 se nao tem extensao conhecida de texto/dados
        exe = 1 if not install_path.endswith(('.txt', '.cfg', '.lst', '.md')) else 0
        rec += struct.pack('B', exe)               # executable
        assert len(rec) == 73, f"Record size wrong: {len(rec)}"
        records += rec
        print(f"  [{i}] {install_path} ({len(file_contents[i])} bytes, off={offsets[i]})")

    # Escreve o arquivo
    with open(out_path, 'wb') as f:
        f.write(header)
        f.write(records)
        f.write(payload)

    total_size = len(header) + len(records) + len(payload)
    print(f"\nCriado: {out_path} ({total_size} bytes)")

    if total_size > 64 * 1024:
        print("AVISO: pacote maior que 64KB — pode nao instalar no SpaceOS")

def main():
    p = argparse.ArgumentParser(description='Criador de pacotes .spk para SpaceOS')
    p.add_argument('src_dir',  help='Diretorio fonte com MANIFEST')
    p.add_argument('out_spk',  help='Arquivo .spk de saida')
    p.add_argument('--name',    required=True, help='Nome do pacote (max 8 chars)')
    p.add_argument('--version', default='1.0.0', help='Versao do pacote')
    p.add_argument('--author',  default='unknown', help='Autor do pacote')
    p.add_argument('--dep',     action='append', default=[], help='Dependencia (pode repetir)')
    args = p.parse_args()

    if len(args.name) > 8:
        print(f"AVISO: nome '{args.name}' tem mais de 8 chars — FAT32 pode truncar")

    criar_spk(args.src_dir, args.out_spk,
              args.name, args.version, args.author, args.dep)

if __name__ == '__main__':
    main()
