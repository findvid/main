import multiprocessing
import Queue
import signal, os
import time

MAX_PRIORITYS = 4

def resultPacker(queue, target, args=()):
	result = target(*args)
	queue.put(result)

class ProcessHandler:
	maxProcesses = 7
	waitingProcesses = [[] for i in range(MAX_PRIORITYS)]
	condidionalProcesses = [[] for i in range(MAX_PRIORITYS)]
	pausedProcesses = [[] for i in range(MAX_PRIORITYS)]
	activeProcesses = [[] for i in range(MAX_PRIORITYS)]

	def __init__(self, maxProcesses=7):
		self.maxProcesses = maxProcesses

	def checkDependencies(self, dependencies):
		for p in dependencies:
			if p.exitcode == None:
				return False
		return True

	def addTask(self, priority, target, args=(), name=None, results=multiprocessing.Queue(), dependencies=None):
		p = multiprocessing.Process(target=resultPacker, args=(results, target, args), name=name)
		self.addProcess(p, priority, dependencies)
		return (results,p)

	def addProcess(self, process, priority, dependencies=None):
		if priority >= MAX_PRIORITYS:
			raise "Fuckedup Priority"
		if dependencies != None:
			if not (self.checkDependencies):
				self.condidionalProcesses[priority].append((dependencies,process))
				return False
		if self.activeCount() < self.maxProcesses:
			process.start()
			self.activeProcesses[priority].append(process)
			print "Start:", process.name
			return True
		if self.freeProcess(priority):
			process.start()
			self.activeProcesses[priority].append(process)
			print "Start:", process.name
			return True
		self.waitingProcesses[priority].append(process)
		return False

	def activeCount(self):
		count = 0
		for pros in self.activeProcesses:
			count += len(pros)
		return count

	def freeProcess(self, priority):
		if self.join() > 0:
			return True
		for (prio,(paused,active)) in enumerate(zip(self.pausedProcesses, self.activeProcesses)):
			if prio >= priority:
				return False
			if len(active) > 1:
				toPause = active.pop(0)
				try:
					os.kill(toPause.pid, signal.SIGSTOP)
				except OSError, e:
					# TODO This is just a quick fix so for. Make this error handleing better
					# And make sure process.join happens if that's needed!
					#toPause.join()
					print "Tried to pause process but it's already gone?", toPause.pid
					return True
				paused.append(toPause)
				print "Pause:", toPause.name
				return True
		return False

	def startProcess(self):
		if self.activeCount() >= self.maxProcesses:
			return False
		for (waiting,condidional,paused,active) in reversed(zip(self.waitingProcesses, self.condidionalProcesses, self.pausedProcesses, self.activeProcesses)):
			if len(paused) > 0:
				toStart = paused.pop(0)
				os.kill(toStart.pid, signal.SIGCONT)
				active.append(toStart)
				print "Continue:", toStart.name
				return True
			if len(condidional) > 0:
				toStart = None
				deps = None
				for (dependencies,process) in condidional:
					if checkDependencies(dependencies):
						toStart = process
						deps = dependencies
						break
				if toStart != None:
					dependencies.remove((deps,toStart))
					toStart.start()
					active.append(toStart)
					print "Start (dependend):", toStart.name
					return True
			if len(waiting) > 0:
				toStart = waiting.pop(0)
				toStart.start()
				active.append(toStart)
				print "Start:", toStart.name
				return True
		return False

	def join(self):
		count = 0
		for active in self.activeProcesses:
			removed = []
			for p in active:
				if p.exitcode != None:
					count += 1
					#p.join()
					removed.append(p)
			for p in removed:
				active.remove(p)
				print "Removed:", p.name
		return count

def fib(n):
	if n <= 2:
		return n
	return fib(n-1) + fib(n-2)

def worker():
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

phand = ProcessHandler()

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
