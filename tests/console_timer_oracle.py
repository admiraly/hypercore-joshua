"""Physical console timer fixture predicates, shared by capture and mutations."""
import re
import struct
from keyboard_runtime_oracle import pending_vectors


def validate_timer_fixture(mode, source, window, lapic):
    assert mode in ('UnexpectedTimer','PendingTimer','DrainExhaust'), 'timer fixture mode'
    assert len(source)==64 and len(window)==288, 'exact evidence extents'
    t=struct.unpack('<8Q',source)
    w=struct.unpack('<20Q64H',window)
    assert w[0]==0x3157424b48534f4a and w[2] and w[2]%4096==0, 'owned window identity'
    assert w[10]==0x202 and w[13]==w[17]==0, 'exact flags, no ticks/failure'
    assert w[14]==w[15] and w[14]>=1, 'actual keyboard IRQ accounting'
    assert t[1]&1 and t[1]&~0x11==0 and t[2]==0, 'actual pending40 proof and EOI baseline'
    text=lapic.replace('\\r\\n','\n').replace('\r','')
    lvt=re.search(r'(?m)^LVTT\s+0x([0-9a-fA-F]+)\b',text)
    timer=re.search(r'Timer\s+DCR=0x([0-9a-fA-F]+).*?initial_count\s*=\s*(\d+)\s+current_count\s*=\s*(\d+)',text)
    assert lvt and timer and int(timer[1],16)==3 and int(timer[3])==0, 'actual expired/stopped timer'
    if mode=='UnexpectedTimer':
        assert t[0]==2 and w[1]==1 and w[16]==w[18]==0, 'phase1 rejection before EOI'
        assert int(lvt[1],16)==0x40 and int(timer[2])==1, 'injected unmasked one-shot'
        assert pending_vectors(text,'ISR')=={64} and pending_vectors(text,'IRR')<={68}, 'actual timer ISR, only keyboard possibly pending'
    else:
        assert int(lvt[1],16)==0x10040 and int(timer[2])==0, 'masked stopped timer'
        assert pending_vectors(text,'ISR')==set(), 'no in-service delivery'
        if mode=='PendingTimer':
            assert t[0]==2 and w[1]==3 and w[16]==1 and 1<=w[18]<=1024, 'one owned drain EOI'
            assert pending_vectors(text,'IRR')==set(), 'complete physical drain'
        else:
            assert t[0]==1 and w[1]==2 and w[16]==0 and w[18]==1025, 'bounded drain rejection before EOI'
            assert pending_vectors(text,'IRR')=={64} and t[3:]==(0,)*5, 'timer still pending with no delivery'
    if mode!='DrainExhaust':
        assert t[3]==w[6 if mode=='UnexpectedTimer' else 7], 'exact saved continuation'
        assert t[4]==w[8] and t[6]==w[3] and t[7]==w[9], 'exact return selectors and stack'
        assert t[5]&~0x10000==0x202, 'only architectural RF tolerance'
