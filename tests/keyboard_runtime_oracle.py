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
