for i in {1..100}
do
    ./client -a 192.168.0.20 -b 10000 -c 100 &
    sleep 0.3
done
wait
