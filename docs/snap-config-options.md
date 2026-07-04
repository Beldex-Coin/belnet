
# mnapp config options

## required

### ifname
network interface name 
### ifaddr
ip range of network interface

## optional

### keyfile
the private key to persist address with.
if not specified the address will be ephemeral.
### reachable
bool value that sets if we publish our mnapp to the dht
`true`: we are reachable via dht
`false`: we are not reachable via dht
### hops
number of hops in a path, min is `1`, max is `8`, defaults to `4`

setting this to `3` gives a "fast mode": one less relay traversal in each
direction (typically a 25-40% RTT reduction and one less queueing/loss
point) at a reduced-anonymity cost. the effective value is logged at
startup as `configured with numHops=`.
### paths
number of paths to maintain at any given time, defaults to `6`.
### blacklist-mnode
adds a `.mnode` to path build blacklist
### exit-node
specify a `.mnode` or `.bdx` address to use as an exit broker
### local-dns
address to bind local dns resoler to, defaults to `127.3.2.1:53`
if port is omitted it uses port `53`
### upstream-dns
address to forward non belnet related queries to. if not set belnet dns will reply with srvfail.
### mapaddr
perma map `.bdx` address to an ip owned by the mnapp
to map `whatever.bdx` to `10.0.10.10` it can be specified via:
```
mapaddr=whatever.bdx:10.0.10.10
```

## compile time optional features

### on-up
path to shell script to call when our interface is up
### on-down
path to shell script to call when our interface is down
### on-ready
path to shell script to call when mnapp is first ready
