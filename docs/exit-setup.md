
to configure belnet to be an exit add into `belnet.ini`:

    [router]
    min-connections=8
    max-connections=16

    [network]
    exit=true
    keyfile=/var/lib/belnet/exit.private
    reachable=1
    ifaddr=10.0.0.1/16
    hops=2
    paths=8


post setup for exit (as root) given `eth0` is used to get to the internet:

    # echo 1 > /proc/sys/net/ipv4/ip_forward
    # iptables -t nat -A POSTROUTING -s 10.0.0.0/16 -o eth0 -j MASQUERADE
