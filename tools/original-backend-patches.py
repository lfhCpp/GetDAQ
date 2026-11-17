"""Apply small, reviewed entry-point diffs in the build directory only."""
import re

def apply_patch(source, patch):
    original=source.splitlines(keepends=True);result=[];position=0;lines=patch.splitlines(keepends=True);i=0
    while i<len(lines):
        if lines[i].startswith(('--- ','+++ ')):i+=1;continue
        match=re.match(r'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@',lines[i])
        if not match:raise ValueError('Invalid entry-point patch header')
        start=int(match[1])-1
        if start<position:raise ValueError('Overlapping entry-point patch')
        result.extend(original[position:start]);position=start;i+=1
        while i<len(lines) and not lines[i].startswith('@@ '):
            line=lines[i];kind=line[:1];text=line[1:]
            if kind in (' ','-'):
                if position>=len(original) or original[position]!=text:raise ValueError('Original entry-point patch context differs')
                position+=1
            if kind in (' ','+'):result.append(text)
            if kind not in (' ','-','+'):raise ValueError('Invalid patch line')
            i+=1
    result.extend(original[position:]);return ''.join(result)
