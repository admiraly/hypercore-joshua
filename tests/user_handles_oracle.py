"""Authored expectations for fixed two-incarnation target handle authority."""
MAGIC=0x314c444e4848534a
READY=0x3152444c4848534a
PROGRESS=0x314b4f4c444e484a

def check_handles(table,counters,ready,index,full=False):
    assert ready==READY
    assert len(table)==132 and len(counters)==9
    expected=[MAGIC,0x55530001+index,index+1,0]
    for slot in range(32):
        generation=(3 if slot<2 else 2) if full else (2 if slot<3 else 1)
        expected += [0,generation,0,0]
    assert table==expected,'Handle table differs from independent terminal expectation'
    assert counters==([45,3,33,2,4,1,1,1,1] if full else [13,3,2,2,4,1,1,0,1]),'Handle result accounting mismatch'
    initial=((index+1)<<48)|0x101
    return initial,initial+1,initial+2
