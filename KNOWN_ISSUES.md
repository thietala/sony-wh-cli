# Known issues

## Known flaky tests

Catch2 test cases that use a scripted `FakeTransport` and wait on a real
condition-variable deadline (not the injectable fake clock) for a
simulated device ACK have intermittently failed with "Timeout waiting for
ACK" when run as part of the *full* suite, while passing reliably (4/4)
in isolation. Seen so far:

- `ProtocolV1: sets noise control with V1 packet layout`
- `ProtocolV2: handles equalizer queries and settings`

Different test files, different fake transports — the common thread is a
tight real-time ACK-wait margin that full-suite contention can blow past.
Not yet root-caused. Re-run a failure in isolation before assuming a real
regression; if it still fails alone, that's a real bug.

## Testing Windows/macOS

No Windows or macOS hardware is available for this project, so CI on
those platforms is validated on GitHub's own hosted runners rather than
locally.

