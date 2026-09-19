"""Bounded positional feedback for the water reference harness (no memory writes)."""
import math


def strafe_decision(position, target, right):
    if any(len(v) != 3 for v in (position,target,right)):
        raise ValueError('Expected three-dimensional vectors')
    if any(not math.isfinite(x) for v in (position,target,right) for x in v):
        raise ValueError('Non-finite position feedback')
    length=math.hypot(right[0],right[1])
    if length < .9 or length > 1.1:
        raise ValueError('Invalid horizontal strafe direction')
    delta=[b-a for a,b in zip(position,target)]
    distance=math.sqrt(sum(x*x for x in delta))
    if distance > 100:
        raise ValueError('Reference target exceeds bounded test movement')
    along=(delta[0]*right[0]+delta[1]*right[1])/length
    cross=abs(delta[0]*right[1]-delta[1]*right[0])/length
    if cross > 1:
        raise ValueError('Target is outside the strafe line')
    if distance <= .75:
        return ('stop',0,distance)
    if abs(along) <= .25:
        raise ValueError('Remaining error cannot be corrected by strafing')
    return ('right' if along>0 else 'left',min(100,max(8,int(abs(along)*5))),distance)


def move_to_reference(observe, pulse, target, right, record, max_steps=30):
    if max_steps < 1 or max_steps > 30:
        raise ValueError('Invalid movement budget')
    for step in range(max_steps):
        position=observe()
        action,milliseconds,distance=strafe_decision(position,target,right)
        record({'step':step,'position':position,'action':action,'milliseconds':milliseconds,'distance':distance})
        if action=='stop':
            return position
        pulse(action,milliseconds)
    raise RuntimeError('Reference position not reached within movement budget')
