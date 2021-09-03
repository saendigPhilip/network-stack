##Testing latency and throughput

The throughput measurement may slow down the server because it takes one thread to summarize the results.
That's why the latency and throughput tests need to be run separately (even though they are exactly the same).

###Testing latency
* Compile the server and the client with ```-DMEASURE_LATENCY=on -DMEASURE_THROUGHPUT=off``` 
  The latency option makes sure that for each request, start and end time are taken 
  (NEVER use a SCONE client compiled with this option!).
  
* On the client, modify the file client_latency_test.py according to your needs 
  (e.g. the variables ```kThreads, test_sizes```). 
  If you change one of these two variables, for SCONE, the file scone_tests/server_test.sh needs to be modified accordingly.
  Before each test, it also makes sense to give the CSV file a different name for not losing the overview.
  Also check the IP address in the SCONE script.

* On the server side run ```sudo python3 test_server.py <server ip>```, then run 
  ```sudo python3 client_latency_test.py <client ip> <server ip>```.
  
* For SCONE servers, run the scone_tests/server_test.sh script on the server side. 
  At the client side, use the same python script as usual, just pass ```-scone``` to it. 

* While running, each test should trigger some output by eRPC and on the client, a test summmary.
  The client writes the test results to a CSV file specified in the python script. 
  For the latency, the value in the field ```Put Latency (us)``` is relevant.
  

###Testing throughput
* Compile client and server with ```-DMEASURE_LATENCY=off -DMEASURE_THROUGHPUT=on```.
  This makes sure that there is no overhead caused by latency measurements. 
  Furthermore, it makes the server measure the throughput which is needed for comparison with iperf.
  
* As before, you can modify the same script as before (client_latency_test.py) according to your needs. 
  The script scone_tests/server_test.sh for SCONE may need to be modified as well in this case.

* On the server side run ```sudo python3 test_server.py <server ip>```, then run
  ```sudo python3 client_latency_test.py <client ip> <server ip>```.

* To be 100% correct (in terms of comparison to iperf),
  the average throughput at the server side after each test needs to be taken as a result
  (by copy+paste)
  

