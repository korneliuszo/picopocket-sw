#!/usr/bin/env python3

# - <>
#   uid: uint16_t
#   name: str
#   c_validate: |
#   help: |
#   type: str/hex/uint/dir/ip
#   def_val: str!!
#   runtime;
#   to_flash; 
#   - <>

import re
import sys
import pprint

def loaddef(deflines):
    uid_map = {}
    entries = {}
    multiline = None
    current_entry = None
    for line in deflines:
        if False:
            pass
        elif m:=re.search(r'UID_TABLE\( *\)',line) is not None:
            uid_no=0;
        elif (m:=re.search(r'UID_TABLE_ENTRY\( *([_a-zA-Z0-9]*) *\)',line)) is not None:
            uid_map[m.group(1)] = uid_no
            uid_no+=1;
        elif m:=re.search(r'UID_TABLE_END\( *\)',line) is not None:
            uid_no=None;
            
        elif m:=re.search(r'FIELD_STORED_TABLE\( *\)',line) is not None:
            pass
        elif (m:=re.search(r'STORE_FIELD_FIRST\( *([_a-zA-Z0-9]*) *\)',line)) is not None:
            entries[m.group(1)]["to_flash"] = True
        elif (m:=re.search(r'STORE_FIELD\( *([_a-zA-Z0-9]*) *\)',line)) is not None:
            entries[m.group(1)]["to_flash"] = True
        elif m:=re.search(r'FIELD_STORED_TABLE_END\( *\)',line) is not None:
            pass
            
        elif (m:=re.search(r'STR_FIELD\( *([_0-9a-zA-Z]*) *, *(.*) *, *"(.*)" *\)',line)) is not None:
            entries[m.group(1)] = dict({
                "uid":uid_map[m.group(1)],
                "type" :"str," + m.group(2),
                "def_val":m.group(3),
                })
            current_entry = entries[m[1]]
        elif (m:=re.search(r'STR_FIELD_END\( *\)',line)) is not None:
            if current_entry["type"].split(",")[0] != "str":
                print("not expected line: ",line,file=sys.stderr)
            current_entry = None
        elif (m:=re.search(r'UINT_FIELD\( *([_0-9a-zA-Z]*) *, *(.*) *, *(.*) *\)',line)) is not None:
            entries[m.group(1)] = dict({
                "uid":uid_map[m.group(1)],
                "type" :"uint,"+m.group(2),
                "def_val":m.group(3),
                })
            current_entry = entries[m[1]]
        elif (m:=re.search(r'UINT_FIELD_END\( *\)',line)) is not None:
            if current_entry["type"].split(",")[0] != "uint":
                print("not expected line: ",line,file=sys.stderr)
            current_entry = None
        elif (m:=re.search(r'HEX_FIELD\( *([_0-9a-zA-Z]*) *, *(.*) *, *(.*) *\)',line)) is not None:
            entries[m.group(1)] = dict({
                "uid":uid_map[m.group(1)],
                "type" :"hex,"+m.group(2),
                "def_val":m.group(3),
                })
            current_entry = entries[m.group(1)]
        elif (m:=re.search(r'HEX_FIELD_END\( *\)',line)) is not None:
            if current_entry["type"].split(",")[0] != "hex":
                print("not expected line: ",line,file=sys.stderr)
            current_entry = None
        elif (m:=re.search(r'IP_FIELD\( *([_0-9a-zA-Z]*) *, *(.*) *\)',line)) is not None:
            entries[m.group(1)] = dict({
                "uid":uid_map[m.group(1)],
                "type" :"ip",
                "def_val":m.group(2),
                })
            current_entry = entries[m.group(1)]
        elif (m:=re.search(r'IP_FIELD_END\( *\)',line)) is not None:
            if current_entry["type"] != "ip":
                print("not expected line: ",line,file=sys.stderr)
            current_entry = None
        elif (m:=re.search(r'FIELD_NAME\( *"(.*)" *\)',line)) is not None:
            current_entry["name"] = m.group(1)
        elif (m:=re.search(r'FIELD_C_VALIDATE\((.*)\)',line)) is not None:
            current_entry["c_validate_type"] = m.group(1)
            current_entry["c_validate_c_program"] = []
            multiline = (r'FIELD_C_VALIDATE_END\(\)',current_entry["c_validate_c_program"])
        elif multiline and (m:=re.search(multiline[0],line)) is not None:
            multiline = None
        elif multiline:
            multiline[1].append(line)
        elif (m:=re.search(r'FIELD_HELP\(\)',line)) is not None:
            current_entry["help"] = []
            multiline = (r'FIELD_HELP_END\(\)',current_entry["help"])
        elif (m:=re.search(r'FIELD_COLDBOOT\(false\)',line)) is not None:
            current_entry["runtime"] = True
        elif (m:=re.search(r'FIELD_COLDBOOT\(true\)',line)) is not None:
            pass
        elif (m:=re.search(r'DIR_FIELD\( *([_0-9a-zA-Z]*) *\)',line)) is not None:
            entries[m.group(1)] = dict({
                "uid":uid_map[m.group(1)],
                "type" :"dir",
                "elements": [],
                })
            current_entry = entries[m.group(1)]
            current_dir = entries[m.group(1)]["elements"]
        elif (m:=re.search(r'DIR_FIELD_END\( *\)',line)) is not None:
            if current_entry["type"] != "dir":
                print("not expected line: ",line,file=sys.stderr)
            current_entry = None
        elif (m:=re.search(r'BOOL_FIELD\( *([_0-9a-zA-Z]*) *, *(.*) *\)',line)) is not None:
            entries[m.group(1)] = dict({
                "uid":uid_map[m.group(1)],
                "type" :"bool",
                "default_val": m.group(2),
                })
            current_entry = entries[m[1]]
        elif (m:=re.search(r'BOOL_FIELD_END\( *\)',line)) is not None:
            if current_entry["type"] != "bool":
                print("not expected line: ",line,file=sys.stderr)
            current_entry = None
        elif (m:=re.search(r'FIELD_ENTRY\( *([_0-9a-zA-Z]*) *\)',line)) is not None:
            current_dir.append(entries[m.group(1)])
        elif (m:=re.search(r'DIR_FIELD_END_ENTRIES\( *\)',line)) is not None:
            current_dir = None 
        elif len(line)==1:
            pass               
        else:
            print("unrecognized line: ",line,file=sys.stderr)  

    print(uid_map)
    pprint.pp([v for k,v in entries.items() if v["uid"]==0][0])

if __name__ == "__main__":
    loaddef(open("config.def","rt").readlines())
    