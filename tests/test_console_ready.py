"""Exercise the actual target readiness regex without booting or importing the driver."""
import ast
from pathlib import Path
import re

source=ast.parse((Path(__file__).with_name('test_console_runtime.py')).read_text())
pattern=next(ast.literal_eval(n.value) for n in source.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='READY_PATTERN' for t in n.targets))
ready=re.compile(pattern)
fields='MODEL GRID EDITOR COMMAND RESPONSE FACTS WINDOW DECODER CONTROLLER'.split()
line='JOSHUA:CONSOLE_READY '+' '.join(name+'='+format(i+4096,'016X') for i,name in enumerate(fields))+'\r\n'
match=ready.search(line)
assert match and tuple(int(x,16) for x in match.groups())==tuple(range(4096,4105))
checks=1
# Every possible interrupted serial write must remain pending, not a parsed record.
for end in range(len(line)):
    assert ready.search(line[:end]) is None
    checks+=1
assert ready.search('old log\n'+line+'next line\n')
assert ready.search(line.replace('\r\n','\n'))
checks+=2
for field in fields:
    damaged=line.replace(field+'=',field+'=G',1)
    assert ready.search(damaged) is None
    checks+=1
assert ready.search(line.rstrip()+' junk\n') is None
checks+=1
print(f'PASS: {checks} complete/partial actual console READY parser checks')
