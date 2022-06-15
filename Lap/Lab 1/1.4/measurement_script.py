import sys
import os
import time
import subprocess
import csv
# How to run
# python measurements_script.py (multi-client executable) (target hostname) (port) (number of clients ) (number of clients) ... (messages per client) 


if (len(sys.argv) < 6) :  
	print "Not enough input arguements"
	print "Usage:"
	print "python measurements_script.py (multi-client executable) (target hostname) (port) (number of clients ) (number of clients) ... (messages per client)"
	sys.exit()

exec_name = sys.argv[1]
host  = sys.argv[2]
port = sys.argv[3]
num_messages = sys.argv[-1]
repeat  = 5
num_clients  = sys.argv[4:-1]
print num_clients
avg_elapsed_time = []
avg_conn_time = []
#avg_rtt = []
for i in num_clients:
	temp = 0
	conn_time = 0
	#rtt = 0
	for j in range(1,repeat):
		start_time = time.time()
		proc = subprocess.Popen(["./"+exec_name, host, port, i, num_messages],\
		stderr = subprocess.STDOUT, stdout = subprocess.PIPE)
		proc.wait()
		temp += 1000.0 *(time.time() - start_time)
		out = proc.communicate()[0]
		print out
		g = out.split("average time:")
		conn_time += float(g[1].split()[0])
		#rtt += float(g[2].split()[0])
		time.sleep(1)
	avg_elapsed_time.append(temp / float(repeat))
	avg_conn_time.append(conn_time / float(repeat))
	#avg_rtt.append(rtt / float(repeat))
print "-------- results -------"
print "Average total time (in msec)"
for i in avg_elapsed_time:
	print ('%f' % i).replace('.',',')
print "Average connection time (in msec)"
for i in avg_conn_time:
	print ('%f' % i).replace('.',',')
#print "Average round trip time"
#for i in avg_rtt:
#	print ('%f' % i).replace('.',',')

f1 = open('output_avg_conn_time.csv','w')
for i in avg_conn_time:
	f1.write('%f,\n' % i)

f2 = open('output_avg_total_time.csv','w')
for i in avg_elapsed_time:
	f2.write('%f,\n' % i)
#with open('output_avg_conn_time.csv','w') as csvfile:
#	wr = csv.writer(csvfile)
#	wr.writerow(avg_conn_time)
#	#wr.writerow(avg_conn_time)

