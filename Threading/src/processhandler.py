import multiprocessing
import threading
import Queue
import signal, os
import time

MAX_PRIORITYS = 4

"""
Runs a callable with given arguments and put the results on a queue.

@param queue	The queue to put the results on
@param target	The callable to run
@param args	Arguments for the callable
@param kwargs	Keyword arguments for the callable
"""
def resultPacker(queue, target, args=(), kwargs={}):
	result = target(*args, **kwargs)
	queue.put(result)


class ProcessHandler:
	maxProcesses = 7
	lock = threading.Lock()
	waitingProcesses = [[] for i in range(MAX_PRIORITYS)]
	# TODO Remove this conditional shit
	conditionalProcesses = [[] for i in range(MAX_PRIORITYS)]
	pausedProcesses = [[] for i in range(MAX_PRIORITYS)]
	activeProcesses = [[] for i in range(MAX_PRIORITYS)]

	def __init__(self, maxProcesses=7):
		self.maxProcesses = maxProcesses

	"""
	Counts the amount of currently running processes

	@return	the amount of running processes
	"""
	def activeCount(self):
		count = 0
		for pros in self.activeProcesses:
			count += len(pros)
		return count

	# TODO Remove this method
	"""
	Checks if all given processes are stopped.

	@param processes	List of processes to check

	@return			True if all processes are done, False otherwise
	"""
	def checkDone(self, processes):
		for pro in processes:
			if pro.exitcode == None:
				return False
		return True

	"""
	Removes all finished processes from the list
	"""
	def removeFinished(self):
		for active in self.activeProcesses:
			removed = []
			for p in active:
				if p.exitcode != None:
					removed.append(p)
			for p in removed:
				active.remove(p)
				print "Removed:", p.name

	"""
	Tries to free a process for a given priority

	@param neededPriority	The priority of the process that needs the resource

	@return			True if a process resource is free/was freed, False otherwise
	"""
	def freeProcess(self, neededPriority):
		#self.removeFinished()
		# Are there free resources?
		if self.activeCount() < self.maxProcesses:
			return True
		# If there is no Process of this priority running one should get freed not matter what
		if len(self.activeProcesses[neededPriority]) < 1:
			neededPriority = MAX_PRIORITYS + 1

		for (priority,(paused,active)) in enumerate(zip(self.pausedProcesses, self.activeProcesses)):
			# Not important enough?
			if neededPriority <= priority:
				return False
			# Only stop a process if there is still one of the same priority running
			if len(active) > 1:
				toPause = active.pop(0)
				try:
					os.kill(toPause.pid, signal.SIGSTOP)
				except OSError, e:
					print "Tried to pause process but it's already gone?", toPause.pid
					return True
				paused.append(toPause)
				print "Pause:", toPause.name
				return True
		return False

	"""
	Tries to start new processes and pauses other ones if needed
	"""
	def update(self):
		self.lock.acquire()
		try:
			self.removeFinished()
			for (priority,(waiting,paused,active)) in reversed(list(enumerate(zip(self.waitingProcesses, self.pausedProcesses, self.activeProcesses)))):
				# Try to continue processes
				while len(paused) > 0:
					if not self.freeProcess(priority):
						break
					toStart = paused.pop(0)
					os.kill(toStart.pid, signal.SIGCONT)
					active.append(toStart)
					print "Continue:", toStart.name
				# Try to start new processes
				while len(waiting) > 0:
					if not self.freeProcess(priority):
						break
					toStart = waiting.pop(0)
					toStart.start()
					active.append(toStart)
					print "Start:", toStart.name
			pro = self.activeProcesses
			print len(pro[0]), len(pro[1]), len(pro[2]), len(pro[3])
		finally:
			self.lock.release()

	"""
	Waits for the queue and then let's a callable deal with the result

	@param queue		Queue wich will get one result put on
	@param worker		Callable that can work with this result
	"""
	def threadWorker(self, queue, worker, process):
		res = queue.get()
		#self.lock.acquire()
		#try:
		#	for active in self.activeProcesses:
		#		if active.count(process) > 0:
		#			active.remove(process)
		#			print "I Removed:", process.name
		#			break
		#finally:
		#	self.lock.release()
		#self.update()
		worker(res)

	"""
	Run a task in it's own process and executes another callable on the result
	in an own thread

	@param priority		Priority of the task
	@param worker		Callable that will deal with the result of target
	@param target		Callable that represents the task
	@param args		Arguments for target
	@param kwargs		Keyword arguments for target
	@param name		Name of the process

	@return			A pair of the thread and the process
	"""
	def runTaskHidden(self, priority, worker, target, args=(), kwargs={}, name=None):
		(queue,process) = self.addTask(priority, target, args, kwargs, name)
		thread = threading.Thread(target=self.threadWorker, args=(queue,worker, process))
		thread.start()
		return (thread,process)

	"""
	Run a task in it's own process and wait for the result

	@param priority		Priority of the task
	@param target		Callable that represents the task
	@param args		Arguments for target
	@param kwargs		Keyword arguments for target
	@param name		Name of the process

	@return			The result of target
	"""
	def runTask(self, priority, target, args=(), kwargs={}, name=None):
		res = self.addTask(priority, target, args, kwargs, name)[0].get()
		self.update()
		return res

	"""
	Adds a task and returns a queue which will recive the result
	It's recommented to call update() after you recived the result

	@param priority		Priority of the task
	@param target		Callable that represents the task
	@param args		Arguments for target
	@param kwargs		Keyword arguments for target
	@param name		Name of the process
	@param results		Multiprocessing Queue for the results

	@return			A pair of the result queue and the process object
	"""
	def addTask(self, priority, target, args=(), kwargs={}, name=None, results=multiprocessing.Queue()):
		p = multiprocessing.Process(target=resultPacker, args=(results, target, args, kwargs), name=name)
		self.addProcess(p, priority)
		return (results,p)

	"""
	Shoots a process

	@param process		The process that should get stopped
	"""
	def stopProcess(self, process):
		try:
			os.kill(process.pid, signal.SIGKILL)
		except OSError, e:
			print "Tried to shoot process but it's already gone?", process.pid
		

	"""
	Adds a process with a given priority

	@param process		The process to be started
	@param priority		The priority of the process
	"""
	def addProcess(self, process, priority):
		if priority >= MAX_PRIORITYS or priority < 0:
			raise "Fuckedup Priority"
		# Add process to the wainting list
		self.lock.acquire()
		try:
			self.waitingProcesses[priority].append(process)
		finally:
			self.lock.release()
		# Update the running processes
		self.update()


def fib(n):
	if n <= 2:
		return n
	return fib(n-1) + fib(n-2)

def printer(toPrint):
	y = toPrint
	print "\tPrinter:", toPrint

def workeri():
	#print "Begin", multiprocessing.current_process().name
	fib(38)#time.sleep(5)
	#print "End", multiprocessing.current_process().name

def test1(phand):
	print phand.addTask(0, fib, tuple([8]), "TestFib")[0].get()

	deps = []
	for i in range(10):
		res = phand.addTask(0, fib, tuple([38]), "0-DepTest-"+str(i))
		deps.append(res[1])

	print phand.addTask(0, fib, tuple([38]), "DepTest", dependencies=deps)[0].get()




ph = ProcessHandler()
#ph.runTaskHidden(0, printer, fib, args=tuple([38]), name=str(0)+"-FromLoop-"+str(0))
#ph.runTaskHidden(0, printer, fib, args=tuple([38]), name=str(0)+"-FromLoop-"+str(1))

for prio in range(4):
	for i in range(10):
		ph.runTaskHidden(prio, printer, fib, args=tuple([38]), name=str(prio)+"-FromLoop-"+str(i))

#time.sleep(100)
print "I'm out"
while True:
	time.sleep(1)
	ph.update()
"""
#phand.addTask(3, test1, tuple([phand]), "Test1orso")

#for i in range(100):
#	time.sleep(1)
#	print "Bla-", i
#	phand.join()
#	x = True
#	while x:
#		x = phand.startProcess()


for i in range(10):
	phand.addTask(0, fib, tuple([38]), "0-FromLoop-"+str(i))
for i in range(10):
	phand.addTask(1, fib, tuple([38]), "1-FromLoop-"+str(i))
for i in range(10):
	phand.addTask(2, fib, tuple([38]), "2-FromLoop-"+str(i))
for i in range(10):
	phand.addTask(3, fib, tuple([38]), "3-FromLoop-"+str(i))


for i in range(100):
	time.sleep(1)
	print "Bla-", i
	phand.join()
	x = True
	while x:
		x = phand.startProcess()

#p = multiprocessing.Process(target=worker)
#p.start()
#os.kill(p.pid, signal.SIGSTOP)
#time.sleep(1)
#print "Bla"
#os.kill(p.pid, signal.SIGCONT)
#time.sleep(1)
#print "Bla"
#p.join()
#"""
