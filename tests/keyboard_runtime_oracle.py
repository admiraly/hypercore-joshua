"""Independent target-state predicates shared by actual capture and mutations."""
import re


def validate_lapic(text):
    text=text.replace('\\r\\n','\n').replace('\r','')
    lvt=re.search(r'(?m)^LVTT\s+0x([0-9a-fA-F]+)\b',text)
    assert lvt and int(lvt[1],16)==0x10040,'actual masked one-shot vector40 LVT'
    timer=re.search(r'Timer\s+DCR=0x([0-9a-fA-F]+).*?initial_count\s*=\s*(\d+)\s+current_count\s*=\s*(\d+)',text)
    assert timer and int(timer[1],16)==3 and int(timer[2])==0 and int(timer[3])==0,'actual stopped deadline counts'
    for name in ['ISR','IRR']:
        assert re.search(r'(?m)^'+name+r'\s+\(none\)\s*$',text),'actual empty '+name


def validate_queue_indices(head,tail,count):
    assert 0<=head<64 and 0<=tail<64 and 0<=count<=64,'physical queue index bounds'
    assert tail==(head+count)%64,'physical queue index invariant'


def pending_vectors(text,name):
    text=text.replace('\\r\\n','\n').replace('\r','')
    match=re.search(r'(?m)^'+name+r'[ \t]+([^\n]*)$',text)
    assert match,'missing actual '+name
    value=match[1].strip()
    if value=='(none)':return set()
    assert re.fullmatch(r'[0-9 ]+',value),'unknown actual '+name+' format'
    return {int(number) for number in value.split()}


def validate_stopped_lapic(text,isr,irr,masked):
    text=text.replace('\\r\\n','\n').replace('\r','')
    lvt=re.search(r'(?m)^LVTT\s+0x([0-9a-fA-F]+)\b',text)
    assert lvt and int(lvt[1],16)==(0x10040 if masked else 0x40),'actual fixture timer LVT'
    timer=re.search(r'Timer\s+DCR=0x([0-9a-fA-F]+).*?initial_count\s*=\s*(\d+)\s+current_count\s*=\s*(\d+)',text)
    assert timer and int(timer[1],16)==3 and int(timer[3])==0,'actual stopped/expired count'
    assert int(timer[2])==(0 if masked else 0x100000),'actual initial timer count'
    assert pending_vectors(text,'ISR')==isr and pending_vectors(text,'IRR')==irr,'actual terminal pending/in-service vectors'


def validate_refill_evidence(fixture):
    assert fixture[11:13]==(32,1) and fixture[13]>=1,'actual reads/one disable/failure-preserving timer'
    assert fixture[15]==0,'refill timeout cannot qualify data-budget exhaustion'
    assert fixture[16]==3,'both refill checkpoints must observe actual OBF'
    assert fixture[17]&1,'actual OBF must remain set at zero data budget'


def validate_output_quiet(text):
    text=text.replace('\\r\\n','\n').replace('\r','')
    status=re.fullmatch(r'\s*portb\[0x0*64\]\s*=\s*0x([0-9a-fA-F]{1,2})\s*',text)
    assert status,'actual byte-port64 response required'
    assert int(status[1],16)&1==0,'actual terminal output must be empty'
