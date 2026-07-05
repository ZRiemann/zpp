# NNG protocol examples

The protocol examples use `doc/server.json`. For paired examples, start the
listening side before the dialing side. For PUB/SUB, start the publisher, the
device, and then the subscriber in separate terminals.

After building the selected configuration, list or start a registered example
with `zbuild.py`:

```sh
./zbuild.py runs
./zbuild.py run --BUILD_TYPE=Debug publisher_example
./zbuild.py run --BUILD_TYPE=Debug pub_sub_device_example
./zbuild.py run --BUILD_TYPE=Debug subscriber_example
./zbuild.py run --BUILD_TYPE=Debug replier_example
./zbuild.py run --BUILD_TYPE=Debug requester_example
```

The `run` command uses an existing build tree and does not build the executable.
Examples can also be started directly:

```sh
./build/Debug/examples/nng/publisher_example doc/server.json publisher_example
./build/Debug/examples/nng/pub_sub_device_example doc/server.json pub_sub_device_example
./build/Debug/examples/nng/subscriber_example doc/server.json subscriber_example

./build/Debug/examples/nng/pusher_example doc/server.json pusher_example
./build/Debug/examples/nng/puller_example doc/server.json puller_example

./build/Debug/examples/nng/replier_example doc/server.json replier_example
./build/Debug/examples/nng/requester_example doc/server.json requester_example

./build/Debug/examples/nng/pair1_listen_example doc/server.json pair1_listen_example
./build/Debug/examples/nng/pair1_dial_example doc/server.json pair1_dial_example
```

PUB sends one counter per second into the raw SUB-to-PUB device. The device
dials the configured ingress and listens on every configured egress endpoint.
SUB processes forwarded messages through three managed receive AIOs on one
default context so every message is delivered once.
PUSH sends one counter per second. REQ/REP use three managed contexts: the
requester keeps three requests in flight and correlates out-of-order replies
with stable hints, while the replier processes up to three transactions
concurrently. PAIR1 uses a reusable AIO handle and limits exchanges to one round
per second. Stop each process with `Ctrl-C`.
