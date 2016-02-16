
from schedulers.easyBackfillNotopo import EasyBackfillNotopo

def list_copy(my_list):
        result = []
        for i in my_list:
            result.append(i)
        return result

# shortest job first 
sjf_sort_key = (
    lambda job :  job.requested_time
)

# this scheduler is similar to the standard easy scheduler. The only diffrence is that 
# the tail of jobs is reordered by shortest job first before trying to backfill jobs in the tail.
  
class  EasySjbfBackfillNotopo(EasyBackfillNotopo):
    """ This algorithm implements the algorithm in the paper of Tsafrir, Etzion, Feitelson, june 2007?
    """


    def _backfill_jobs(self, current_time):
        "Overriding parent method"
        if len(self.unscheduled_jobs) <= 1:
            return []

        result = []  
        first_job = self.unscheduled_jobs[0]        
        tail =  list_copy(self.unscheduled_jobs[1:])
        tail_of_jobs_by_sjf_order = sorted(tail, key=sjf_sort_key)
        
        first_job.start_time = current_time
        self.cpu_snapshot.assignJobEarliest(first_job, current_time)
        
        for job in tail_of_jobs_by_sjf_order:
            if self.cpu_snapshot.canJobStartNow(job, current_time):
                job.is_backfilled = 1
                job.start_time = current_time
                self.unscheduled_jobs.remove(job)
                self.cpu_snapshot.assignJob(job, current_time)
                result.append(job)
                
        self.cpu_snapshot.unAssignJob(first_job)
        first_job.start_time = None

        return result

