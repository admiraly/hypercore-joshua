"""Contract-derived live editor/transcript reference; no Assembly code imported."""
import json
import struct

MAX=(1<<64)-1
MAGIC=0x3152444553484f4a
class Editor:
    def __init__(self):
        self.line = self.draft = self.submitted = ''
        self.cursor = self.draft_cursor = self.position = 0
        self.history = [''] * 8
        self.count = self.next = 0
        self.submissions = self.rejected = self.cancellations = 0

    def image(self):
        fields = [MAGIC, len(self.line), self.cursor, self.count, self.next,
                  self.position, len(self.draft), self.draft_cursor,
                  len(self.submitted), self.submissions, self.rejected,
                  self.cancellations]
        def buf(s):
            return s.encode('ascii').ljust(128, b'\0')
        return (struct.pack('<12Q', *fields) + buf(self.line) + buf(self.draft)
                + buf(self.submitted) + struct.pack('<8Q', *map(len, self.history))
                + b''.join(map(buf, self.history)))

    def event(self, key):
        if key > 65535:
            return 1
        if key == 13:
            self.submitted = self.line
            self.submissions = min(MAX, self.submissions + 1)
            if self.line:
                self.history[self.next] = self.line
                self.next = (self.next + 1) % 8
                self.count = min(8, self.count + 1)
            self.line = self.draft = ''
            self.cursor = self.draft_cursor = self.position = 0
            return 2 << 32
        if key == 3:
            self.line = self.draft = ''
            self.cursor = self.draft_cursor = self.position = 0
            self.cancellations = min(MAX, self.cancellations + 1)
            return 4 << 32
        if key == 0x105:
            if self.position == self.count:
                return 0
            if not self.position:
                self.draft, self.draft_cursor = self.line, self.cursor
            self.position += 1
            self.line = self.history[(self.next - self.position) % 8]
            self.cursor = len(self.line)
        elif key == 0x106:
            if not self.position:
                return 0
            self.position -= 1
            if self.position:
                self.line = self.history[(self.next - self.position) % 8]
                self.cursor = len(self.line)
            else:
                self.line, self.cursor = self.draft, self.draft_cursor
        elif key in (0x100, 0x101, 0x102, 0x103):
            target = {0x100: max(0, self.cursor-1),
                      0x101: min(len(self.line), self.cursor+1),
                      0x102: 0, 0x103: len(self.line)}[key]
            if target == self.cursor:
                return 0
            self.cursor = target
        elif key == 8 or key == 0x104:
            at = self.cursor - (key == 8)
            if at < 0 or at >= len(self.line):
                return 0
            self.line = self.line[:at] + self.line[at+1:]
            self.cursor = at
            self.position = 0
        elif 32 <= key <= 126:
            if len(self.line) == 127:
                self.rejected = min(MAX, self.rejected + 1)
                return 3 << 32
            self.line = self.line[:self.cursor] + chr(key) + self.line[self.cursor:]
            self.cursor += 1
            self.position = 0
        else:
            return 0
        return 1 << 32


NAMES='help about status mem ps devices clear halt files net model agents grant revoke bench logs shutdown'.split()
FIELDS='image_bytes free_pages live_allocations candidate_ranges owned_table_pages enabled_cpus apic_mode timer_eois keyboard_irqs keyboard_eois keyboard_drops process_a_state process_a_status process_b_state process_b_status'.split()
GROUPS={3:[0,5,6,7],4:[1,2,3,4],5:[11,12,13,14],6:[6,5,8,9,10]}


def parse(text):
    words=text.split()
    ident=NAMES.index(words[0].lower())+1 if words and words[0].lower() in NAMES else (255 if words else 0)
    status=flag=action=0
    if len(words)>2:status=2
    elif ident==255:status=127
    elif len(words)==2:
        if ident in GROUPS and words[1]=='--json':flag=1
        else:status=2
    elif ident>=9:status=69
    else:action={7:1,8:2}.get(ident,0)
    return ident,status,flag,action


def response(command,facts):
    ident,status,flag,_=command
    if status:return {2:'USAGE ERROR (2)\n',69:'UNAVAILABLE (69)\n',127:'UNKNOWN COMMAND (127)\n'}[status]
    if ident not in GROUPS:
        return {0:'',1:'HELP ABOUT STATUS MEM PS DEVICES CLEAR HALT\nSTATUS|MEM|PS|DEVICES --json\n',
                2:'JOSHUA / HYPERCORE\nBSP DEVELOPMENT CONSOLE\nASSEMBLY RUNTIME. NO AI EXECUTION.\nQEMU EVIDENCE; PHYSICAL QUALIFICATION OPEN.\n',
                7:'CONSOLE CLEARED\n',8:'HALTING: QUIESCING KEYBOARD\n'}[ident]
    name=NAMES[ident-1]
    values={FIELDS[i]:facts[i] for i in GROUPS[ident]}
    if flag:return json.dumps({'command':name,'status':0,**values},separators=(',',':'))+'\n'
    return name.upper()+'\n'+''.join(key+'='+str(value)+'\n' for key,value in values.items())


class LiveReference:
    def __init__(self,anchor):
        self.anchor=struct.unpack('<6Q',anchor)
        _,_,width,height,_,_=self.anchor
        self.cols=min(100,(width-32)//12)
        self.rows=min(48,(height-32)//16)
        assert 48<=self.cols<=100 and 20<=self.rows<=48
        self.editor=Editor()
        self.cells=bytearray(4800)
        for y in range(self.rows):self.row_text(y,'')
        self.row_text(0,'JOSHUA / HYPERCORE')
        self.row_text(1,'BSP DEVELOPMENT CONSOLE | NO AI EXECUTION')
        self.row_text(self.rows-1,'READY / BSP DEVELOPMENT CONSOLE')
        self.row,self.col,self.completed=2,0,0
        self.last=(0,0,0,0)
        self.command=bytes(40)
        self.facts=bytes(128)
        self.response=bytes(4112)
        self.prompt()
        self.stopped=False

    def row_text(self,y,text):
        assert len(text)<=self.cols
        self.cells[y*100:y*100+self.cols]=text.ljust(self.cols).encode('ascii')

    def prompt(self):
        start=max(0,self.editor.cursor-(self.cols-9))
        self.row_text(self.rows-2,'JOSHUA> '+self.editor.line[start:start+self.cols-8])
        self.cursor=(8+self.editor.cursor-start,self.rows-2,1)

    def advance(self):
        self.col=0
        if self.row==self.rows-3:
            self.cells[200:(self.rows-3)*100]=self.cells[300:(self.rows-2)*100]
            self.row_text(self.rows-3,'')
        else:self.row+=1

    def append(self,text):
        for char in text:
            if char=='\n':self.advance()
            else:
                if self.col==self.cols:self.advance()
                self.cells[self.row*100+self.col]=ord(char)
                self.col+=1

    def event(self,key,facts=None):
        action=self.editor.event(key)>>32
        if action==2:
            assert facts is not None and len(facts)==15,'independent pre-submission facts'
            command=parse(self.editor.submitted)
            self.command=struct.pack('<5Q',0x31444e4d43484f4a,*command)
            self.facts=struct.pack('<16Q',0x3154434146484f4a,*facts)
            body=response(command,facts)
            raw=body.encode('ascii')
            assert len(raw)<=4095
            self.response=struct.pack('<2Q',0x3150534552484f4a,len(raw))+raw.ljust(4096,b'\0')
            if command[3]==1:
                for y in range(2,self.rows-2):self.row_text(y,'')
                self.row,self.col=2,0
            else:
                if self.col:self.advance()
                self.append('JOSHUA> '+self.editor.submitted+'\n')
            self.append(body)
            self.completed=min(MAX,self.completed+1)
            self.last=command
            self.row_text(self.rows-1,{0:'RESULT 0 / OK',2:'RESULT 2 / USAGE',69:'RESULT 69 / UNAVAILABLE',127:'RESULT 127 / UNKNOWN'}[command[1]])
            self.prompt()
        elif action in (1,4):self.prompt()

    def images(self,terminal=False):
        if terminal:
            assert self.last==(8,0,0,2),'only a valid halt may stop'
            self.row_text(self.rows-1,'HALTED / INTERRUPTS RETIRED')
            self.cursor=(*self.cursor[:2],0)
            self.stopped=True
        return {'editor':self.editor.image(),
                'model':struct.pack('<8Q',0x314c444f4d484f4a,self.row,self.col,self.completed,*self.last),
                'grid':struct.pack('<13Q',0x3144495247484f4a,*self.anchor,self.cols,self.rows,0,*self.cursor)+self.cells,
                'command':self.command,'facts':self.facts,'response':self.response}

    def check(self,actual,terminal=False):
        expected=self.images(terminal)
        for name,data in expected.items():
            assert actual[name]==data,'independent complete live '+name
        return expected
