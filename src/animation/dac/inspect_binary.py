"""Inspect an untouched original DAC DLL/PDB and emit an exact offline ABI profile.

Uses LLVM readers only. Never builds, patches, loads or deploys DAC.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import uuid
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--pdb', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--package-label', default='DAC NG Plus 3.2.4')
    parser.add_argument('--embedded-version', default='3.2.3')
    args = parser.parse_args()
    data = args.dll.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    timestamp = struct.unpack_from('<I', data, pe+8)[0]
    opt = pe+24
    size = struct.unpack_from('<I', data, opt+56)[0]
    section_count = struct.unpack_from('<H', data, pe+6)[0]
    section_table = opt+struct.unpack_from('<H',data,pe+20)[0]
    sections = [struct.unpack_from('<IIII',data,section_table+i*40+8) for i in range(section_count)]
    def file_offset(rva: int) -> int:
        for virtual_size, address, raw_size, raw in sections:
            if address <= rva < address+max(virtual_size,raw_size): return raw+rva-address
        raise ValueError(f'Unmapped RVA {rva:x}')
    debug_rva,debug_size = struct.unpack_from('<II',data,opt+112+6*8)
    guid=age=None
    for i in range(debug_size//28):
        _,_,_,_,kind,length,_,raw=struct.unpack_from('<IIHHIIII',data,file_offset(debug_rva)+i*28)
        if kind==2 and data[raw:raw+4]==b'RSDS': guid=data[raw+4:raw+20]; age=struct.unpack_from('<I',data,raw+20)[0]
    assert guid is not None
    summary = subprocess.check_output(['llvm-pdbutil','dump','--summary',str(args.pdb)],text=True,encoding='utf-8')
    pdb_guid = re.search(r'GUID:\s*\{([^}]+)\}', summary).group(1)
    pdb_age = int(re.search(r'Age:\s*(\d+)', summary).group(1))
    if uuid.UUID(pdb_guid).bytes_le != guid or pdb_age != age:
        raise SystemExit('PDB GUID/age does not match the DLL CodeView record')
    output_names = ['publics.txt', 'pdb-summary.txt', 'native-profile.json', 'CastTrigger-layout.txt', 'WeaponTrigger-layout.txt',
                    'ProcessEvent-disassembly.txt', 'PCProcessEvent-disassembly.txt', 'NPCProcessEvent-disassembly.txt']
    inputs = {args.dll.resolve(), args.pdb.resolve()}
    if any(path.resolve() in inputs for path in [args.header] + [args.evidence / name for name in output_names]):
        raise SystemExit('Inspection outputs must not overwrite either original input')
    args.evidence.mkdir(parents=True, exist_ok=True)
    (args.evidence/'pdb-summary.txt').write_text(summary,encoding='utf-8')
    symbols = subprocess.check_output(['llvm-pdbutil','dump','--publics',str(args.pdb)],text=True,encoding='utf-8')
    types = subprocess.check_output(['llvm-pdbutil','dump','--types',str(args.pdb)],text=True,encoding='utf-8')
    (args.evidence/'publics.txt').write_text(symbols,encoding='utf-8')
    def symbol(name: str) -> int:
        match = re.search(r'S_PUB32[^\n]*`\?'+re.escape(name)+r'@DynamicAnimationCasting@Loki[^\n]*`\n\s*flags = [^\n]*addr = (\d+):(\d+)',symbols)
        if not match: raise ValueError(name)
        segment, offset = map(int,match.groups())
        return sections[segment-1][1]+offset
    entries = {name:symbol(name) for name in ['ProcessEvent','PCProcessEvent','NPCProcessEvent','LoadTomls','CastTriggers','TomlLoaded']}
    layouts = {}
    for name in ['CastTrigger','WeaponTrigger']:
        records = re.findall(r'(?ms)^\s*(0x[0-9A-F]+) \| LF_STRUCTURE[^\n]*`Loki::AnimationCasting::'+name+r'`\n(.*?)(?=^\s*0x[0-9A-F]+ \|)',types)
        record = next((index,text) for index,text in records if 'forward ref' not in text)
        fields = re.search(r'field list: (0x[0-9A-F]+)',record[1]).group(1)
        field_record = re.search(r'(?ms)^\s*'+fields+r' \| LF_FIELDLIST[^\n]*\n(.*?)(?=^\s*0x[0-9A-F]+ \|)',types).group(1)
        members = {key:int(offset) for key,offset in re.findall(r'LF_MEMBER \[name = `([^`]+)`[^\n]*offset = (\d+)',field_record)}
        layouts[name]={'size':int(re.search(r'sizeof (\d+)',record[1]).group(1)), 'fields':members}
        (args.evidence/(name+'-layout.txt')).write_text(record[0]+'\n'+record[1]+'\n'+field_record,encoding='utf-8')


    prefixes={key:data[file_offset(entries[key]):file_offset(entries[key])+16] for key in ['ProcessEvent','LoadTomls']}
    proof = {'module_name': args.dll.name, 'package_label': args.package_label, 'embedded_version_label': args.embedded_version, 'dll_sha256':hashlib.sha256(data).hexdigest(),
        'pdb_sha256':hashlib.sha256(args.pdb.read_bytes()).hexdigest(), 'timestamp':timestamp, 'image_size':size,
        'codeview_guid_bytes':guid.hex(), 'codeview_age':age, 'rvas':entries, 'prefixes':{k:v.hex() for k,v in prefixes.items()},'layouts':layouts }
    (args.evidence/'native-profile.json').write_text(json.dumps(proof,indent=2),encoding='utf-8')
    lines=['// Generated by src/animation/dac/inspect_binary.py from the original paired DLL/PDB.','#pragma once','#include <array>','#include <cstdint>','#include <cstddef>','PLUGIN_NAMESPACE_BEGIN','namespace dac::profile','{']
    lines.append(f'    inline constexpr wchar_t Module_Name[] = L{json.dumps(args.dll.name)};')
    for name,value in [('Package_Label', args.package_label), ('Embedded_Version', args.embedded_version), ('Dll_Sha256',proof['dll_sha256']), ('Pdb_Sha256',proof['pdb_sha256'])]:
        lines.append(f'    inline constexpr char {name}[] = {json.dumps(value)};')
    for name,value in [('Timestamp',timestamp),('Image_Size',size),('Codeview_Age',age)]+[(k+'_Rva',v) for k,v in entries.items()]:
        lines.append(f'    inline constexpr std::uint32_t {name} = 0x{value:X};')
    for name,values in [('Codeview_Guid',guid)]+[(k+'_Prefix',v) for k,v in prefixes.items()]:
        lines.append(f'    inline constexpr std::array<std::uint8_t, 16> {name}{{ '+', '.join(f'0x{x:02X}' for x in values)+' };')
    for kind,layout in layouts.items():
        lines.append(f'    inline constexpr std::size_t {kind}_Size = {layout["size"]};')
        for field,offset in layout['fields'].items(): lines.append(f'    inline constexpr std::size_t {kind}_{field} = {offset};')
    lines += ['}','PLUGIN_NAMESPACE_END','']
    args.header.write_text('\n'.join(lines),encoding='utf-8')
    base=struct.unpack_from('<Q',data,opt+24)[0]
    for name in ['ProcessEvent','PCProcessEvent','NPCProcessEvent']:
        start=base+entries[name]
        disassembly=subprocess.check_output(['llvm-objdump','-d',f'--start-address={start}',f'--stop-address={start+(512 if name=="ProcessEvent" else 96)}',str(args.dll)],text=True,encoding='utf-8')
        (args.evidence/(name+'-disassembly.txt')).write_text(disassembly,encoding='utf-8')
    print(json.dumps({'profile':str(args.header),'rvas':entries,'trigger_size':layouts['CastTrigger']['size'],'dll_sha256':proof['dll_sha256']}))


if __name__ == "__main__":
    main()
