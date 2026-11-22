#!/usr/bin/env python3
"""Render the maintained guide TeX subset into offline Qt rich-text HTML.
No TeX, PDF or web packages are required to regenerate the HTML.
"""
from pathlib import Path
import html
import re

ROOT=Path(__file__).resolve().parents[1]
MANUAL=ROOT/'docs/manual'

class Renderer:
 def __init__(self):self.unknown=set();self.chapters=[]
 def group(self,s,pos,left='{',right='}'):
  while pos<len(s) and s[pos].isspace():pos+=1
  if pos>=len(s) or s[pos]!=left:raise ValueError('Missing '+left+' at '+s[pos:pos+80])
  start=pos+1;depth=1;pos+=1
  while pos<len(s):
   if s[pos]=='\\' and pos+1<len(s) and s[pos+1] in '{}[]':pos+=2;continue
   if s[pos]==left:depth+=1
   if s[pos]==right:
    depth-=1
    if not depth:return s[start:pos],pos+1
   pos+=1
  raise ValueError('Unclosed guide group')
 def optional(self,s,pos):
  if pos<len(s) and s[pos]=='[':return self.group(s,pos,'[',']')
  return '',pos
 def heading(self,title):
  rendered=self.render(title);plain=html.unescape(re.sub('<[^>]+>','',rendered)).strip();number=re.match(r'\d+',plain);anchor='chapter-'+(number.group() if number else str(len(self.chapters)+1));self.chapters.append((anchor,plain))
  return '<a name="'+anchor+'"></a><h1 id="'+anchor+'">'+html.escape(plain)+'</h1>'
 def render(self,s):
  out=[];i=0
  while i<len(s):
   c=s[i]
   if c=='\\':
    if i+1<len(s) and s[i+1] in '_%&#${}~':out.append(html.escape(s[i+1]));i+=2;continue
    if i+1<len(s) and s[i+1]=='\\':out.append('<br/>');i+=2;continue
    m=re.match(r'\\([A-Za-z]+)\*?',s[i:])
    if not m:out.append(html.escape(c));i+=1;continue
    command=m.group(1);i+=len(m.group())
    if command=='begin':
     env,i=self.group(s,i)
     if env=='tabularx':_,i=self.group(s,i);_,i=self.group(s,i)
     if env=='tikzpicture':_,i=self.optional(s,i)
     end='\\end{'+env+'}';finish=s.find(end,i)
     if finish<0:raise ValueError('Unclosed '+env)
     body=s[i:finish];i=finish+len(end)
     if env=='verbatim':out.append('<pre>'+html.escape(body.strip('\n'))+'</pre>')
     elif env=='tabularx':
      body=re.sub(r'\\(?:toprule|midrule|bottomrule)\b','',body);rows=[]
      for row in re.split(r'\\\\',body):
       if not row.strip():continue
       cols=re.split(r'(?<!\\)&',row);tag='th' if not rows else 'td';rows.append('<tr>'+''.join('<'+tag+'>'+self.render(col.strip())+'</'+tag+'>' for col in cols)+'</tr>')
      out.append('<table cellspacing="0" cellpadding="7">'+''.join(rows)+'</table>')
     elif env in ('enumerate','itemize'):
      tag='ol' if env=='enumerate' else 'ul';items=re.split(r'\\item\b\s*',body);out.append('<'+tag+'>'+''.join('<li>'+self.render(x.strip())+'</li>' for x in items[1:])+'</'+tag+'>')
     elif env=='tikzpicture':out.append('<p class="note"><b>GetDAQ → SOAP ECC → CoBo / AsAd / AGET</b><br/>CoBo → DataExporter / DataRouter → raw data<br/>Sentry / node agent: monitoring and management</p>')
     else:out.append(self.render(body))
    elif command in ('ui','textbf','code','note','subsection','section'):
     value,i=self.group(s,i);tag={'ui':'b','textbf':'b','code':'code','note':'blockquote','subsection':'h2','section':'h1'}[command];out.append('<'+tag+'>'+self.render(value)+'</'+tag+'>')
    elif command=='pagehead':
     title,i=self.group(s,i);lead,i=self.group(s,i);out.append(self.heading(title)+'<p class="lead">'+self.render(lead)+'</p>')
    elif command=='textcolor':_,i=self.group(s,i);value,i=self.group(s,i);out.append(self.render(value))
    elif command=='includegraphics':
     _,i=self.optional(s,i);value,i=self.group(s,i);out.append('<p><img src="'+html.escape(value,quote=True)+'" width="660"/></p>')
    elif command=='pdfbookmark':_,i=self.optional(s,i);_,i=self.group(s,i);_,i=self.group(s,i)
    elif command in ('label','vspace','color'):_,i=self.group(s,i)
    elif command=='fontsize':_,i=self.group(s,i);_,i=self.group(s,i)
    elif command=='par':out.append('<br/><br/>')
    elif command=='newpage':out.append('<hr/>')
    elif command in ('smallskip','quad'):out.append(' ')
    elif command in ('selectfont','bfseries','footnotesize','noindent','vfill','small','Large'):pass
    elif command in ('rightarrow','Delta','times','textasciitilde','textbackslash'):out.append({'rightarrow':'→','Delta':'Δ','times':'×','textasciitilde':'~','textbackslash':'\\'}[command])
    elif command in ('href','url'):
     value,i=self.group(s,i)
     if command=='href':label,i=self.group(s,i)
     else:label=value
     out.append('<a href="'+html.escape(value,quote=True)+'">'+self.render(label)+'</a>')
    else:self.unknown.add(command)
   elif c=='{':value,i=self.group(s,i);out.append(self.render(value))
   elif c=='$':
    finish=s.find('$',i+1)
    if finish<0:raise ValueError('Unclosed inline math')
    out.append(self.render(s[i+1:finish]));i=finish+1
   else:
    start=i
    while i<len(s) and s[i] not in '\\{$':i+=1
    out.append(html.escape(s[start:i]))
  return ''.join(out)

def expand(path):
 source=path.read_text();source=re.sub(r'(?m)^\s*%[^\n]*','',source)
 return re.sub(r'\\input\{([^}]+)\}',lambda m:expand(path.parent/m.group(1)),source)

def main():
 for language,filename,title in [('zh','GetDAQ-Quick-Guide.tex','GetDAQ 中文使用说明'),('en','GetDAQ-Quick-Guide-en.tex','GetDAQ User Guide')]:
  source=expand(MANUAL/filename).split(r'\begin{document}',1)[1].rsplit(r'\end{document}',1)[0];renderer=Renderer();prefix=renderer.heading('1 运行原理与总览') if language=='zh' else '';body=renderer.render(source)
  if renderer.unknown:raise ValueError('Unsupported guide macros: '+str(sorted(renderer.unknown)))
  if len(renderer.chapters)!=20:raise ValueError('Expected 20 guide chapters: '+str(renderer.chapters))
  style='body {color:#17364d;font-family:GetDAQ Guide Sans;font-size:11pt;} h1 {color:#17364d;} h2 {color:#087f8c;} table {border-collapse:collapse;width:100%;margin-top:8px;margin-bottom:16px;} td,th {border:1px solid #cbd5df;vertical-align:top;} th {background-color:#eff5f8;text-align:left;} blockquote {background-color:#eff5f8;margin:12px 0;padding:12px;} pre,code {font-family:monospace;} pre {background-color:#f4f6f8;padding:10px;white-space:pre-wrap;} .lead {color:#526477;} hr {color:#cbd5df;}'
  output='<!doctype html><html lang="'+language+'"><head><meta charset="utf-8"/><title>'+title+'</title><style>'+style+'</style></head><body>'+prefix+body+'</body></html>\n'
  destination=MANUAL/('GetDAQ-Quick-Guide-'+language+'.html');destination.write_text(output);print('Generated',destination.name,'chapters:',len(renderer.chapters))

if __name__=='__main__':main()
