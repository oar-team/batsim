
"""

These class represents the schedule without any considerations for the topology. It is an adaptation from the common.py file of the Pyss simulator.

If you want to adapt other scheduler from Pyss to Batsim, here are some tips:
- Pyss does not support topology, so are the scheduling algorithm. Batsim can support topolgy.
- num_required_processors => requested_resources
- predicted_run_time => requested_time

"""






class CpuTimeSlice(object):
    """
    represents a "tentative feasible" snapshot of the cpu between the
    start_time until start_time + dur_time.  It is tentative since a job might
    be rescheduled to an earlier slice. It is feasible since the total demand
    for processors ba all the jobs assigned to this slice never exceeds the
    amount of the total processors available.
    Assumption: the duration of the slice is never changed.
    We can replace this slice with a new slice with shorter duration.
    """

    def __init__(self, free_processors, start_time, duration, total_processors):
        assert duration > 0.0
        assert start_time >= 0.0
        assert total_processors > 0.0
        assert 0.0 <= free_processors <= total_processors

        self.total_processors = total_processors
        self.free_processors = free_processors
        self.job_ids = set()
        #DO NOT MODIFY these 2 attributes
        self.start_time = start_time
        self.duration = duration
        #OR this variable will be out of date:
        self.end_time = self.start_time + self.duration
        #INSTEAD you can use some methods declared later


    def updateStartTimeAndDuration(self, st, dur):
	self.start_time = st
        self.duration = dur
        self.end_time = self.start_time + self.duration

    def updateDuration(self,dur):
        self.duration = dur
        self.end_time = self.start_time + self.duration


    @property
    def busy_processors(self):
        return self.total_processors - self.free_processors

    def addJob(self, job):
        assert job.requested_resources <= self.free_processors, job
        assert job.id not in self.job_ids, "job.id = "+str(job.id)+", job_ids "+str(self.job_ids)
        self.free_processors -= job.requested_resources
        self.job_ids.add(job.id)

    def delJob(self, job):
        assert job.requested_resources <= self.busy_processors, job
        self.free_processors += job.requested_resources
        self.job_ids.remove(job.id)

    def __str__(self):
        return '%d %d %d %s' % (self.start_time, self.duration, self.free_processors, str(len(self.job_ids)))
    def diffstr(self, diff):
        return '%d %d %d %s' % (self.start_time-diff, self.duration, self.free_processors, str(len(self.job_ids)))

    def quick_copy(self): # copy the slice without the set of job_ids
        result = CpuTimeSlice(
                free_processors = self.free_processors,
                start_time = self.start_time,
                duration = self.duration,
                total_processors = self.total_processors,
            )

        return result


    def copy(self):
        result = CpuTimeSlice(
                free_processors = self.free_processors,
                start_time = self.start_time,
                duration = self.duration,
                total_processors = self.total_processors,
            )

        result.job_ids = self.job_ids.copy()

        return result

    def split(self, split_time):
        first = self.copy()
        first.updateDuration(split_time - self.start_time)

        second = self.copy()
        second.updateStartTimeAndDuration(split_time, self.end_time - split_time)

        return first, second

    def __eq__(self, o):
        if type(o) == type(self):
            return self.free_processors == o.free_processors and self.start_time == o.start_time and self.duration == o.duration and self.total_processors == o.total_processors
        else:
            return False
    def __ne__(self, o):
        return not(self.__eq__(o))


class CpuTimeSliceList:

	def __init__(self, obj):
		self.first = obj
		self.last = obj
		obj.list_prev = None
		obj.list_next = None


class CpuSnapshot(object):
    """
    represents the time table with the assignments of jobs to available processors
    """
    # Assumption: the snapshot always has at least one slice
    def __init__(self, total_processors, archive_snapshots):
        self.total_processors = total_processors
        self.slices = CpuTimeSliceList( CpuTimeSlice(self.total_processors, start_time=0, duration=1000, total_processors=total_processors))
        self.archive_of_old_slices=[]
        self.archive_snapshots= archive_snapshots

    @property
    def snapshot_end_time(self):
        return self.slices.last.end_time


    def _ensure_a_slice_starts_at(self, start_time):
        """
        A preprocessing stage.

        Usage:
        First, to ensure that the assignment time of the new added job will
        start at a beginning of a slice.

        Second, to ensure that the actual end time of the job will end at the
        ending of slice.  we need this for adding a new job, or for deleting a tail
        of job when the user estimation is larger than the actual duration.

        The idea: we first append 2 slices, just to make sure that there's a
        slice which ends after the start_time.  We add one more slice just
        because we actually use list.insert() when we add a new slice.
        After that we iterate through the slices and split a slice if needed.
        """

        if self._slice_starts_at(start_time):
            return # already have one

        if start_time < self.snapshot_end_time:
            cur_slice = self.slices.first
            while cur_slice != None:
                 if cur_slice.start_time < start_time < cur_slice.end_time:
                     (first_slice, second_slice) = cur_slice.split(start_time)

                     first_slice.list_prev = cur_slice.list_prev
                     first_slice.list_next = second_slice
                     if cur_slice.list_prev == None:
                          self.slices.first = first_slice
                     else:
                          first_slice.list_prev.list_next = first_slice

                     second_slice.list_prev = first_slice
                     second_slice.list_next = cur_slice.list_next
                     if second_slice.list_next == None:
                          self.slices.last = second_slice
                     else:
                          second_slice.list_next.list_prev = second_slice
                     return
                 cur_slice = cur_slice.list_next
            assert "you should not be there"
            return

        if start_time > self.snapshot_end_time:
            # add slice until start_time
            self._append_time_slice(self.total_processors, start_time - self.snapshot_end_time)
            assert self.snapshot_end_time == start_time

	# add a tail slice, duration is arbitrary whenever start_time >= self.snapshot_end_time
	self._append_time_slice(self.total_processors, 1000)


    def _slice_starts_at(self, time):
        cur_slice = self.slices.first
        while cur_slice != None:
            st = cur_slice.start_time
            if st == time:
                return True
            if st > time:
                return False
            cur_slice = cur_slice.list_next
        return False # no slice found


    def _append_time_slice(self, free_processors, duration):
        obj = CpuTimeSlice(free_processors, self.snapshot_end_time, duration, self.total_processors)
        self.slices.last.list_next = obj
        obj.list_next = None
        obj.list_prev = self.slices.last
        self.slices.last = obj


    def free_processors_available_at(self, time):
        cur_slice = self.slices.first
        while cur_slice != None:
            if cur_slice.start_time <= time < cur_slice.end_time:
                return cur_slice.free_processors
            cur_slice = cur_slice.list_next
        return self.total_processors


    def jobs_at(self, time):
        cur_slice = self.slices.first
        while cur_slice != None:
            if cur_slice.start_time <= time < cur_slice.end_time:
                return cur_slice.job_ids
            cur_slice = cur_slice.list_next
        return set()

    def canJobStartNow(self, job, current_time):
        """
        Do the same that jobEarliestAssignment, but force the start to current_time
        """
        assert job.requested_resources <= self.total_processors, str(self.total_processors)
        time = current_time

        if self.slices.last.free_processors == self.total_processors:
            self.slices.last.updateDuration(time + job.requested_time + 1)
        else:
            self._append_time_slice(self.total_processors, time + job.requested_time + 1)

        partially_assigned = False
        tentative_start_time = accumulated_duration = 0

        assert time >= 0

        s = self.slices.first
        while s != None: # continuity assumption: if t' is the successor of t, then: t' = t + duration_of_slice_t
            #reach the current_time
            if s.start_time < time:
                s = s.list_next
                continue

            if s.end_time <= time or s.free_processors < job.requested_resources:
                # the job can't be assigned to this slice, need to reset
                #the job can't be launch right now
                return False

            elif not partially_assigned:
                # we'll check if the job can be assigned to this slice and perhaps to its successive
                partially_assigned = True
                tentative_start_time =  max(time, s.start_time)
                accumulated_duration = s.end_time - tentative_start_time

            else:
                # job is partially_assigned:
                accumulated_duration += s.duration

            if accumulated_duration >= job.requested_time:
                self.slices.last.updateDuration(1000) # making sure that the last "empty" slice we've just added will not be huge
                return True
            s = s.list_next

        assert False # should never reach here

    def jobEarliestAssignment(self, job, time):
        """
        returns the earliest time right after the given time for which the job
        can be assigned enough processors for job.requested_time unit of
        times in an uninterrupted fashion.
        Assumptions: the given is greater than the submission time of the job >= 0.
        """
        assert job.requested_resources <= self.total_processors, str(self.total_processors)

        self._append_time_slice(self.total_processors, time + job.requested_time + 1)

        partially_assigned = False
        tentative_start_time = accumulated_duration = 0

        max_core_available = 0

        assert time >= 0

        s = self.slices.first
        while s != None: # continuity assumption: if t' is the successor of t, then: t' = t + duration_of_slice_t
            if s.end_time <= time or s.free_processors < job.requested_resources:
                # the job can't be assigned to this slice, need to reset
                # partially_assigned and accumulated_duration
                partially_assigned = False
                accumulated_duration = 0

            elif not partially_assigned:
                # we'll check if the job can be assigned to this slice and perhaps to its successive
                partially_assigned = True
                tentative_start_time =  max(time, s.start_time)
                accumulated_duration = s.end_time - tentative_start_time

            else:
                # job is partially_assigned:
                accumulated_duration += s.duration

            if accumulated_duration >= job.requested_time:
                self.slices.last.updateDuration(1000) # making sure that the last "empty" slice we've just added will not be huge
                return tentative_start_time
            s = s.list_next

        assert False # should never reach here


    def _slices_time_range(self, start, end):
        assert self._slice_starts_at(start), "start time is: " + str(start)
        #assert self._slice_starts_at(end), "end time is: " + str(end) It should work without this... hopefullly...

	s = self.slices.first
	while s != None:
		st = s.start_time
		if st >= end:
			break
		if start <= st:
			yield s
		s = s.list_next


    def delJobFromCpuSlices(self, job):
        """
        Deletes an _entire_ job from the slices.
        Assumption: job resides at consecutive slices (no preemptions), and
        nothing is archived!
        DEPRECATED: see unAssignJob
        """
        for s in self._slices_time_range(job.start_to_run_at_time, job.start_time+job.requested_time):
            s.delJob(job)


    def delTailofJobFromCpuSlices(self, job):
        """
        This function is used when the actual duration is smaller than the
        estimated duration, so the tail of the job must be deleted from the
        slices. We iterate trough the sorted slices until the critical point is found:
        the point from which the tail of the job starts.
        Assumptions: job is assigned to successive slices.
        """
        for s in self._slices_time_range(job.finish_time, job.start_time+job.requested_time):
            s.delJob(job)

    def assignTailofJobToTheCpuSlices(self, job, new_requested_time):
	"""
	This function extends the duration of a job, if the predicted duration is smaller
	than the user estimated duration, then the function adds more slices to the job accordingly.
	"""
	assert isinstance(new_requested_time, int)
	job_estimated_finish_time = job.start_to_run_at_time + new_requested_time
        self._ensure_a_slice_starts_at(job_estimated_finish_time)
        for s in self._slices_time_range(job.start_time+job.requested_time, job_estimated_finish_time):
            s.addJob(job)


    def unAssignJob(self, job):
        """
        unAssigns a job previously assigned.
        """
        for s in self._slices_time_range(job.start_to_run_at_time, job.start_time+job.requested_time):
            s.delJob(job)


    def assignJob(self, job, job_start):
        """
        assigns the job to start at the given job_start time.
        Important assumption: job_start was returned by jobEarliestAssignment.
        """
        assert job.requested_time > 0
        job.start_to_run_at_time = job_start
        self._ensure_a_slice_starts_at(job_start)
        self._ensure_a_slice_starts_at(job_start + job.requested_time)
        for s in self._slices_time_range(job_start, job_start + job.requested_time):
            s.addJob(job)


    def assignJobEarliest(self, job, time):
        starttime = self.jobEarliestAssignment(job, time)
        self.assignJob(job, starttime)
        return starttime


    def archive_old_slices(self, current_time):
        assert self.slices
        self._ensure_a_slice_starts_at(current_time)

	while self.slices.first.end_time <= current_time:
		cur_slice = self.slices.first
		self.slices.first = cur_slice.list_next
		self.slices.first.list_prev = None

		cur_slice.list_prev = None
		cur_slice.list_next = None
		if self.archive_snapshots:
			self.archive_of_old_slices.append(cur_slice)
		else:
			del cur_slice
	self.unify_slices()


    def unify_slices(self):
	assert self.slices

	prev_slice = self.slices.first
	cur_slice = prev_slice.list_next
	while cur_slice != None:
                print cur_slice.start_time, prev_slice.start_time + prev_slice.duration
		#assert cur_slice.start_time == prev_slice.start_time + prev_slice.duration not possible with floats!
		if cur_slice.free_processors == prev_slice.free_processors and cur_slice.job_ids == prev_slice.job_ids:
			prev_slice.updateDuration( prev_slice.duration + cur_slice.duration)
			prev_slice.list_next = cur_slice.list_next
			if cur_slice == self.slices.last:
				self.slices.last = prev_slice
			if cur_slice.list_next != None:
				cur_slice.list_next.list_prev = prev_slice
			cur_slice.list_prev = None
			cur_slice.list_next = None
			del cur_slice
		else:
			prev_slice = cur_slice
		cur_slice = prev_slice.list_next


    def _restore_old_slices(self):
        size = len(self.archive_of_old_slices)
        self.slices = []
        while size > 0:
            size -= 1
            s = self.archive_of_old_slices.pop()
            self.slices.insert(0, s)

    def printCpuSlices(self, str=None):
        if str is not None:
		print str
        print "start time | duration | #free processors | jobs"
	#for s in self.archive_of_old_slices:
	#    print s
	#print("-----------------------------------------------")
        s = self.slices.first
        diff = s.start_time
        print s.diffstr(diff)
        s = s.list_next
        while s != None:
            print s.diffstr(diff)
            assert s.free_processors > s.list_prev
            s = s.list_next
        print


    def copy(self):
        result = CpuSnapshot(self.total_processors, self.archive_snapshots)
        import copy
        result.slices = copy.deepcopy(self.slices)
        return result

    def quick_copy(self):
        assert "NOT YET UPDATED"
        result = CpuSnapshot(self.total_processors, self.archive_snapshots)
        result.slices = [slice.quick_copy() for slice in self.slices]
        return result

    def CpuSlicesTestFeasibility(self):
        assert "NOT YET UPDATED"
        self._restore_old_slices()
        duration = 0
        time = 0

        for s in self.slices:
            prev_duration = duration
            prev_time = time

            if s.free_processors < 0 or s.free_processors > self.total_processors:
                print ">>> PROBLEM: number of free processors is either negative or huge", s
                return False

            if s.start_time != prev_time + prev_duration:
                print ">>> PROBLEM: non successive slices", s.start_time, prev_time
                return False

            duration = s.duration
            time = s.start_time

        return True

    def CpuSlicesTestEmptyFeasibility(self):
        assert "NOT YET UPDATED"
        self._restore_old_slices()
        duration = 0
        time = 0

        for s in self.slices:
            prev_duration = duration
            prev_time = time

            if s.free_processors != self.total_processors:
                print ">>> PROBLEM: number of free processors is not the total processors", s
                return False

            if s.start_time != prev_time + prev_duration:
                print ">>> PROBLEM: non successive slices", s.start_time, prev_time
                return False

            duration = s.duration
            time = s.start_time

        return True

