
for SIZE in 64 256 1024 2048 4096
do
	echo "Testing with size $SIZE"

	iperf3 -c fe80::9e69:b4ff:fe60:b86d -B fe80::9e69:b4ff:fe60:bacd%enp2s0f1 -p 5201 -t 100 -l $SIZE -i 10 --repeating-payload >> result_client1 & 
	iperf3 -c fe80::9e69:b4ff:fe60:b86d -B fe80::9e69:b4ff:fe60:bacd%enp2s0f1 -p 5202 -t 100 -l $SIZE -i 10 --repeating-payload >> result_client2 & 
 	iperf3 -c fe80::9e69:b4ff:fe60:b86d -B fe80::9e69:b4ff:fe60:bacd%enp2s0f1 -p 5203 -t 100 -l $SIZE -i 10 --repeating-payload >> result_client3 &
 	iperf3 -c fe80::9e69:b4ff:fe60:b86d -B fe80::9e69:b4ff:fe60:bacd%enp2s0f1 -p 5204 -t 100 -l $SIZE -i 10 --repeating-payload >> result_client4 &
	wait

done

