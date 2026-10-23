"""Independent response schema/string oracle against actual Assembly."""
import ctypes as C
import json
import mmap
import random
import struct
import sys

MAX = (1 << 64)-1
COMMAND_MAGIC = 0x31444e4d43484f4a
FACTS_MAGIC = 0x3154434146484f4a
RESPONSE_MAGIC = 0x3150534552484f4a
lib = C.CDLL(sys.argv[1])
call = lib.host_response_abi
call.argtypes = [C.c_void_p,C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint64]
call.restype = C.c_uint64
checks = 0
fields = 'image_bytes free_pages live_allocations candidate_ranges owned_table_pages enabled_cpus apic_mode timer_eois keyboard_irqs keyboard_eois keyboard_drops process_a_state process_a_status process_b_state process_b_status'.split()
groups = {
    3:('status','image_bytes enabled_cpus apic_mode timer_eois'.split()),
    4:('mem','free_pages live_allocations candidate_ranges owned_table_pages'.split()),
    5:('ps','process_a_state process_a_status process_b_state process_b_status'.split()),
    6:('devices','apic_mode enabled_cpus keyboard_irqs keyboard_eois keyboard_drops'.split())}
constants = {
    0:'',1:'HELP ABOUT STATUS MEM PS DEVICES CLEAR HALT\nSTATUS|MEM|PS|DEVICES --json\n',
    2:'JOSHUA / HYPERCORE\nBSP DEVELOPMENT CONSOLE\nASSEMBLY RUNTIME. NO AI EXECUTION.\nQEMU EVIDENCE; PHYSICAL QUALIFICATION OPEN.\n',
    7:'CONSOLE CLEARED\n',8:'HALTING: QUIESCING KEYBOARD\n'}


def require(ok,msg):
    global checks
    checks += 1
    assert ok,msg


def valid_command(command,status,flag,action):
    if command == 0:
        return (status,flag,action) == (0,0,0)
    if 1 <= command <= 8:
        if status == 2:
            return flag == action == 0
        return (status == 0 and flag in ([0,1] if command in groups else [0])
                and action == {7:1,8:2}.get(command,0))
    return (flag == action == 0 and
            ((9 <= command <= 17 and status in [2,69]) or
             (command == 255 and status in [2,127])))


def model(command,status,flag,values):
    if status:
        text = {2:'USAGE ERROR (2)\n',69:'UNAVAILABLE (69)\n',127:'UNKNOWN COMMAND (127)\n'}[status]
    elif command in constants:
        text = constants[command]
    else:
        name,keys = groups[command]
        if flag:
            data = {'command':name,'status':0}
            data.update({key:values[fields.index(key)] for key in keys})
            text = json.dumps(data,separators=(',',':'))+'\n'
        else:
            text = name.upper()+'\n'+''.join(f'{key}={values[fields.index(key)]}\n' for key in keys)
    body = text.encode('ascii')
    return struct.pack('<2Q',RESPONSE_MAGIC,len(body))+body.ljust(4096,b'\0')


command_arena = (C.c_uint64*7)()
facts_arena = (C.c_uint64*18)()
output_arena = (C.c_uint64*516)()
cmdptr = C.addressof(command_arena)+8
factptr = C.addressof(facts_arena)+8
outptr = C.addressof(output_arena)+8
baseline_values = [1000000,0,0,0,1,1,1,0,0,0,0,3,0,3,0]


def invoke(command,status,flag,action,values=None):
    values = values or baseline_values
    C.memset(C.addressof(command_arena),0xa5,C.sizeof(command_arena))
    C.memset(C.addressof(facts_arena),0xa5,C.sizeof(facts_arena))
    C.memset(C.addressof(output_arena),0x5a,C.sizeof(output_arena))
    C.memmove(cmdptr,struct.pack('<5Q',COMMAND_MAGIC,command,status,flag,action),40)
    C.memmove(factptr,struct.pack('<16Q',FACTS_MAGIC,*values),128)
    before = bytes(command_arena),bytes(facts_arena),bytes(output_arena)
    valid = valid_command(command,status,flag,action)
    require(call(cmdptr,factptr,outptr,0,0) == (0 if valid else 1), ('result / ABI',command,status,flag,action))
    require((bytes(command_arena),bytes(facts_arena)) == before[:2],'immutable inputs')
    if valid:
        expected = model(command,status,flag,values)
        require(C.string_at(outptr,4112) == expected, ('complete response / zero tail',command,status,flag))
        require(output_arena[0] == 0x5a5a5a5a5a5a5a5a and
                output_arena[-1] == 0x5a5a5a5a5a5a5a5a,'output guards')
        body = expected[16:16+struct.unpack_from('<Q',expected,8)[0]]
        require(len(body) <= 4095 and all(x == 10 or 32 <= x <= 126 for x in body),'output alphabet/capacity')
        if flag:
            require(json.loads(body)['status'] == 0,'independent JSON decode')
    else:
        require(bytes(output_arena) == before[2],'invalid command atomicity')


for command in list(range(19))+[255,MAX]:
    for status in [0,2,69,127,MAX]:
        for flag in [0,1,2,MAX]:
            for action in [0,1,2,3,MAX]:
                invoke(command,status,flag,action)
rng = random.Random(0x4a4f5348)
for _ in range(1000):
    values = [rng.getrandbits(64) for _ in fields]
    values[0] = max(1,values[0])
    values[4] = max(1,values[4])
    values[5] = rng.randrange(1,257)
    values[6] = rng.choice([1,2])
    values[9] = rng.randrange(values[8]+1)
    values[11] = rng.randrange(6)
    values[13] = rng.randrange(6)
    for command in groups:
        invoke(command,0,rng.randrange(2),0,values)
# Zero and maximum decimal values, including raw unsigned process statuses.
for extreme in [0,MAX]:
    values = [extreme for _ in fields]
    values[0] = values[4] = max(1,extreme)
    values[5] = 256
    values[6] = 2
    values[11] = values[13] = 5
    for command in groups:
        for flag in [0,1]:
            invoke(command,0,flag,0,values)
# Ready and bounded semantic corruption leave complete output unchanged.
invoke(3,0,1,0)
cmd = C.string_at(cmdptr,40)
facts = C.string_at(factptr,128)
for target,offset,value in [(cmdptr,0,0),(factptr,0,0),(factptr,8,0),
                            (factptr,40,0),(factptr,48,0),(factptr,48,257),
                            (factptr,56,0),(factptr,56,3),(factptr,96,6),
                            (factptr,112,MAX),(factptr,80,1)]:
    C.memmove(cmdptr,cmd,40)
    C.memmove(factptr,facts,128)
    C.c_uint64.from_address(target+offset).value = value
    before = bytes(command_arena),bytes(facts_arena),bytes(output_arena)
    require(call(cmdptr,factptr,outptr,0,0) == 1,'invalid snapshot / ABI')
    require((bytes(command_arena),bytes(facts_arena),bytes(output_arena)) == before,'snapshot atomicity')
C.memmove(cmdptr,cmd,40)
C.memmove(factptr,facts,128)
for args in [(0,factptr,outptr),(cmdptr,0,outptr),(cmdptr,factptr,0),
             (cmdptr+1,factptr,outptr),(cmdptr,factptr+1,outptr),(cmdptr,factptr,outptr+1),
             (MAX-7,factptr,outptr),(cmdptr,MAX-7,outptr),(cmdptr,factptr,MAX-7),
             (cmdptr,cmdptr+8,outptr),(cmdptr,factptr,cmdptr),
             (cmdptr,factptr,factptr),(cmdptr,factptr,cmdptr-4096),
             (cmdptr,factptr,factptr-4096)]:
    before = bytes(command_arena),bytes(facts_arena),bytes(output_arena)
    require(call(*args,0,0) == 1,'invalid extent / overlap')
    require((bytes(command_arena),bytes(facts_arena),bytes(output_arena)) == before,'extent atomicity')
# Exercise the actual internal append gate at capacity and unsigned-overflow
# boundaries. Current fixed templates cannot exhaust the public output buffer.
append = lib.host_response_append
append.argtypes = [C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint32,C.c_uint32]
append.restype = C.c_uint32
source_bytes = (C.c_ubyte*4096)(*([65]*4096))
for count in [0,4094,4095,4096,MAX]:
    for length in [0,1,2,4095,4096]:
        for phase in [0,1]:
            C.memset(C.addressof(output_arena),0x5a,C.sizeof(output_arena))
            C.c_uint64.from_address(outptr+8).value = count
            before = bytearray(output_arena)
            ok = count+length <= 4095
            require(append(outptr,C.addressof(source_bytes),count,length,phase)
                    == (0 if ok else 1),'actual append capacity gate')
            expected = before[:]
            if ok:
                struct.pack_into('<Q',expected,16,count+length)
                if phase:
                    expected[24+count:24+count+length] = b'A'*length
            require(bytes(output_arena) == expected,'append exact count / failure atomicity')
# Real inaccessible boundaries for all complete extents, including 20-digit text.
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p,C.c_size_t,C.c_int]
page = mmap.PAGESIZE
for end in [False,True]:
    arenas = []
    pointers = []
    try:
        for size in [40,128,4112]:
            pages = (size+page-1)//page
            memory = mmap.mmap(-1,(pages+2)*page)
            anchor = C.c_char.from_buffer(memory)
            base = C.addressof(anchor)
            require(libc.mprotect(base,page,0) == 0,'guard before')
            require(libc.mprotect(base+(pages+1)*page,page,0) == 0,'guard after')
            arenas.append((memory,anchor,base,pages))
            pointers.append(base+(pages+1)*page-size if end else base+page)
        values = [MAX for _ in fields]
        values[5],values[6],values[11],values[13] = 256,2,5,5
        C.memmove(pointers[1],struct.pack('<16Q',FACTS_MAGIC,*values),128)
        for command in list(constants)+list(groups):
            for flag in ([0,1] if command in groups else [0]):
                action = {7:1,8:2}.get(command,0)
                C.memmove(pointers[0],struct.pack('<5Q',COMMAND_MAGIC,command,0,flag,action),40)
                require(call(*pointers,0,0) == 0,'guarded build / ABI')
                require(C.string_at(pointers[2],4112) == model(command,0,flag,values),'guarded output')
    finally:
        for memory,anchor,base,pages in arenas:
            require(libc.mprotect(base,page,3) == 0,'restore before')
            require(libc.mprotect(base+(pages+1)*page,page,3) == 0,'restore after')
        arenas.clear()
        del anchor
print(f'Console response: {checks:,} cumulative checks passed')
