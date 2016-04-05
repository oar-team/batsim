



class Interval(object):
    def __init__(self, start, end, state, prev, next):
        assert start<=end
        self.start = start
        self.end = end
        self.state = state
        self.prev = prev
        self.next = next
    def __repr__(self):
        return "<Interval "+str(self.state)+" ["+str(self.start)+"-"+str(self.end)+"]>"


class IntervalOfStates(object):
    def __init__(self, minn, maxx, startingState):
        self.first_item = Interval(minn, maxx, startingState, None, None)
        self.callbacks = {}
        self.minn = minn
        self.maxx = maxx

    def printme(self):
        item = self.first_item
        while not(item is None):
            print item
            item = item.next

    def registerCallback(self, fromState, toState, callback):
        self.callbacks[(fromState, toState)] = callback


    def callCallback(self, start, end, fromState, toState, optCallBack):
        #print "()() callCallback [", start, "-", end, "] ", fromState, "=>", toState
        try:
            func =  self.callbacks[(fromState, toState)]
        except KeyError:
            return
        func(optCallBack, start, end, fromState, toState)

    def changeState(self, start, end, toState, optCallBack=None):
        """
        changeState from start to end to toState
        callbacks are called after all the modifications
        """
        assert start>=self.minn
        assert end<=self.maxx
        
        #print "==== changeState", start, end, toState
        callbacks_to_call = []
        #find first item
        item = self.first_item
        while not(item is None):
            if start <= item.end:
                break
            item = item.next
        #print "item", item
        
        if item.start == start and not(item.prev is None) and item.prev.state == toState:
            item = item.prev
        
        if item.state == toState:
            #print "grow existing item"
            newInter = Interval(item.start, end, toState, item.prev, None)
            prev_item = item.prev
        elif item.start == start:
            #print "change state, keep same inter"
            newInter = Interval(item.start, end, toState, item.prev, None)
            prev_item = item.prev
        else:
            #print "split item"
            newInter = Interval(start, end, toState, item, None)
            prev_item = item
        
        if prev_item is None:
            item = item
        else:
            item = prev_item
        while not(item is None):
            if end == item.end and not(item.next is None) and item.next.state == toState:
                #if the next item have the same state
                # we will stop the search on the next item
                # to then grow newInterval upto the next item
                pass
            elif end <= item.end:
                break
            s = max(item.start, start)
            e = min(item.end, end)
            if s <= e:
                callbacks_to_call.append((s, e, item.state, toState, optCallBack))
            
            item = item.next
        
        #print "next item", item
        if item.end == end:
            #print "just delete it"
            newInter.next = item.next
            if not(item.next is None):
                item.next.prev = newInter
            nextInter = item.next
        else:
            if item.state == toState:
                #print "grow"
                newInter.end = item.end
                newInter.next = item.next
                if not(item.next is None):
                    item.next.prev = newInter
            else:
                #print "split"
                nextInter = Interval(end+1, item.end, item.state, newInter, item.next)
                if not(item.next is None):
                    item.next.prev = nextInter
                
                newInter.next = nextInter
        
        s = max(item.start, start)
        e = min(item.end, end)
        if s <= e:
            callbacks_to_call.append((s, e, item.state,toState, optCallBack))
        
        
        #insert new item in the list compeltly
        if prev_item is None:
            #print "prev_item is None"
            self.first_item = newInter
        else:
            #print "update prev_item"
            prev_item.next = newInter
            prev_item.end = min(start-1, prev_item.end)
        
        #call all the callbacks
        for (s,e,fst,tst,o) in callbacks_to_call:
                self.callCallback(s,e,fst,tst,o)
        


    def contains(self, start, end, state):
        #print "==== contains", start, end, state
        item = self.first_item
        while not(item is None):
            if item.start <= start and end <= item.end and item.state == state:
                return True
            if start < item.start:
                return False
            item = item.next
        return False



if __name__ == '__main__':
    
    from enum import Enum

    class State(Enum):
        SwitchedON = 0
        SwitchedOFF = 1
        ToSwitchOnWhenSwitchedOFF = 2
        SwitchingON = 3
        SwitchingOFF = 4

    
    expecting_state = []
    def testcallback(opt, start, end, fromState, toState):
        #print "()() callCallback [", start, "-", end, "] ", fromState, "=>", toState
        s = expecting_state.pop(0)
        assert s == (start, end, fromState, toState), "Expecting "+str(s)+" HAD "+str((start, end, fromState, toState))
    
    l = IntervalOfStates(0,100,State.SwitchedON)
    
    for i in State:
        for j in State:
            l.registerCallback(i, j, testcallback)
    
    #print "-----------------"
    #l.printme()
    #print "-----------------"
    
    
    expecting_state.append((0, 0, State.SwitchedON, State.SwitchedOFF))
    l.changeState(0, 0, State.SwitchedOFF)
    
    expecting_state.append((0, 0, State.SwitchedOFF, State.SwitchedON))
    l.changeState(0, 0, State.SwitchedON)
    
    
    expecting_state.append((100, 100, State.SwitchedON, State.SwitchedOFF))
    l.changeState(100, 100, State.SwitchedOFF)
    
    expecting_state.append((100, 100, State.SwitchedOFF, State.SwitchedON))
    l.changeState(100, 100, State.SwitchedON)
    
    
    expecting_state.append((10, 50, State.SwitchedON, State.SwitchedOFF))
    l.changeState(10, 50, State.SwitchedOFF)
    expecting_state.append((45, 50, State.SwitchedOFF, State.SwitchingOFF))
    expecting_state.append((51, 70, State.SwitchedON, State.SwitchingOFF))
    l.changeState(45, 70, State.SwitchingOFF)
    expecting_state.append((45, 70, State.SwitchingOFF, State.SwitchingON))
    l.changeState(45, 70, State.SwitchingON)
    
    
    
    expecting_state.append((5, 9, State.SwitchedON, State.ToSwitchOnWhenSwitchedOFF))
    expecting_state.append((10, 44, State.SwitchedOFF, State.ToSwitchOnWhenSwitchedOFF))
    expecting_state.append((45, 70, State.SwitchingON, State.ToSwitchOnWhenSwitchedOFF))
    expecting_state.append((71, 95, State.SwitchedON, State.ToSwitchOnWhenSwitchedOFF))
    l.changeState(5, 95, State.ToSwitchOnWhenSwitchedOFF)
    
    
    
    expecting_state.append((5, 95, State.ToSwitchOnWhenSwitchedOFF, State.SwitchedON))
    l.changeState(5, 95, State.SwitchedON)
    
    
    
    expecting_state.append((0, 100, State.SwitchedON, State.SwitchedON))
    l.changeState(0, 100, State.SwitchedON)
    
    
    expecting_state.append((10, 20, State.SwitchedON, State.ToSwitchOnWhenSwitchedOFF))
    l.changeState(10, 20, State.ToSwitchOnWhenSwitchedOFF)
    expecting_state.append((30, 40, State.SwitchedON, State.ToSwitchOnWhenSwitchedOFF))
    l.changeState(30, 40, State.ToSwitchOnWhenSwitchedOFF)
    
    expecting_state.append((21, 29, State.SwitchedON, State.ToSwitchOnWhenSwitchedOFF))
    l.changeState(21, 29, State.ToSwitchOnWhenSwitchedOFF)
    
    expecting_state.append((50, 60, State.SwitchedON, State.ToSwitchOnWhenSwitchedOFF))
    l.changeState(50, 60, State.ToSwitchOnWhenSwitchedOFF)
    
    expecting_state.append((45, 49, State.SwitchedON, State.SwitchingON))
    expecting_state.append((50, 55, State.ToSwitchOnWhenSwitchedOFF, State.SwitchingON))
    l.changeState(45, 55, State.SwitchingON)
    
    
    assert len(expecting_state) == 0
    
    #l.printme()
    assert l.contains(45,55,State.SwitchingON) == True
    assert l.contains(42,42,State.SwitchedON) == True
    assert l.contains(45,55,State.SwitchingOFF) == False
    assert l.contains(30,70,State.SwitchingOFF) == False
    
