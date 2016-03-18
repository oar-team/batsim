#/usr/bin/python3

from batsim.batsim import BatsimScheduler
from schedulers.common_pyss_adaptator import CpuSnapshot

import sys
import os
from random import sample
from sortedcontainers import SortedSet
from sortedcontainers import SortedListWithKey
import copy


"""
An Easy Backfill scheduler that care a little about topology.
This scheduler consider job as rectangle.


"""




INFINITY = float('inf')


class FreeSpace(object):
    def __init__(self, first_res, last_res, len, p, n):
        assert first_res <= last_res
        self.first_res = first_res
        self.last_res = last_res
        self.res = last_res-first_res+1
        self.length = len
        self.prev = p
        self.nextt = n
        self.allocSmallestResFirst = True
    def __repr__(self):
        if self.prev is None:
            p = "|"
        else:
            p = "<"
        if self.nextt is None:
            n = "|"
        else:
            n = ">"
        if hasattr(self, "linkedTo"):
            link = "L"
        else:
            link = ""
        if hasattr(self, "removed"):
            delet = "NOTinLIST"
        else:
            delet = ""
        if self.allocSmallestResFirst:
            asrf1 = "*"
            asrf2 = " "
        else:
            asrf1 = " "
            asrf2 = "*"
        return "<<FreeSpace ["+str(self.first_res)+"-"+str(self.last_res)+"] "+str(self.length)+" "+link+" \t"+asrf1+p+n+asrf2+" "+delet+"    >>"
    

class FressSpaceContainer(object):
    """
    Developers, NEVER FORGET:
    - operations on the list can be done will iterating on it with the generator()
    - when on BF mode, 2 consecutives items can have the same first_res or last_res.
    """
    def __init__(self, total_processors):
        self.firstItem = FreeSpace(0, total_processors-1, INFINITY, None, None)
        self.free_processors = total_processors

    def generator(self):
        curit = self.firstItem
        while curit is not None:
            yield curit
            curit = curit.nextt

    def remove(self, item):
        prev = item.prev
        nextt = item.nextt
        if item == self.firstItem:
            self.firstItem = nextt
        else:
            assert prev is not None, "The  self.firstItem ("+str(self.firstItem)+") should be set to "+str(item)+", but its not!"
            prev.nextt = nextt
        if nextt is not None:
            nextt.prev = prev
        #if someone hold a direct refecence to item, it can knwo if this item have been removed from the list
        item.removed = True
    
    def _assignJobBeginning(self, l, job):
            alloc = (l.first_res, l.first_res+job.requested_resources-1)
            l.first_res = l.first_res+job.requested_resources
            l.res = l.last_res-l.first_res+1
            assert l.res>=0
            if l.res == 0:
                self.remove(l)
            return alloc
    
    def _assignJobEnding(self, l, job):
            alloc = (l.last_res-job.requested_resources+1, l.last_res)
            l.last_res = l.last_res-job.requested_resources
            l.res = l.last_res-l.first_res+1
            assert l.res>=0
            if l.res == 0:
                self.remove(l)
            return alloc
    
    def assignJob(self, l, job, current_time):
        assert job.requested_resources <= l.res
        #TODO:here we can alloc close to job that will end as the same time as the current job
        if l.allocSmallestResFirst:
            alloc = self._assignJobBeginning(l, job)
            #remove the resources of the linked FreeSpace (see allocFutureJob)
            if hasattr(l, "linkedTo"):
                if l.linkedTo.first_res <= alloc[1]:
                    l.linkedTo.first_res = alloc[1]+1
                
        else:
            alloc = self._assignJobEnding(l, job)
            #remove the resources of the linked FreeSpace (see allocFutureJob)
            if hasattr(l, "linkedTo"):
                if  l.linkedTo.last_res >= alloc[0]:
                    l.linkedTo.last_res = alloc[0]-1
        job.estimate_finish_time = job.requested_time + current_time
        job.alloc = alloc
        
        if hasattr(l, "linkedTo"):
            l.linkedTo.res = l.linkedTo.last_res-l.linkedTo.first_res+1
            #assert l.linkedTo.res>=0
            if l.linkedTo.res <= 0:
                self.remove(l.linkedTo)

        self.free_processors -= job.requested_resources

        return alloc

    def _findSurroundingFreeSpaces(self, job):
        prev_fspc = None
        for fspc in self.generator():
            if fspc.first_res > job.alloc[0]:
                return (prev_fspc, fspc)
            prev_fspc = fspc
        #prev_fspc = last fspc
        return (prev_fspc, None)
        
    def unassignJob(self, job):
        self.free_processors += job.requested_resources
        
        (l1, l2) = self._findSurroundingFreeSpaces(job)
        
        #merge with l1?
        mergel1 = ((l1 is not None) and (l1.last_res+1 == job.alloc[0]))
        mergel2 = ((l2 is not None) and (l2.first_res-1 == job.alloc[-1]))
        if mergel1 and mergel2:
            #merge l2 into l1
            l1.nextt = l2.nextt
            if l1.nextt is not None:
                l1.nextt.prev = l1
            l1.last_res = l2.last_res
            l1.res = l1.last_res-l1.first_res+1
            return l1
        elif mergel1:
            #increase l1 size
            l1.last_res = l1.last_res + job.requested_resources
            l1.res = l1.last_res-l1.first_res+1
            #we will alloc jobs close to where the last job were scheduled
            l1.allocSmallestResFirst = False
            return l1
        elif mergel2:
            #increase l2 size
            l2.first_res = l2.first_res - job.requested_resources
            l2.res = l2.last_res-l2.first_res+1
            #we will alloc jobs close to where the last job were scheduled
            l2.allocSmallestResFirst = True
            return l2
        else:
            #create a new freespace
            lnew = FreeSpace(job.alloc[0], job.alloc[-1], INFINITY, l1, l2)
            
            if l1 is None:
                self.firstItem = lnew
            else:
                l1.nextt = lnew
            if l2 is not None:
                l2.prev = lnew
            return lnew
        assert False
        
    def printme(self):
        print "-------------------"
        for l in self.generator():
            print str(l)
        print "-------------------"


    def insertNewFreeSpaceAfter(self, first_res, last_res, len, l):
        newfs = FreeSpace(first_res, last_res, len, l, l.nextt)
        
        if l.nextt is not None:
            l.nextt.prev = newfs
        
        l.nextt = newfs
        
        return newfs





class EasyBackfill(BatsimScheduler):
    """
    An EASY backfill scheduler that schedule rectangles.
    """

    def __init__(self, options):
        self.options = options

    def onAfterBatsimInit(self):
        self.listFreeSpace = FressSpaceContainer(self.bs.nb_res)

        self.listRunningJob = SortedListWithKey(key=lambda job: job.estimate_finish_time)
        self.listWaitingJob = []



    def onJobSubmission(self, just_submitted_job):
        current_time = self.bs.time()
        self.listWaitingJob.append(just_submitted_job)
        #if (self.cpu_snapshot.free_processors_available_at(current_time) >= just_submitted_job.requested_resources):
        self._schedule_jobs(current_time)

    def onJobCompletion(self, job):
        current_time = self.bs.time()
        
        self.listFreeSpace.unassignJob(job)
        self._removeAjobFromRunningJob(job)
        job.finish_time = current_time
        
        self._schedule_jobs(current_time)

    def _removeAjobFromRunningJob(self, job):
        #because we use "key", .remove does not work as intended
        for (i,j) in zip(range(0, len(self.listRunningJob)), self.listRunningJob):
            if j == job:
                del self.listRunningJob[i]
                return
        raise "Job not found!"
        

    def _schedule_jobs(self, current_time):
        
        allocs = self.allocHeadOfList(current_time)
        
        if len(self.listWaitingJob) > 1:
            first_job = self.listWaitingJob.pop(0)
            allocs += self.allocBackFill(first_job, current_time)
            self.listWaitingJob.insert(0, first_job)
        
        if len(allocs) > 0:
            self.bs.start_jobs_continuous(allocs)


    def allocJobFCFS(self, job, current_time):
        for l in self.listFreeSpace.generator():
            if job.requested_resources <= l.res:
                alloc = self.listFreeSpace.assignJob(l, job, current_time)
                return alloc
        return None

    def allocJobBackfill(self, job, current_time):
        """
        The same as algo as allocJobFCFS BUT we check for the length of the job because there can be a reservation (the "firstjob" of the backfilling).
        """
        for l in self.listFreeSpace.generator():
            if job.requested_resources <= l.res and job.requested_time <= l.length:
                alloc = self.listFreeSpace.assignJob(l, job, current_time)
                return alloc
        return None


    def allocHeadOfList(self, current_time):
        allocs = []
        while len(self.listWaitingJob) > 0:
            alloc = self.allocJobFCFS(self.listWaitingJob[0], current_time)
            if alloc is None:
                break
            job = self.listWaitingJob.pop(0)
            job.start_time = current_time
            self.listRunningJob.add(job)
            allocs.append( (job, alloc) )
        return allocs


    def findAllocFuture(self, job):
        #rjobs = sort(self.listRunningJob, by=estimate_finish_time) automaticly done by SortedList
        listFreeSpaceTemp = copy.deepcopy(self.listFreeSpace)
        for j in self.listRunningJob:
            new_free_space_created_by_this_unallocation = listFreeSpaceTemp.unassignJob(j)
            if job.requested_resources <= new_free_space_created_by_this_unallocation.res:
                alloc = listFreeSpaceTemp.assignJob(new_free_space_created_by_this_unallocation, job, j.estimate_finish_time)
                #we find a valid allocation
                return (alloc, j.estimate_finish_time)
        #if we are it means that the job will never fit in the cluster!
        assert False
        return None


    def allocFutureJob(self, first_job_res, first_job_starttime, current_time):
        """
        Update self.listFreeSpace to insert (if needed) 2 virtual free space. These freespaces need to be removes afterwards
        Example: 3 machine (A,B,C), 1 job running (1), the firstjob is 2
        A|
        B|     22
        C|1111122
        -'------------>
        A FreeSpace (A,B, INFINITY) exists, we replace it with 2 FreeSpaces (A,A, INFINITY) and (A,B,first_job_starttime)
        
        These 2 new FreeSpaces are "linked", in order to modify one freeSpace when the other is modified.
        
        TODO: here we can optimise the code. The free sapces that we are looking for are already been found in findAllocFuture(). BUT, in this function we find the FreeSpaces of listFreeSpaceTemp not self.listFreeSpace; so a link should be made betweend these 2 things.
        """
        first_virtual_space = None
        first_shortened_space = None
        second_virtual_space = None
        second_shortened_space = None
        for l in self.listFreeSpace.generator():
            if l.first_res == first_job_res[0]:
                assert first_virtual_space is None and first_shortened_space is None
                
                first_shortened_space = l
                l.length = first_job_starttime-current_time
                
            elif l.first_res < first_job_res[0] and l.last_res >= first_job_res[0]:
                #we transform this free space as 2 free spaces, the wider rectangle and the longest rectangle
                assert first_virtual_space is None and first_shortened_space is None
                
                first_virtual_space = self.listFreeSpace.insertNewFreeSpaceAfter(l.first_res, first_job_res[0]-1, INFINITY, l)
                
                first_virtual_space.linkedTo = l
                l.linkedTo = first_virtual_space
                
                first_virtual_space.allocSmallestResFirst = True
                l.allocSmallestResFirst = False
                
                first_shortened_space = l
                l.length = first_job_starttime-current_time
            
            if l.last_res == first_job_res[-1]:
                assert second_virtual_space is None and second_shortened_space is None
                
                second_shortened_space = l
                l.length = first_job_starttime-current_time
            elif l.first_res <= first_job_res[-1] and l.last_res > first_job_res[-1]:
                #we transform this free space as 2 free spaces, the wider rectangle and the longest rectangle
                assert second_virtual_space is None and second_shortened_space is None
                
                second_virtual_space = self.listFreeSpace.insertNewFreeSpaceAfter(first_job_res[-1]+1, l.last_res, INFINITY, l)
                
                second_virtual_space.linkedTo = l
                l.linkedTo = second_virtual_space
                
                second_virtual_space.allocSmallestResFirst = False
                l.allocSmallestResFirst = True
                
                second_shortened_space = l
                l.length = first_job_starttime-current_time
                #no need to continue
                break
        return (first_virtual_space, first_shortened_space, second_virtual_space, second_shortened_space)
        



    def findBackfilledAllocs(self, current_time, first_job_starttime):
        allocs = []
        jobsToRemove = []
        for j in self.listWaitingJob:
            alloc = self.allocJobBackfill(j, current_time)
            if alloc is not None:
                allocs.append( (j, alloc) )
                j.start_time = current_time
                jobsToRemove.append(j)
                self.listRunningJob.add(j)
        
        for j in jobsToRemove:
            self.listWaitingJob.remove(j)
        
        return allocs




    def allocBackFill(self, first_job, current_time):
        
        
        (first_job_res, first_job_starttime) = self.findAllocFuture(first_job)
        
        (first_virtual_space, first_shortened_space, second_virtual_space, second_shortened_space) = self.allocFutureJob(first_job_res, first_job_starttime, current_time)
        
        allocs = self.findBackfilledAllocs(current_time, first_job_starttime)
        
        if first_virtual_space is not None:
            del first_virtual_space.linkedTo.linkedTo
            del first_virtual_space.linkedTo
            if not hasattr(first_virtual_space, "removed"):
                self.listFreeSpace.remove(first_virtual_space)
        if first_shortened_space is not None:
            first_shortened_space.length = INFINITY
            first_shortened_space.allocSmallestResFirst = True
        if second_virtual_space is not None:
            del second_virtual_space.linkedTo.linkedTo
            del second_virtual_space.linkedTo
            if not hasattr(second_virtual_space, "removed"):
                self.listFreeSpace.remove(second_virtual_space)
        if second_shortened_space is not None:
            second_shortened_space.length = INFINITY
            second_shortened_space.allocSmallestResFirst = True
        
        return allocs

    def assert_listFreeSpace_listRunningJob(self):
        len_fp = sum(l.res for l in self.listFreeSpace.generator())
        len_rj = sum(j.requested_resources for j in self.listRunningJob)
        assert len_fp + len_rj == self.bs.nb_res, "INCOHERENT freespaces:"+str(len_fp)+" jobs:"+str(len_rj)+" tot:"+str(self.bs.nb_res)

