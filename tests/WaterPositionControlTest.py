import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from water_position_control import strafe_decision, move_to_reference

class PositionControlTest(unittest.TestCase):
    def test_decisions(self):
        self.assertEqual(strafe_decision([0,0,0],[0,0,.75],[1,0,0])[0],'stop')
        self.assertEqual(strafe_decision([0,0,0],[20,0,0],[1,0,0])[:2],('right',100))
        self.assertEqual(strafe_decision([0,0,0],[-1,0,0],[1,0,0])[:2],('left',8))
        for p,t,r in (([],[0,0,0],[1,0,0]),([float('nan'),0,0],[0,0,0],[1,0,0]),
                      ([0,0,0],[0,0,0],[0,0,0]),([0,0,0],[0,0,0],[2,0,0]),
                      ([0,0,0],[101,0,0],[1,0,0]),([0,0,0],[2,2,0],[1,0,0]),
                      ([0,0,0],[0,0,2],[1,0,0])):
            with self.subTest(p=p,t=t,r=r),self.assertRaises(ValueError):strafe_decision(p,t,r)
    def test_controller(self):
        positions=iter(([0,0,0],[11,0,0],[10,0,0]));events=[];pulses=[]
        result=move_to_reference(lambda:next(positions),lambda *p:pulses.append(p),[10,0,0],[1,0,0],events.append)
        self.assertEqual(result,[10,0,0]);self.assertEqual([e['action'] for e in events],['right','left','stop'])
        self.assertEqual(len(pulses),2)
        for budget in (0,31):
            with self.assertRaises(ValueError):move_to_reference(lambda:None,lambda *p:None,[0,0,0],[1,0,0],events.append,budget)
        with self.assertRaises(RuntimeError):move_to_reference(lambda:[0,0,0],lambda *p:None,[10,0,0],[1,0,0],events.append,2)
        def fail(*args):raise OSError('sensor or input failed')
        with self.assertRaises(OSError):move_to_reference(fail,lambda *p:None,[10,0,0],[1,0,0],events.append)
        with self.assertRaises(OSError):move_to_reference(lambda:[0,0,0],fail,[10,0,0],[1,0,0],events.append)

if __name__=='__main__':unittest.main()
