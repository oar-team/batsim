

INFINITY = float('inf')

class Interval(object):
    def __init__(self, begin, end, prev, next):
        assert begin<=end
        self.begin = begin
        self.end = end
        self.prev = prev
        self.next = next
    def __repr__(self):
        return "<Interval ["+str(self.begin)+"-"+str(self.end)+"]>"


class IntervalContainer(object):
    def __init__(self):
        self.first_item = None
    
    def addInterval(self, rmFrom, rmTo):
        if self.first_item is None:
            self.first_item = Interval(rmFrom,rmTo,None,None)
            return
        
        #find the first elm
        item = None
        nextitem = self.first_item
        while not(nextitem is None):
            if rmFrom < nextitem.begin:
                break
            item = nextitem
            nextitem = nextitem.next
        #if there is no intersections with exisiting intervals
        newitem = item is None and (rmTo < nextitem.begin-1)
        if not(item is None):
            newitem = newitem or (item.end+1 < rmFrom and nextitem is None) or \
            (not(nextitem is None) and item.end+1 < rmFrom and rmTo < nextitem.begin-1)
        if newitem:
            i = Interval(rmFrom,rmTo,item,nextitem)
            if not(item is None):
                item.next = i
            else:
                self.first_item = i
            if not(nextitem is None):
                nextitem.prev = i
            return
        if not(item is None) and rmFrom <= item.end+1:
            first_intersection = item
        else:
            first_intersection = nextitem
            nextitem.begin = rmFrom
        
        last_intersection = first_intersection
        nextlast_intersection = first_intersection.next
        while not(nextlast_intersection is None):
            if rmTo < nextlast_intersection.begin-1:
                break
            last_intersection = nextlast_intersection
            nextlast_intersection = nextlast_intersection.next
        
        first_intersection.end = max(last_intersection.end, rmTo)
        first_intersection.next = nextlast_intersection
        if not(nextlast_intersection is None):
            nextlast_intersection.prev = first_intersection
        
        return
        
    def removeInterval(self, rmFrom, rmTo):
        """
        remove from rmFrom to rmTo INCLUDED:
        removing (5,7) in (1,10) will produce (1,4),(8,10)
        """
        assert rmFrom <= rmTo
        item = self.first_item
        while not(item is None):
            if rmFrom <= item.begin and item.end <= rmTo:
                #remove item
                if item.prev is None:
                    self.first_item = item.next
                else:
                    item.prev.next = item.next
                if item.next is None:
                    pass
                else:
                    item.next.prev = item.prev
            elif  item.begin < rmFrom and rmTo < item.end:
                #split item
                newInter = Interval(rmTo+1, item.end, item, item.next)
                if not(newInter.next is None):
                    newInter.next.prev = newInter
                item.next = newInter
                item.end = rmFrom-1
                return
            elif rmFrom <= item.begin and item.begin <= rmTo:
                item.begin = rmTo+1
                return
            elif rmFrom <= item.end and item.end <= rmTo:
                item.end = rmFrom-1
            
            item = item.next
    
    
    
    def intersection(self, rmFrom, rmTo):
        """
        intersection from rmFrom to rmTo INCLUDED
        """
        assert rmFrom <= rmTo
        item = self.first_item
        ret = []
        while not(item is None):
            if rmFrom <= item.begin and item.end <= rmTo:
                ret.append( (item.begin, item.end) )
            elif  item.begin < rmFrom and rmTo < item.end:
                ret.append( (rmFrom, rmTo) )
                return ret
            elif rmFrom <= item.begin and item.begin <= rmTo:
                ret.append( (item.begin, rmTo) )
                return ret
            elif rmFrom <= item.end and item.end <= rmTo:
                ret.append( (rmFrom, item.end) )
            
            item = item.next
        return ret
    
    
    def difference(self, rmFrom, rmTo):
        """
        return the part of (rmFrom, rmTo) that is not in the container
        """
        assert rmFrom <= rmTo
        previtem = None
        item = self.first_item
        ret = []
        while not(item is None):
            if previtem is None and rmFrom < item.begin:
                ret.append( (rmFrom, min(item.begin-1, rmTo)) )
            elif previtem is None:
                pass
            elif rmTo <= previtem.end:
                return ret
            elif rmFrom < item.begin and item.begin <= rmTo:
                ret.append( (max(rmFrom, previtem.end+1), item.begin-1) )
            elif previtem.end < rmTo and rmTo < item.begin:
                ret.append( (max(rmFrom, previtem.end+1), rmTo) )
                return ret
            
            previtem = item
            item = item.next
        if previtem is None:#and item is None
            return [(rmFrom, rmTo)]
        if not(previtem is None) and previtem.end < rmTo:
            ret.append( (max(previtem.end+1, rmFrom), rmTo) )
            
        return ret



    def printme(self):
        item = self.first_item
        while not(item is None):
            print item
            item = item.next




if __name__ == '__main__':
    l = IntervalContainer()
    l.printme()
    print "---------------"
    l.addInterval(13,13)
    l.addInterval(15,15)
    l.addInterval(14,14)
    print l.intersection(13,15) == [(13,15)]
    l = IntervalContainer()
    l.addInterval(14,14)
    l.addInterval(13,13)
    l.addInterval(15,15)
    print l.intersection(13,15) == [(13,15)]
    l = IntervalContainer()
    print "---------------"
    #print l.difference(12, 24) == [(12, 24)]
    l.addInterval(10,20)
    l.addInterval(30,40)
    l.addInterval(50,60)
    l.printme()
    print l.difference(0, 0) == [(0, 0)]
    print l.difference(70, 80) == [(70, 80)]
    print l.difference(15, 45) == [(21, 29), (41, 45)]
    print l.difference(5, 65) == [(5, 9), (21, 29), (41, 49), (61, 65)]
    print l.difference(10, 20) == []
    print l.difference(20, 50) == [(21, 29), (41, 49)]

    print "---------------"
    print l.intersection(10,30)
    print l.intersection(100,300)
    l.removeInterval(10,20)
    l.printme()
    print "---------------"
    l.addInterval(10,20)
    
    l.addInterval(50,110)
    l.addInterval(35,45)
    l.printme()
    
    
    
    
