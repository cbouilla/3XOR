COMPILING
=========

The M4RI library must be available. "make" should compile everything.


CHECK THAT THE CODE IS WORKING
==============================

### generate "good" hashes

```
mkdir data
tests/doctor_hashes data/A data/B data/C 128000
```

### prepare indexes (quadratic algorithm only)

```
tooling/bucketize --advise data/C
```

Read the proposed parameters and use them below.

```
tooling/bucketize --prefix-width=9 data/A data/B data/C
```

### Run the code in standalone mode (speed test)

```
client_server/quadratic --prefix-width=9 data/A data/B data/C
```

or

```
client_server/gjoux data/A data/B data/C
```

The last one can be run in parallel (just choose the number of workers wisely):

```
OMP_PLACES=sockets client_server/gjoux --workers=2 --task-width=4 data/A data/B data/C
```

you should get the solutions voluntarily injected by `doctor_hashes`.

### run the code in client-server mode

NOTE: with the quadratic algorithm, the number of tasks (--N parameter of the server) is 2**(2*task_width).

```
client_server/server --N 256 --journal-file=demo.log
client_server/quad_client --server-address=tcp://localhost:5555 --prefix-width=9 --task-width=4 data/A data/B data/C
```

when the client has terminated, kill the server (CTRL+C). The "demo.log" file contains the solutions.


FINDING A 3XOR FROM BITCOIN MINER SHARES FILES
==============================================

This is a project to find large 3XORs using bitcoin miner(s). Using a [custom stratum server](https://github.com/cbouilla/3sum-pool), bitcoin miners can be used to find many preimages yielding hashes starting with 32+ zero bits.

### Get share files

We accumulated some of these in Amazon S3 in what we call "share files". Get some of them:

```
mkdir shares
cd shares
wget https://s3.amazonaws.com/3sum-pool/blocks6.bin
```

### Concatenate / split the share files

```
tooling/split_shares --npart=16 --target=shares/part [all share files]
```

Sorry, this program is sequential.

### Deduplicate the shares

```
tooling/check_shares --purge shares/part.*
```

### Generate the hashes

```
mkdir hashes
tooling/parse_shares --hash-directory=hashes shares/part.*
```

NOTE: this one is multi-threaded, yet still long.

### concatenate the hashes

```
cat hashes/*.foo > hashes/FOO
cat hashes/*.bar > hashes/BAR
cat hashes/*.foobar > hashes/FOOBAR
```

### sort the hashes (only needed for the quadratic algorithm)

```
tooling/sort_hashes hashes/FOO
tooling/sort_hashes hashes/BAR
tooling/sort_hashes hashes/FOOBAR
```

This is sequential, but quick. Choose the largest file and make it the third one (here, it is BAR)

### Compute the indexes (only needed for the quadratic algorithm)

```
tooling/bucketize --advise hashes/BAR
```

In our case, this suggests a bucket size of 20.

```
tooling/bucketize --prefix-width=20 hashes/FOO hashes/BAR hashes/FOOBAR
```

### Standalone run (speed test)

```
client_server/quadratic --prefix-width=19 data/SHARES.bin.foo data/SHARES.bin.foobar data/SHARES.bin.bar
```

or

```
client_server/gjoux data/SHARES.bin.foo data/SHARES.bin.foobar data/SHARES.bin.bar
```

### Client-server parallel run

Start server:

```
client_server/match_server --prefix-width=20
```

Start many clients (adjust the address of the server):

```
client_server/match_client --prefix-width=20 --server-address="tcp://pool.fil.cool:5555" data/FOO data/FOOBAR data/BAR
```

### Get the result

Once the computation is over (or during the computation), the result can be observed. The "permutation" describes the order of the 3 files (cf. `tooling/match_solutions.c`).

```
tooling/match_solutions --permutation=xxx shares/...
```