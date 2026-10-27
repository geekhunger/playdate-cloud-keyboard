"""Reproducible keyboard action counts, not human speed measurements.
Fixed bilingual corpus from the original layout study; lowercase ASCII only.
Text entry only, each phrase starts fresh; excludes Done/Cancel and corrections.
An action is one D-pad or A/B press. Crank results count logical selection ticks
separately; they do not equate physical rotation with a button press.
"""
from collections import deque
import json
from dataclasses import dataclass
from pathlib import Path
HERE=Path(__file__).resolve().parent
CORPORA = {
    "English": [
        "client notes",
        "project update",
        "meet anna at noon",
        "weekly planning",
        "important reminder",
        "release notes ready",
    ],
    "German": [
        "neue notiz",
        "termin morgen",
        "einkaufsliste",
        "besprechung um zehn",
        "projekt status",
        "kunden termin",
    ],
}

CIRCLE_GROUPS = [
    "abcdefgh",
    "ijklmnop",
    "qrstuvwx",
    "yz,.!?-_",
    "0123456789",
]

@dataclass(frozen=True)
class Score:
    actions: int
    dpad: int = 0
    buttons: int = 0
    crank_ticks: int = 0
    crank_gestures: int = 0
    waits: int = 0

    def __add__(self, other: "Score") -> "Score":
        return Score(
            self.actions + other.actions,
            self.dpad + other.dpad,
            self.buttons + other.buttons,
            self.crank_ticks + other.crank_ticks,
            self.crank_gestures + other.crank_gestures,
            self.waits + other.waits,
        )

def circle_confirmations(text: str) -> int:
    """A confirmations needed to type continuously without 750 ms waits."""
    return sum(
        1
        for index, char in enumerate(text)
        if char != " " and (index == len(text) - 1 or text[index + 1] != " ")
    )

def score_circle(text: str, method: str, no_wait: bool = True) -> Score:
    """Score Circle with method set to dpad, crank, or hybrid."""
    positions = {
        char: (group, index)
        for group, values in enumerate(CIRCLE_GROUPS)
        for index, char in enumerate(values)
    }
    group = 0
    score = Score(0)

    for char in text:
        if char == " ":
            score += Score(1, buttons=1)
            continue

        target_group, index = positions[char]
        clockwise = (target_group - group) % len(CIRCLE_GROUPS)
        counterclockwise = (group - target_group) % len(CIRCLE_GROUPS)
        turns = min(clockwise, counterclockwise)
        if method == "dpad":
            score += Score(turns, dpad=turns)
        else:
            score += Score(
                turns,
                crank_ticks=turns,
                crank_gestures=int(turns > 0),
            )
        group = target_group

        group_size = len(CIRCLE_GROUPS[group])
        half_size = group_size // 2
        left_start = half_size - 1
        right_start = half_size
        left_taps = 1 + ((left_start - index) % group_size)
        right_taps = 1 + ((index - right_start) % group_size)
        dpad_taps = min(left_taps, right_taps)
        crank_ticks = min(index + 1, group_size - index)
        score += Score(1, buttons=1)  # A opens the group.
        if method == "dpad" or (method == "hybrid" and dpad_taps < crank_ticks):
            score += Score(dpad_taps, dpad=dpad_taps)
        else:
            score += Score(
                crank_ticks,
                crank_ticks=crank_ticks,
                crank_gestures=1,
            )

    confirmations = circle_confirmations(text)
    if no_wait:
        score += Score(confirmations, buttons=confirmations)
    else:
        score += Score(0, waits=confirmations)
    return score

WRAP_LAYOUT = "abcdefghijklmnopqrstuvwxyz-_,."

WRAP_POSITIONS = {
    char: divmod(index, 6) for index, char in enumerate(WRAP_LAYOUT)
}

def torus_distance(first: str, second: str) -> int:
    row_a, col_a = WRAP_POSITIONS[first]
    row_b, col_b = WRAP_POSITIONS[second]
    row_delta = abs(row_a - row_b)
    col_delta = abs(col_a - col_b)
    return min(row_delta, 5 - row_delta) + min(col_delta, 6 - col_delta)

def score_wrap(text: str) -> Score:
    cursor = "e"
    actions = 0
    dpad = 0
    for char in text:
        if char == " ":
            actions += 1  # Short B; cursor stays on the last letter.
            continue
        movement = torus_distance(cursor, char)
        actions += movement + 1
        dpad += movement
        cursor = char
    return Score(actions, dpad)
# Exact cursor coordinates and directional nearest-neighbour rule of shipped ABC.
KEYS='abcdefghijklmnopqrstuvwxyz-_,.0123456789'
def pos(i):
 if i<30:return (33+(i%6)*46,98+(i//6)*18)
 if i<40:return (33+(6+(i-30)%2)*46,98+((i-30)//2)*18)
 return (60+(i-40)*92,193)
def neighbour(i,dx,dy):
 if i>=40 and dx:return i+dx if 40<=i+dx<=43 else i
 x,y=pos(i);best=i;score=10**9
 for j in range(44):
  cx,cy=pos(j);along=(cx-x)*dx if dx else (cy-y)*dy
  if along<=0:continue
  across=abs(cy-y) if dx else abs(cx-x);s=along*10+across*24
  if s<score:best,score=j,s
 return best
def distance(a,b):
 q=deque([(a,0)]);seen={a}
 while q:
  i,n=q.popleft()
  if i==b:return n
  for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
   j=neighbour(i,dx,dy)
   if j not in seen:seen.add(j);q.append((j,n+1))
 raise ValueError('Unreachable')
def abc(text):
 cursor=0;dpad=0
 for c in text:
  target=41 if c==' ' else KEYS.index(c);dpad+=distance(cursor,target);cursor=target
 return {'actions':dpad+len(text),'dpad':dpad,'buttons':len(text)}
def multi(text,crank=False):
 groups=['abcdefgh','ijklmnop','qrstuvwx','yz,.!?-_'];active=None;side=None;dpad=buttons=ticks=waits=0
 for c in text:
  if c==' ':
   if active is not None:waits+=1
   active=side=None;buttons+=1;continue
  g=next(g for g,chars in enumerate(groups) if c in chars);i=groups[g].index(c)
  targetside='crank' if crank else i//4
  if active!=g or side==targetside:
   dpad+=1;active=g
  if crank:ticks+=min(i+1,8-i)
  else:buttons+=i%4+1
  side=targetside
 if active is not None:waits+=1
 return {'actions':dpad+buttons if not crank else None,'dpad':dpad,'buttons':buttons,'crank_ticks':ticks,'auto_commit_waits':waits}
def circle(text,method):
 s=score_circle(text,method)
 return {'actions':s.actions if method=='dpad' else None,'dpad':s.dpad,'buttons':s.buttons,'crank_ticks':s.crank_ticks}
def wrapping(text):
 s=score_wrap(text);return {'actions':s.actions,'dpad':s.dpad,'buttons':s.actions-s.dpad}
scorers={'ABC (shipped 1.0.1)':abc,'Wrap+ prototype':wrapping,'Circle D-pad prototype':lambda t:circle(t,'dpad'),'Circle D-pad (wait per letter)':lambda t:{'actions':score_circle(t,'dpad',False).actions,'waits':score_circle(t,'dpad',False).waits},'Cloud Multi-Tap':multi,'Circle Crank prototype':lambda t:circle(t,'crank'),'Cloud Crank':lambda t:multi(t,True)}
report={'method':'Deterministic optimal actions; not WPM. Lowercase ASCII, no corrections or Done. Circle immediate uses A confirmations; Circle wait-per-letter substitutes 750 ms pauses; Cloud waits only before Space and final commit. Crank tick counts are separate and not comparable as physical actions.','corpora':CORPORA,'results':{}}
for lang,phrases in {'Combined':sum(CORPORA.values(),[]),**CORPORA}.items():
 total={'characters':sum(map(len,phrases)),'phrases':len(phrases),'layouts':{}}
 for name,fn in scorers.items():
  rows=[fn(t) for t in phrases];keys=set().union(*(r.keys() for r in rows));s={k:sum(r.get(k,0) for r in rows) if all(r.get(k,0) is not None for r in rows) else None for k in keys};total['layouts'][name]=s
 baseline=total['layouts']['ABC (shipped 1.0.1)']['actions']
 for s in total['layouts'].values():
  if s['actions'] is not None:s['actions_per_character']=s['actions']/total['characters'];s['reduction_vs_ABC_percent']=100*(1-s['actions']/baseline)
 report['results'][lang]=total
normal=report['results']['Combined']['layouts']['Cloud Multi-Tap']
crank=report['results']['Combined']['layouts']['Cloud Crank']
crank['continuous_turn_gestures']=sum(c!=' ' for phrases in CORPORA.values() for text in phrases for c in text)
crank['control_gestures']=crank['dpad']+crank['buttons']+crank['continuous_turn_gestures']
crank['gesture_reduction_vs_button_presses_percent']=100*(1-crank['control_gestures']/normal['actions'])
crank['selection_step_degrees']=360/8/1.496
crank['total_absolute_rotation_degrees']=crank['crank_ticks']*crank['selection_step_degrees']
crank['time_speedup_percent']=None
report['crank_comparison_note']='339 gestures versus 509 button presses is 33.4% fewer separate control gestures. One continuous turn for a letter counts once despite multiple selection ticks. There are 465 ticks, about 38.9 crank revolutions of absolute rotation. This is not measured time or WPM and does not establish a crank speed advantage.'
assert normal['actions']==509 and report['results']['Combined']['layouts']['ABC (shipped 1.0.1)']['actions']==788
(HERE/'benchmark.json').write_text(json.dumps(report,indent=2))
for lang,row in report['results'].items():
 print(lang,row['characters'],'characters')
 for name,s in row['layouts'].items():print(name,s)
