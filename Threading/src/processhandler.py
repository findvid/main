import multiprocessing
import threading
import Queue
import signal, os
import time

"""
Runs a callable with given arguments and put the results on a queue.

@param queue	The queue to put the results on
@param target	The callable to run
@param args 	Arguments for the callable
@param kwargs	Keyword arguments for the callable
"""
def resultPacker(queue, target, args=(), kwargs={}):
	result = target(*args, **kwargs)
	queue.put(result)

class ProcessHandler:
	maxProcesses = 7
	maxPrioritys = 4
	debug = False
	lock = threading.Lock()
	waitingProcesses = None
	pausedProcesses = None
	activeProcesses = None

	def __init__(self, maxProcesses=7, maxPrioritys=4, debug=False):
		self.maxProcesses = maxProcesses
		self.maxPrioritys = maxPrioritys
		self.debug = debug
		
		self.waitingProcesses = [[] for i in range(self.maxPrioritys)]
		self.pausedProcesses = [[] for i in range(self.maxPrioritys)]
		self.activeProcesses = [[] for i in range(self.maxPrioritys)]

	"""
	Counts the amount of currently running processes

	@return	the amount of running processes
	"""
	def activeCount(self):
		count = 0
		for pros in self.activeProcesses:
			count += len(pros)
		return count

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
				if self.debug:
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
			neededPriority = self.maxPrioritys + 1

		for (priority,(paused,active)) in enumerate(zip(self.pausedProcesses, self.activeProcesses)):
			# Not important enough?
			if neededPriority <= priority:
				return False
			# Only stop a process if there is still one of the same priority running
			if len(active) > 1:
				toPause = active.pop(0)
				try:
					os.kill(toPause.pid, signal.SIGSTOP)
					paused.append(toPause)
					if self.debug:
						print "Pause:", toPause.name
				except OSError, e:
					print "Tried to pause process but it's already gone?", toPause.name
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
					try:
						os.kill(toStart.pid, signal.SIGCONT)
						active.append(toStart)
						if self.debug:
							print "Continue:", toStart.name
					except OSError, e:
						print "Can't kill process. Process '%s' is not running." % toStart.name
				# Try to start new processes
				while len(waiting) > 0:
					if not self.freeProcess(priority):
						break
					onComplete, ocArgs, ocKwargs, target, args, kwargs, name = waiting.pop(0)
					results = multiprocessing.Queue()
					process = multiprocessing.Process(target=resultPacker, args=(results, target, args, kwargs), name=name)
					thread = threading.Thread(target=self.runProcess, args=(results, process, onComplete, ocArgs, ocKwargs))
					thread.start()
					active.append(process)
					if self.debug:
						print "Start:", process.name
		#	pro = self.activeProcesses
		#	print len(pro[0]), len(pro[1]), len(pro[2]), len(pro[3])
		#	tmp = "["
		#	for (waiting,paused,active) in zip(self.waitingProcesses, self.pausedProcesses, self.activeProcesses):
		#		tmp = tmp + "["
		#		for pro in active:
		#			tmp = tmp + str(pro) + ","
		#		tmp = tmp + "]"
		#	tmp = tmp + "]"
		#	print tmp
		finally:
			self.lock.release()

	"""
	Waits for the queue and then let's a callable deal with the result

	@param queue		Queue wich will get one result put on
	@param onComplete	Callable that can work with this result
	"""
	def runProcess(self, queue, process, onComplete=None, onCompleteArgs=(), onCompleteKwargs={}):
		res = False
		poll = True
		process.start()
		while poll:
			try:
				res = queue.get(timeout = 1)
			except Queue.Empty, e:
				pass
			if res != False:
				process.join(timeout = 0)
			if process.exitcode != None:
				poll = False
				if process.exitcode != 0:
					res = False
		self.update()
		if onComplete != None:
			onComplete(res, *onCompleteArgs, **onCompleteKwargs)

	"""
	Run a task in its own process and executes another callable on the result
	in an own thread

	@param priority		Priority of the task
	@param onComplete	Callable that will deal with the result of target
	@param onCompleteArgs	Arguments for target
	@param onCompleteKwargs	Keyword arguments for target
	@param target		Callable that represents the task
	@param args		Arguments for target
	@param kwargs		Keyword arguments for target
	@param name		Name of the process
	"""
	def runTask(self, target, args=(), kwargs={}, priority=0, onComplete=None, onCompleteArgs=(), onCompleteKwargs={}, name=None):
		if priority >= self.maxPrioritys or priority < 0:
			raise "Fuckedup Priority"
		self.lock.acquire()
		try:
			self.waitingProcesses[priority].append((onComplete, onCompleteArgs, onCompleteKwargs, target, args, kwargs, name))
		finally:
			self.lock.release()

		self.update()

	"""
	Run a task in it's own process and returns the result once it's done

	@param priority		Priority of the task
	@param target		Callable that represents the task
	@param args		Arguments for target
	@param kwargs		Keyword arguments for target
	@param name		Name of the process

	@return			
	"""
	def runTaskWait(self, target, args=(), kwargs={}, priority=0, name=None):
		res = Queue.Queue()
		self.runTask(target=target, args=args, kwargs=kwargs, priority=priority, onComplete=res.put, onCompleteArgs=(), name=name)
		return res.get()

	"""
	Shoots a process

	@param name		The name of the process that should get stopped
	@param process		The process that should get stopped
	"""
	def stopProcess(self, name=None, process=None):
		if process == None:
			if name == None:
				return
			self.lock.acquire()
			try:
				for (paused, waiting, active) in zip(self.pausedProcesses, self.waitingProcesses, self.activeProcesses):
					for pro in paused:
						if pro.name == name:
							process = pro
					for pro in waiting:
						if pro.name == name:
							process = pro
					for pro in active:
						if pro.name == name:
							process = pro
			finally:
				self.lock.release()
		try:
			os.kill(process.pid, signal.SIGKILL)
			#process.join()
			#self.update()
		except OSError, e:
			print "Tried to shoot process but it's already gone?", process.pid

	"""
	Waits till all processes of a priority are finished and then returns

	@param priority		The priority to wait for
	@param waitTime		Amount of seconds between checking
	"""
	def waitForPriority(self, priority, waitTime=1):
		while True:
			count = 0
			self.lock.acquire()
			try:
				if (len(self.pausedProcesses[priority]) + len(self.waitingProcesses[priority]) + len(self.activeProcesses[priority])) == 0:
					return
			finally:
				self.lock.release()
			time.sleep(waitTime)

def fib(n):
	if n <= 2:
		return n
	return fib(n-1) + fib(n-2)

def printer(toPrint):
	y = toPrint
	print "\tPrinter:", toPrint

if __name__ == '__main__':

	ph = ProcessHandler(8)
	#ph.runTask(0, printer, fib, args=tuple([38]), name=str(0)+"-FromLoop-"+str(0))
	#ph.runTask(0, printer, fib, args=tuple([38]), name=str(0)+"-FromLoop-"+str(1))

	print ph.runTaskWait(priority=0, target=fib, args=tuple([38]))

	for prio in range(4):
		for i in range(10):
			ph.runTask(priority=prio, onComplete=printer, target=fib, args=tuple([34]), name=str(prio)+"-"+str(i))

	ph.waitForPriority(2, 1)
	print "Done!"
	#time.sleep(100)
	#print "I'm out"
	#while True:
	#	time.sleep(1)
	#	ph.update()


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

	#p = multiprocessing.Process(target=onComplete)
	#p.start()
	#os.kill(p.pid, signal.SIGSTOP)
	#time.sleep(1)
	#print "Bla"
	#os.kill(p.pid, signal.SIGCONT)
	#time.sleep(1)
	#print "Bla"
	#p.join()
	#"""
